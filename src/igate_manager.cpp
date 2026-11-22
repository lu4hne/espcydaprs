#include "igate_manager.h"
#include "afsk_modem.h"
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>

IgateManager Igate;

IgateManager::IgateManager() {
    memset(_rxLineBuf, 0, sizeof(_rxLineBuf));
}

uint16_t IgateManager::calcPasscode(const char* callsign) {
    if (!callsign || callsign[0] == '\0') return 0;

    char clean[12];
    int len = 0;
    for (int i = 0; callsign[i] != '\0' && callsign[i] != '-' && callsign[i] != ' ' && len < 10; i++) {
        clean[len++] = (char)toupper((unsigned char)callsign[i]);
    }
    clean[len] = '\0';

    uint16_t hash = 0x73e2;
    int i = 0;
    while (i < len) {
        hash ^= ((uint16_t)((uint8_t)clean[i])) << 8;
        if (i + 1 < len) {
            hash ^= (uint16_t)((uint8_t)clean[i + 1]);
        }
        i += 2;
    }
    return hash & 0x7fff;
}

uint16_t IgateManager::getEffectivePasscode() const {
    const AprsConfig& cfg = Aprs.config();
    if (cfg.aprsIsPasscode >= 0 && cfg.aprsIsPasscode <= 32767) {
        return (uint16_t)cfg.aprsIsPasscode;
    }
    return calcPasscode(cfg.callsign);
}

void IgateManager::begin() {
    const AprsConfig& cfg = Aprs.config();
    bool needWifi = (cfg.stationMode == STATION_MODE_FIXED_IGATE) || cfg.igateEnabled;

    if (needWifi && strlen(cfg.wifiSsid) > 0) {
        // Diferir el inicio de WiFi ~1.5s tras el arranque para que la pantalla se dibuje primero
        _state = IGATE_WIFI_WAIT;
        _lastWifiAttemptMs = millis() - 18500UL;
    } else {
        _state = IGATE_DISABLED;
    }
}

void IgateManager::connectWifi() {
    const AprsConfig& cfg = Aprs.config();
    if (strlen(cfg.wifiSsid) == 0) {
        _state = IGATE_DISABLED;
        return;
    }

    // Desactivar brownout temporalmente durante la calibración RF PHY de WiFi.
    // NO pausar el módem AFSK: usa ADC1_CH7 (GPIO 35) en Core 1, que NO tiene conflicto con WiFi.
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    if (WiFi.getMode() != WIFI_AP_STA) {
        WiFi.mode(WIFI_STA);
    }
    WiFi.setTxPower(WIFI_POWER_15dBm); // Reduce pico de corriente en el regulador LDO 3.3V de la CYD
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(cfg.wifiSsid, cfg.wifiPass);

    _lastWifiAttemptMs = millis();
    _state = IGATE_WIFI_WAIT;
    Serial.printf("[IGATE] Conectando a WiFi '%s'...\n", cfg.wifiSsid);
}

void IgateManager::disconnectWifi() {
    if (_client.connected()) {
        _client.stop();
    }
    if (WiFi.getMode() == WIFI_STA) {
        WiFi.disconnect(true);
    }
    _state = IGATE_DISABLED;
}

bool IgateManager::isWifiConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String IgateManager::getLocalIpString() const {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    if ((WiFi.getMode() & WIFI_AP) != 0) {
        return WiFi.softAPIP().toString();
    }
    return String("Sin IP");
}

const char* IgateManager::getStateText() const {
    switch (_state) {
        case IGATE_DISABLED:       return "APAGADO";
        case IGATE_WIFI_WAIT:      return "CONECTANDO WIFI";
        case IGATE_TCP_CONNECTING: return "CONECTANDO APRS-IS";
        case IGATE_CONNECTED:      return "CONECTADO (LOGIN)";
        case IGATE_VERIFIED:       return "ONLINE VERIFICADO";
        case IGATE_ERROR:          return "ERROR / REINTENTO";
        default:                   return "DESCONOCIDO";
    }
}

uint32_t IgateManager::getLastInjectedAgeSec() const {
    if (_lastInjectedMs == 0) return 0;
    return (millis() - _lastInjectedMs) / 1000UL;
}

void IgateManager::reconnectNow() {
    if (_client.connected()) {
        _client.stop();
    }
    _rxLineLen = 0;
    const AprsConfig& cfg = Aprs.config();
    bool active = (cfg.stationMode == STATION_MODE_FIXED_IGATE) || cfg.igateEnabled;
    if (!active) {
        _state = IGATE_DISABLED;
        return;
    }
    if (!isWifiConnected()) {
        connectWifi();
    } else {
        _state = IGATE_WIFI_WAIT;
        _lastTcpAttemptMs = 0; // Forzar reconexión TCP inmediata en update()
    }
}

void IgateManager::update() {
    const AprsConfig& cfg = Aprs.config();
    bool active = (cfg.stationMode == STATION_MODE_FIXED_IGATE) || cfg.igateEnabled;

    if (!active) {
        if (_client.connected()) {
            _client.stop();
        }
        _state = IGATE_DISABLED;
        return;
    }

    uint32_t now = millis();

    // 1. Verificar estado WiFi
    if (!isWifiConnected()) {
        if (_client.connected()) {
            _client.stop();
        }
        _state = IGATE_WIFI_WAIT;
        if (strlen(cfg.wifiSsid) > 0 && (now - _lastWifiAttemptMs >= 60000UL)) {
            connectWifi();
        }
        return;
    }

    // 2. Si WiFi está conectado pero no hay socket TCP a APRS-IS
    if (!_client.connected()) {
        if (_state == IGATE_CONNECTED || _state == IGATE_VERIFIED) {
            Serial.println(F("[IGATE] Conexion TCP APRS-IS cerrada. Reconectando..."));
            _state = IGATE_ERROR;
        }

        if (_lastTcpAttemptMs == 0 || (now - _lastTcpAttemptMs >= 15000UL)) {
            _lastTcpAttemptMs = now;
            _state = IGATE_TCP_CONNECTING;

            const char* server = (strlen(cfg.aprsIsServer) > 0 && strcmp(cfg.aprsIsServer, "samerica.aprs2.net") != 0)
                                 ? cfg.aprsIsServer
                                 : "rotate.aprs2.net";
            uint16_t port = (cfg.aprsIsPort > 0) ? cfg.aprsIsPort : 14580;

            Serial.printf("[IGATE] Conectando TCP a %s:%u...\n", server, port);
            _client.setTimeout(3000);
            bool tcpOk = _client.connect(server, port);
            if (!tcpOk && strcmp(server, "rotate.aprs2.net") != 0) {
                Serial.println(F("[IGATE] Probando servidor global rotate.aprs2.net:14580..."));
                tcpOk = _client.connect("rotate.aprs2.net", port);
            }
            if (tcpOk) {
                _state = IGATE_CONNECTED;
                _lastServerActivityMs = now;
                _lastKeepAliveTxMs = now;
                _rxLineLen = 0;

                char myFullCall[16];
                if (cfg.ssid > 0) {
                    snprintf(myFullCall, sizeof(myFullCall), "%s-%u", cfg.callsign, cfg.ssid);
                } else {
                    snprintf(myFullCall, sizeof(myFullCall), "%s", cfg.callsign);
                }

                uint16_t pass = getEffectivePasscode();
                char loginLine[180];
                snprintf(loginLine, sizeof(loginLine),
                         "user %s pass %u vers ESP32-CYD-APRS %s filter r/%.2f/%.2f/100 g/%s* b/%s*\r\n",
                         myFullCall, pass, FIRMWARE_VERSION,
                         Aprs.getCurrentLat(), Aprs.getCurrentLon(),
                         cfg.callsign, cfg.callsign);

                _client.print(loginLine);
                Serial.printf("[IGATE] Login enviado: %s", loginLine);
            } else {
                Serial.println(F("[IGATE] Error al conectar TCP con servidor APRS-IS."));
                _state = IGATE_ERROR;
            }
        }
        return;
    }

    // 3. Leer líneas entrantes desde APRS-IS
    while (_client.available() > 0) {
        char c = (char)_client.read();
        _lastServerActivityMs = now;

        if (c == '\n') {
            _rxLineBuf[_rxLineLen] = '\0';
            if (_rxLineLen > 0 && _rxLineBuf[_rxLineLen - 1] == '\r') {
                _rxLineBuf[_rxLineLen - 1] = '\0';
            }
            if (strlen(_rxLineBuf) > 0) {
                processServerLine(_rxLineBuf);
            }
            _rxLineLen = 0;
        } else if (c != '\r') {
            if (_rxLineLen < sizeof(_rxLineBuf) - 1) {
                _rxLineBuf[_rxLineLen++] = c;
            }
        }
    }

    // 4. Keep-alive y detección de timeout de servidor (180s sin actividad)
    if (now - _lastServerActivityMs >= 180000UL) {
        Serial.println(F("[IGATE] Timeout de actividad APRS-IS (180s). Reiniciando socket..."));
        _client.stop();
        _state = IGATE_ERROR;
        return;
    }

    if (now - _lastKeepAliveTxMs >= 60000UL) {
        _lastKeepAliveTxMs = now;
        _client.print("# ESP32-CYD-APRS keepalive\r\n");
    }
}

void IgateManager::processServerLine(const char* line) {
    if (!line || line[0] == '\0') return;

    // Comentarios o respuestas de control del servidor APRS-IS ('#')
    if (line[0] == '#') {
        if (strstr(line, "logresp") != nullptr) {
            if (strstr(line, " verified") != nullptr) {
                _state = IGATE_VERIFIED;
                Serial.printf("[IGATE] Autenticacion APRS-IS VERIFICADA: %s\n", line);
            } else {
                Serial.printf("[IGATE] Aviso Login APRS-IS (no verificado): %s\n", line);
            }
        }
        return;
    }

    // Trama TNC2 recibida desde APRS-IS (ej: mensajes/ACKs dirigidos a nuestra estación o estaciones locales)
    Serial.printf("[IGATE IS->RX] %s\n", line);
    if (Aprs.handleAprsIsIncomingLine(line)) {
        _isToRfCount++;
    }
}

bool IgateManager::shouldGateRfPacket(const DecodedPacket& pkt) const {
    if (strlen(pkt.srcCall) == 0 || strlen(pkt.infoText) == 0) return false;
    if (strcasecmp(pkt.srcCall, "N0CALL") == 0) return false;

    // No inyectar paquetes que ya provienen de Internet o que prohíben explícitamente IGate
    if (strstr(pkt.pathStr, "TCPIP") != nullptr) return false;
    if (strstr(pkt.pathStr, "TCPXX") != nullptr) return false;
    if (strstr(pkt.pathStr, "NOGATE") != nullptr) return false;
    if (strstr(pkt.pathStr, "RFONLY") != nullptr) return false;

    // No re-inyectar paquetes de terceros '}' o consultas '?'
    if (pkt.infoText[0] == '}' || pkt.infoText[0] == '?') return false;

    return true;
}

bool IgateManager::injectRfPacket(const DecodedPacket& pkt) {
    const AprsConfig& cfg = Aprs.config();
    bool active = (cfg.stationMode == STATION_MODE_FIXED_IGATE) || cfg.igateEnabled;
    if (!active || !_client.connected()) return false;

    if (!shouldGateRfPacket(pkt)) return false;

    char myFullCall[16];
    if (cfg.ssid > 0) {
        snprintf(myFullCall, sizeof(myFullCall), "%s-%u", cfg.callsign, cfg.ssid);
    } else {
        snprintf(myFullCall, sizeof(myFullCall), "%s", cfg.callsign);
    }

    // Construir línea TNC2 con constructo qAR (paquete recibido directamente por RF en este IGate)
    char cleanPath[48];
    strlcpy(cleanPath, pkt.pathStr, sizeof(cleanPath));

    char tnc2[260];
    if (strlen(cleanPath) > 0) {
        snprintf(tnc2, sizeof(tnc2), "%s>%s,%s,qAR,%s:%s\r\n",
                 pkt.srcCall, pkt.dstCall, cleanPath, myFullCall, pkt.infoText);
    } else {
        snprintf(tnc2, sizeof(tnc2), "%s>%s,qAR,%s:%s\r\n",
                 pkt.srcCall, pkt.dstCall, myFullCall, pkt.infoText);
    }

    size_t len = strlen(tnc2);
    if (_client.write((const uint8_t*)tnc2, len) == len) {
        _rfToIsCount++;
        strlcpy(_lastInjectedCall, pkt.srcCall, sizeof(_lastInjectedCall));
        _lastInjectedMs = millis();
        Serial.printf("[IGATE RF->IS #%lu] %s", (unsigned long)_rfToIsCount, tnc2);
        return true;
    }
    return false;
}

bool IgateManager::sendSelfBeaconToAprsIs(const char* aprsInfoPayload) {
    if (!aprsInfoPayload || !_client.connected()) return false;

    const AprsConfig& cfg = Aprs.config();
    char myFullCall[16];
    if (cfg.ssid > 0) {
        snprintf(myFullCall, sizeof(myFullCall), "%s-%u", cfg.callsign, cfg.ssid);
    } else {
        snprintf(myFullCall, sizeof(myFullCall), "%s", cfg.callsign);
    }

    char tnc2[240];
    snprintf(tnc2, sizeof(tnc2), "%s>%s,TCPIP*:%s\r\n",
             myFullCall, cfg.destCall, aprsInfoPayload);

    size_t len = strlen(tnc2);
    if (_client.write((const uint8_t*)tnc2, len) == len) {
        _selfBeaconCount++;
        Serial.printf("[IGATE BEACON->IS] %s", tnc2);
        return true;
    }
    return false;
}

bool IgateManager::sendRawLineToAprsIs(const char* tnc2Line) {
    if (!tnc2Line || !_client.connected()) return false;
    size_t len = strlen(tnc2Line);
    if (_client.write((const uint8_t*)tnc2Line, len) != len) return false;
    if (len == 0 || tnc2Line[len - 1] != '\n') {
        _client.print("\r\n");
    }
    return true;
}

