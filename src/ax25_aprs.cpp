#include "ax25_aprs.h"
#include "igate_manager.h"
#include <Preferences.h>
#include <TinyGPS++.h>
#include <math.h>

AprsEngine Aprs;
static TinyGPSPlus s_gps;

AprsEngine::AprsEngine() {
    memset(&_cfg, 0, sizeof(_cfg));
    memset(&_gpsState, 0, sizeof(_gpsState));
    memset(_packetLog, 0, sizeof(_packetLog));
    memset(_stations, 0, sizeof(_stations));
    memset(_messages, 0, sizeof(_messages));
}

void AprsEngine::begin() {
    loadConfig();
    Modem.configure(_cfg.txDelayMs, _cfg.txTailMs, _cfg.txVolume, _cfg.txAudioPin);

    // Iniciar UART2 para módulo GPS externo (RX=GPIO17, TX=GPIO16) solo si GPIO17 no se usa para Audio TX
    if (_cfg.txAudioPin != GPS_UART_RX_PIN) {
        Serial2.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_UART_RX_PIN, GPS_UART_TX_PIN);
    }
    _lastBeaconMs = millis();
}

void AprsEngine::loadConfig() {
    Preferences prefs;
    prefs.begin("aprs_cfg", true);

    // Si aún no hay modelo guardado en NVS, auto-detectar por MAC de la placa (o usar ST7789 en la de 1 USB)
    uint8_t defaultBoard = BOARD_MODEL_ST7789;
    uint64_t efuseMac = ESP.getEfuseMac();
    // La placa ILI9341 (MAC 80:F3:DA:BA:DA:84 -> byte bajo 0x80) arranca en ILI9341; la de 1 USB (68:09:47...) en ST7789
    if ((efuseMac & 0xFF) == 0x80) {
        defaultBoard = BOARD_MODEL_ILI9341;
    }

    _cfg.boardModel = prefs.getUChar("brdMod", defaultBoard);
    _cfg.displayInvert = prefs.getBool("dispInv", false);
    _cfg.txAudioPin = prefs.getUChar("txPin", (_cfg.boardModel == BOARD_MODEL_ST7789) ? 4 : 27);

    _cfg.stationMode = prefs.getUChar("stMode", STATION_MODE_TRACKER);
    strlcpy(_cfg.callsign, prefs.getString("call", "LU4HNE").c_str(), sizeof(_cfg.callsign));
    _cfg.ssid = prefs.getUChar("ssid", 7);
    strlcpy(_cfg.destCall, prefs.getString("dest", "APECYD").c_str(), sizeof(_cfg.destCall));
    strlcpy(_cfg.path1, prefs.getString("path1", "WIDE1").c_str(), sizeof(_cfg.path1));
    _cfg.path1Ssid = prefs.getUChar("p1ssid", 1);
    strlcpy(_cfg.path2, prefs.getString("path2", "WIDE2").c_str(), sizeof(_cfg.path2));
    _cfg.path2Ssid = prefs.getUChar("p2ssid", 1);
    _cfg.symbolTable = (char)prefs.getUChar("symT", '/');
    _cfg.symbolCode  = (char)prefs.getUChar("symC", '[');
    strlcpy(_cfg.comment, prefs.getString("comm", "ESP32 CYD APRS 1200bps").c_str(), sizeof(_cfg.comment));
    _cfg.fixedLat = prefs.getDouble("lat", -34.6037);
    _cfg.fixedLon = prefs.getDouble("lon", -58.3816);
    _cfg.fixedAltMeters = prefs.getShort("alt", 25);
    _cfg.posMode = prefs.getUChar("posMode", POS_MODE_FIXED);
    _cfg.autoBeacon = prefs.getBool("autoBcn", false);
    _cfg.beaconIntervalSec = prefs.getUShort("bcnInt", 300);
    _cfg.radioType = prefs.getUChar("radio", RADIO_BAOFENG);
    _cfg.txDelayMs = prefs.getUShort("txDelay", 450);
    _cfg.txTailMs  = prefs.getUShort("txTail", 60);
    _cfg.txVolume  = prefs.getUChar("txVol", 75);
    _cfg.autoAckMessages = prefs.getBool("autoAck", true);
    _cfg.digipeatWide1   = prefs.getBool("digiW1", false);

    // Parámetros WiFi e IGate APRS-IS
    strlcpy(_cfg.wifiSsid, prefs.getString("wSsid", "").c_str(), sizeof(_cfg.wifiSsid));
    strlcpy(_cfg.wifiPass, prefs.getString("wPass", "").c_str(), sizeof(_cfg.wifiPass));
    _cfg.igateEnabled = prefs.getBool("igEn", (_cfg.stationMode == STATION_MODE_FIXED_IGATE));
    strlcpy(_cfg.aprsIsServer, prefs.getString("isSrv", "samerica.aprs2.net").c_str(), sizeof(_cfg.aprsIsServer));
    _cfg.aprsIsPort = prefs.getUShort("isPort", 14580);
    _cfg.aprsIsPasscode = prefs.getInt("isPass", -1);
    _cfg.igateBeaconMode = prefs.getUChar("igBcn", IGATE_BCN_BOTH);
    _cfg.igateIsToRfMsg = prefs.getBool("igMsgRf", true);

    // Parámetros OTA
    strlcpy(_cfg.otaServerUrl, prefs.getString("otaUrl", "").c_str(), sizeof(_cfg.otaServerUrl));

    prefs.end();
}

void AprsEngine::saveConfig() {
    Preferences prefs;
    prefs.begin("aprs_cfg", false);

    prefs.putUChar("brdMod", _cfg.boardModel);
    prefs.putBool("dispInv", _cfg.displayInvert);
    prefs.putUChar("txPin", _cfg.txAudioPin);

    prefs.putUChar("stMode", _cfg.stationMode);
    prefs.putString("call", _cfg.callsign);
    prefs.putUChar("ssid", _cfg.ssid);
    prefs.putString("dest", _cfg.destCall);
    prefs.putString("path1", _cfg.path1);
    prefs.putUChar("p1ssid", _cfg.path1Ssid);
    prefs.putString("path2", _cfg.path2);
    prefs.putUChar("p2ssid", _cfg.path2Ssid);
    prefs.putUChar("symT", (uint8_t)_cfg.symbolTable);
    prefs.putUChar("symC", (uint8_t)_cfg.symbolCode);
    prefs.putString("comm", _cfg.comment);
    prefs.putDouble("lat", _cfg.fixedLat);
    prefs.putDouble("lon", _cfg.fixedLon);
    prefs.putShort("alt", _cfg.fixedAltMeters);
    prefs.putUChar("posMode", _cfg.posMode);
    prefs.putBool("autoBcn", _cfg.autoBeacon);
    prefs.putUShort("bcnInt", _cfg.beaconIntervalSec);
    prefs.putUChar("radio", _cfg.radioType);
    prefs.putUShort("txDelay", _cfg.txDelayMs);
    prefs.putUShort("txTail", _cfg.txTailMs);
    prefs.putUChar("txVol", _cfg.txVolume);
    prefs.putBool("autoAck", _cfg.autoAckMessages);
    prefs.putBool("digiW1", _cfg.digipeatWide1);

    prefs.putString("wSsid", _cfg.wifiSsid);
    prefs.putString("wPass", _cfg.wifiPass);
    prefs.putBool("igEn", _cfg.igateEnabled);
    prefs.putString("isSrv", _cfg.aprsIsServer);
    prefs.putUShort("isPort", _cfg.aprsIsPort);
    prefs.putInt("isPass", _cfg.aprsIsPasscode);
    prefs.putUChar("igBcn", _cfg.igateBeaconMode);
    prefs.putBool("igMsgRf", _cfg.igateIsToRfMsg);
    prefs.putString("otaUrl", _cfg.otaServerUrl);

    prefs.end();
    Modem.configure(_cfg.txDelayMs, _cfg.txTailMs, _cfg.txVolume, _cfg.txAudioPin);
}

void AprsEngine::applyBoardModel(BoardModel model) {
    _cfg.boardModel = model;
    if (model == BOARD_MODEL_ST7789) {
        // Modelo ST7789 (1 USB): Conectores P3 tienen IO35, IO22, IO21(BL), IO4, IO16, IO17
        // Usamos IO4 para Audio TX Sigma-Delta (dejando IO17/IO16 libres para GPS) + IO26 DAC2
        _cfg.txAudioPin = 4;
        _cfg.displayInvert = false;
    } else {
        // Modelo ILI9341: Conectores tienen IO35, IO22, IO21, IO27
        // Usamos IO27 para Audio TX Sigma-Delta (con BL en IO21) + IO26 DAC2
        _cfg.txAudioPin = 27;
        _cfg.displayInvert = false;
    }
    Modem.configure(_cfg.txDelayMs, _cfg.txTailMs, _cfg.txVolume, _cfg.txAudioPin);
}

void AprsEngine::applyRadioPreset(RadioType type) {
    _cfg.radioType = type;
    if (type == RADIO_BAOFENG) {
        // Baofeng UV-5R / UV-82: Squelch/ahorro de batería más lento, requiere preámbulo más largo
        _cfg.txDelayMs = 450;
        _cfg.txTailMs  = 60;
        _cfg.txVolume  = 75;
    } else if (type == RADIO_YAESU) {
        // Yaesu FT-60 / FT-65 / FT-2980: Conmutación PTT rápida y entrada MIC más sensible
        _cfg.txDelayMs = 260;
        _cfg.txTailMs  = 40;
        _cfg.txVolume  = 55;
    }
    Modem.configure(_cfg.txDelayMs, _cfg.txTailMs, _cfg.txVolume, _cfg.txAudioPin);
}

void AprsEngine::applyStationMode(StationMode mode) {
    _cfg.stationMode = mode;
    if (mode == STATION_MODE_TRACKER) {
        // Modo Tracker Móvil / Portátil
        _cfg.igateEnabled = false;
        _cfg.posMode = POS_MODE_GPS;
        _cfg.symbolTable = '/';
        if (_cfg.symbolCode == '&' || _cfg.symbolCode == '-') {
            _cfg.symbolCode = '>'; // Auto / Móvil por defecto en Tracker
        }
        if (_cfg.ssid == 10 || _cfg.ssid == 1) {
            _cfg.ssid = 9; // -9 Móvil / Tracker
        }
        if (_cfg.beaconIntervalSec > 300) {
            _cfg.beaconIntervalSec = 120;
        }
        strlcpy(_cfg.comment, "ESP32 CYD Tracker 1200bps", sizeof(_cfg.comment));
    } else if (mode == STATION_MODE_FIXED_IGATE) {
        // Modo Estación Fija + IGate APRS-IS
        _cfg.igateEnabled = true;
        _cfg.posMode = POS_MODE_FIXED;
        _cfg.symbolTable = 'I';
        _cfg.symbolCode  = '&'; // Símbolo oficial APRS IGate (I&)
        _cfg.ssid = 10;         // -10 SSID estándar de IGate
        if (_cfg.beaconIntervalSec < 300) {
            _cfg.beaconIntervalSec = 600;
        }
        strlcpy(_cfg.comment, "ESP32 CYD Estacion Fija + IGate", sizeof(_cfg.comment));
    } else if (mode == STATION_MODE_FIXED_RF) {
        // Modo Estación Fija Solo RF / Digipeater
        _cfg.igateEnabled = false;
        _cfg.posMode = POS_MODE_FIXED;
        _cfg.symbolTable = '/';
        _cfg.symbolCode  = '-'; // Casa / Estación Base QTH
        if (_cfg.ssid == 9 || _cfg.ssid == 10) {
            _cfg.ssid = 1;
        }
        if (_cfg.beaconIntervalSec < 300) {
            _cfg.beaconIntervalSec = 600;
        }
        strlcpy(_cfg.comment, "ESP32 CYD Estacion Fija VHF", sizeof(_cfg.comment));
    }
}

double AprsEngine::getCurrentLat() const {
    if (_cfg.posMode == POS_MODE_GPS && _gpsState.validFix) {
        return _gpsState.lat;
    }
    return _cfg.fixedLat;
}

double AprsEngine::getCurrentLon() const {
    if (_cfg.posMode == POS_MODE_GPS && _gpsState.validFix) {
        return _gpsState.lon;
    }
    return _cfg.fixedLon;
}

int16_t AprsEngine::getCurrentAlt() const {
    if (_cfg.posMode == POS_MODE_GPS && _gpsState.validFix) {
        return _gpsState.altMeters;
    }
    return _cfg.fixedAltMeters;
}

uint32_t AprsEngine::getSecondsUntilNextBeacon() const {
    if (!_cfg.autoBeacon || _cfg.beaconIntervalSec == 0) return 0;
    uint32_t elapsedSec = (millis() - _lastBeaconMs) / 1000;
    if (elapsedSec >= _cfg.beaconIntervalSec) return 0;
    return _cfg.beaconIntervalSec - elapsedSec;
}

void AprsEngine::update() {
    pollGps();

    // 1. Procesar tramas AX.25 recibidas desde el módem DSP (Core 0 -> Core 1)
    RawAx25Frame raw;
    while (Modem.receiveFrame(&raw)) {
        DecodedPacket decoded;
        if (decodeAx25Frame(raw, &decoded)) {
            decoded.fromAprsIs = false;

            // Si estamos en modo Estación Fija + IGate (o IGate activo), inyectar a APRS-IS
            if (_cfg.stationMode == STATION_MODE_FIXED_IGATE || _cfg.igateEnabled) {
                decoded.gatedToIs = Igate.injectRfPacket(decoded);
            } else {
                decoded.gatedToIs = false;
            }

            // Guardar en el buffer circular de monitoreo
            _packetLog[_packetHead] = decoded;
            _packetHead = (_packetHead + 1) % MAX_PACKET_LOG;
            if (_packetCount < MAX_PACKET_LOG) _packetCount++;

            // Actualizar directorio de estaciones escuchadas
            updateStationTable(decoded);

            // Si está activo el modo Fill-in Digipeater WIDE1-1, evaluar repetición RF
            if (_cfg.digipeatWide1) {
                maybeDigipeat(raw, decoded);
            }
        }
    }

    // 2. Baliza automática periódica
    if (_cfg.autoBeacon && _cfg.beaconIntervalSec >= 30) {
        if (millis() - _lastBeaconMs >= (uint32_t)_cfg.beaconIntervalSec * 1000UL) {
            sendPositionBeacon(false);
        }
    }

    // 3. Reintentos automáticos de mensajes salientes no confirmados
    processMessageRetries();
}

void AprsEngine::pollGps() {
    while (Serial2.available() > 0) {
        char c = (char)Serial2.read();
        _gpsState.charsProcessed++;
        if (s_gps.encode(c)) {
            if (s_gps.location.isValid() && s_gps.location.age() < 5000) {
                _gpsState.validFix = true;
                _gpsState.lat = s_gps.location.lat();
                _gpsState.lon = s_gps.location.lng();
            } else if (s_gps.location.age() > 15000) {
                _gpsState.validFix = false;
            }
            if (s_gps.altitude.isValid()) {
                _gpsState.altMeters = (int16_t)s_gps.altitude.meters();
            }
            if (s_gps.speed.isValid()) {
                _gpsState.speedKnots = (uint16_t)s_gps.speed.knots();
            }
            if (s_gps.course.isValid()) {
                _gpsState.courseDeg = (uint16_t)s_gps.course.deg();
            }
            if (s_gps.satellites.isValid()) {
                _gpsState.satellites = (uint8_t)s_gps.satellites.value();
            }
        }
    }
}

void AprsEngine::parseCallsignSsid(const char* fullStr, char* outCall, uint8_t* outSsid) {
    outCall[0] = '\0';
    *outSsid = 0;
    if (!fullStr) return;

    char temp[16];
    strlcpy(temp, fullStr, sizeof(temp));
    for (int i = 0; temp[i]; i++) {
        temp[i] = (char)toupper((unsigned char)temp[i]);
    }

    char* dash = strchr(temp, '-');
    if (dash) {
        *dash = '\0';
        int s = atoi(dash + 1);
        if (s >= 0 && s <= 15) *outSsid = (uint8_t)s;
    }
    strlcpy(outCall, temp, 7);
}

void AprsEngine::formatFullCallsign(const char* call, uint8_t ssid, char* outStr, size_t maxLen) {
    if (ssid > 0) {
        snprintf(outStr, maxLen, "%s-%u", call, ssid);
    } else {
        snprintf(outStr, maxLen, "%s", call);
    }
}

uint16_t AprsEngine::buildAx25Frame(uint8_t* outBuf,
                                    const char* srcCall, uint8_t srcSsid,
                                    const char* dstCall, uint8_t dstSsid,
                                    const char* path1, uint8_t path1Ssid,
                                    const char* path2, uint8_t path2Ssid,
                                    const char* infoField) {
    uint16_t pos = 0;

    auto writeAddress = [&](const char* call, uint8_t ssid, bool isLast) {
        for (int i = 0; i < 6; i++) {
            char c = ' ';
            if (call && i < (int)strlen(call) && call[i] != '\0') {
                c = (char)toupper((unsigned char)call[i]);
            }
            outBuf[pos++] = (uint8_t)(c << 1);
        }
        uint8_t ssidByte = 0x60 | ((ssid & 0x0F) << 1) | (isLast ? 0x01 : 0x00);
        outBuf[pos++] = ssidByte;
    };

    bool hasPath1 = (path1 && strlen(path1) > 0);
    bool hasPath2 = (path2 && strlen(path2) > 0);

    // 1. Destino (7 bytes)
    writeAddress(dstCall, dstSsid, false);
    // 2. Origen (7 bytes)
    writeAddress(srcCall, srcSsid, !hasPath1 && !hasPath2);
    // 3. Digipeaters (7 bytes c/u)
    if (hasPath1) {
        writeAddress(path1, path1Ssid, !hasPath2);
    }
    if (hasPath2) {
        writeAddress(path2, path2Ssid, true);
    }

    // 4. Control (0x03 = UI Frame) y PID (0xF0 = No Layer 3)
    outBuf[pos++] = 0x03;
    outBuf[pos++] = 0xF0;

    // 5. Campo de Información APRS
    size_t infoLen = strlen(infoField);
    if (pos + infoLen > MAX_AX25_FRAME_LEN - 4) {
        infoLen = MAX_AX25_FRAME_LEN - 4 - pos;
    }
    memcpy(&outBuf[pos], infoField, infoLen);
    pos += infoLen;

    return pos;
}

bool AprsEngine::sendPositionBeacon(bool loopbackTest) {
    double lat = getCurrentLat();
    double lon = getCurrentLon();
    int16_t altMeters = getCurrentAlt();

    // Convertir latitud decimal a formato APRS DDMM.mmN/S
    char ns = (lat >= 0.0) ? 'N' : 'S';
    double absLat = fabs(lat);
    int latDeg = (int)absLat;
    double latMin = (absLat - latDeg) * 60.0;

    // Convertir longitud decimal a formato APRS DDDMM.mmE/W
    char ew = (lon >= 0.0) ? 'E' : 'W';
    double absLon = fabs(lon);
    int lonDeg = (int)absLon;
    double lonMin = (absLon - lonDeg) * 60.0;

    // Convertir altitud en metros a pies para /A=xxxxxx
    int32_t altFeet = (int32_t)roundf(altMeters * 3.28084f);
    if (altFeet < 0) altFeet = 0;

    char info[160];
    snprintf(info, sizeof(info),
             "=%02d%05.2f%c%c%03d%05.2f%c%c/A=%06ld %s",
             latDeg, latMin, ns,
             _cfg.symbolTable,
             lonDeg, lonMin, ew,
             _cfg.symbolCode,
             (long)altFeet,
             _cfg.comment);

    _lastBeaconMs = millis();

    bool isIgateActive = (_cfg.stationMode == STATION_MODE_FIXED_IGATE) || _cfg.igateEnabled;
    bool sentToIs = false;

    // Si estamos en modo Estación Fija + IGate y no es loopback, enviar baliza a APRS-IS según configuración
    if (!loopbackTest && isIgateActive && _cfg.igateBeaconMode != IGATE_BCN_RF_ONLY) {
        sentToIs = Igate.sendSelfBeaconToAprsIs(info);
    }

    // Si el usuario eligió enviar baliza SOLO a APRS-IS (sin activar PTT de radio), retornar
    if (!loopbackTest && isIgateActive && _cfg.igateBeaconMode == IGATE_BCN_IS_ONLY) {
        return sentToIs;
    }

    uint8_t frame[MAX_AX25_FRAME_LEN];
    uint16_t frameLen = buildAx25Frame(
        frame,
        _cfg.callsign, _cfg.ssid,
        _cfg.destCall, 0,
        _cfg.path1, _cfg.path1Ssid,
        _cfg.path2, _cfg.path2Ssid,
        info
    );

    return Modem.transmitFrame(frame, frameLen, loopbackTest);
}

bool AprsEngine::sendTextMessage(const char* destCallsign, const char* messageText, bool loopbackTest) {
    if (!destCallsign || strlen(destCallsign) == 0 || !messageText) return false;

    char cleanDest[12];
    strlcpy(cleanDest, destCallsign, sizeof(cleanDest));
    for (int i = 0; cleanDest[i]; i++) {
        cleanDest[i] = (char)toupper((unsigned char)cleanDest[i]);
    }

    char seqStr[8];
    snprintf(seqStr, sizeof(seqStr), "%u", (unsigned)_nextMsgSeq);
    _nextMsgSeq = (_nextMsgSeq % 99) + 1;

    // Formato de mensaje APRS: ":DESTINO  :Texto del mensaje{ID" (Destino fijo a 9 caracteres)
    char info[140];
    snprintf(info, sizeof(info), ":%-9.9s:%-.64s{%s", cleanDest, messageText, seqStr);

    // Registrar en el historial de mensajes (al principio de la lista)
    for (int i = MAX_MESSAGES_LIST - 1; i > 0; i--) {
        _messages[i] = _messages[i - 1];
    }
    AprsMessageEntry& entry = _messages[0];
    entry.active = true;
    entry.incoming = false;
    strlcpy(entry.peerCall, cleanDest, sizeof(entry.peerCall));
    strlcpy(entry.text, messageText, sizeof(entry.text));
    strlcpy(entry.msgId, seqStr, sizeof(entry.msgId));
    entry.state = MSG_STATE_TX_WAIT;
    entry.retriesLeft = 3;
    entry.lastTxMs = millis();
    entry.timestampMs = millis();
    if (_msgCount < MAX_MESSAGES_LIST) _msgCount++;

    // Si el IGate está conectado a APRS-IS y no es prueba loopback, inyectar también por APRS-IS
    if (!loopbackTest && Igate.isAprsIsConnected()) {
        Igate.sendSelfBeaconToAprsIs(info);
    }

    uint8_t frame[MAX_AX25_FRAME_LEN];
    uint16_t frameLen = buildAx25Frame(
        frame,
        _cfg.callsign, _cfg.ssid,
        _cfg.destCall, 0,
        _cfg.path1, _cfg.path1Ssid,
        _cfg.path2, _cfg.path2Ssid,
        info
    );

    return Modem.transmitFrame(frame, frameLen, loopbackTest);
}

bool AprsEngine::sendCustomStatus(const char* statusText, bool loopbackTest) {
    char info[120];
    snprintf(info, sizeof(info), ">%s", statusText ? statusText : "ESP32 CYD APRS Ready");

    if (!loopbackTest && Igate.isAprsIsConnected()) {
        Igate.sendSelfBeaconToAprsIs(info);
    }

    uint8_t frame[MAX_AX25_FRAME_LEN];
    uint16_t frameLen = buildAx25Frame(
        frame,
        _cfg.callsign, _cfg.ssid,
        _cfg.destCall, 0,
        _cfg.path1, _cfg.path1Ssid,
        _cfg.path2, _cfg.path2Ssid,
        info
    );
    return Modem.transmitFrame(frame, frameLen, loopbackTest);
}

void AprsEngine::sendAckPacket(const char* toCallsign, const char* msgId) {
    char info[64];
    snprintf(info, sizeof(info), ":%-9.9s:ack%s", toCallsign, msgId);

    if (Igate.isAprsIsConnected()) {
        Igate.sendSelfBeaconToAprsIs(info);
    }

    uint8_t frame[MAX_AX25_FRAME_LEN];
    uint16_t frameLen = buildAx25Frame(
        frame,
        _cfg.callsign, _cfg.ssid,
        _cfg.destCall, 0,
        _cfg.path1, _cfg.path1Ssid,
        _cfg.path2, _cfg.path2Ssid,
        info
    );
    Modem.transmitFrame(frame, frameLen, false);
}

void AprsEngine::processMessageRetries() {
    uint32_t now = millis();
    for (int i = 0; i < _msgCount; i++) {
        AprsMessageEntry& m = _messages[i];
        if (!m.active || m.incoming || m.state != MSG_STATE_TX_WAIT) continue;

        if (now - m.lastTxMs >= 25000UL) {
            if (m.retriesLeft > 0) {
                m.retriesLeft--;
                m.lastTxMs = now;

                char info[140];
                snprintf(info, sizeof(info), ":%-9.9s:%-.64s{%s", m.peerCall, m.text, m.msgId);

                if (Igate.isAprsIsConnected()) {
                    Igate.sendSelfBeaconToAprsIs(info);
                }

                uint8_t frame[MAX_AX25_FRAME_LEN];
                uint16_t frameLen = buildAx25Frame(
                    frame,
                    _cfg.callsign, _cfg.ssid,
                    _cfg.destCall, 0,
                    _cfg.path1, _cfg.path1Ssid,
                    _cfg.path2, _cfg.path2Ssid,
                    info
                );
                Modem.transmitFrame(frame, frameLen, false);
            } else {
                m.state = MSG_STATE_TX_FAIL;
            }
        }
    }
}

bool AprsEngine::wasStationHeardRecentlyOnRf(const char* callsign, uint32_t maxAgeMs) const {
    if (!callsign || callsign[0] == '\0') return false;
    uint32_t now = millis();
    for (int i = 0; i < MAX_STATIONS_LIST; i++) {
        if (_stations[i].active && strcasecmp(_stations[i].callsign, callsign) == 0) {
            if (now - _stations[i].lastHeardMs <= maxAgeMs) {
                return true;
            }
        }
    }
    return false;
}

bool AprsEngine::handleAprsIsIncomingLine(const char* tnc2Line) {
    if (!tnc2Line || tnc2Line[0] == '#' || strlen(tnc2Line) < 10) return false;

    // Parsear cabecera TNC2: SRC>DST,PATH:INFO
    const char* gt = strchr(tnc2Line, '>');
    const char* colon = strchr(tnc2Line, ':');
    if (!gt || !colon || colon <= gt) return false;

    DecodedPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.timestampMs = millis();
    pkt.fromAprsIs = true;

    size_t srcLen = (size_t)(gt - tnc2Line);
    if (srcLen >= sizeof(pkt.srcCall)) srcLen = sizeof(pkt.srcCall) - 1;
    memcpy(pkt.srcCall, tnc2Line, srcLen);
    pkt.srcCall[srcLen] = '\0';

    const char* comma = strchr(gt + 1, ',');
    if (comma && comma < colon) {
        size_t dstLen = (size_t)(comma - (gt + 1));
        if (dstLen >= sizeof(pkt.dstCall)) dstLen = sizeof(pkt.dstCall) - 1;
        memcpy(pkt.dstCall, gt + 1, dstLen);
        pkt.dstCall[dstLen] = '\0';

        size_t pathLen = (size_t)(colon - (comma + 1));
        if (pathLen >= sizeof(pkt.pathStr)) pathLen = sizeof(pkt.pathStr) - 1;
        memcpy(pkt.pathStr, comma + 1, pathLen);
        pkt.pathStr[pathLen] = '\0';
    } else {
        size_t dstLen = (size_t)(colon - (gt + 1));
        if (dstLen >= sizeof(pkt.dstCall)) dstLen = sizeof(pkt.dstCall) - 1;
        memcpy(pkt.dstCall, gt + 1, dstLen);
        pkt.dstCall[dstLen] = '\0';
    }

    strlcpy(pkt.infoText, colon + 1, sizeof(pkt.infoText));

    // Evaluar si es un mensaje APRS (:DESTINO  :texto)
    const char* info = pkt.infoText;
    size_t len = strlen(info);
    if (len >= 11 && info[0] == ':' && info[10] == ':') {
        char addressee[10];
        memcpy(addressee, &info[1], 9);
        addressee[9] = '\0';
        for (int i = 8; i >= 0 && addressee[i] == ' '; i--) {
            addressee[i] = '\0';
        }

        const char* body = &info[11];

        // 1. Verificar si es para nuestra propia estación (procesa mensaje o ACK entrante)
        handleIncomingMessage(pkt, addressee, body);

        // 2. Si está habilitado IGate IS->RF y el destinatario fue escuchado por RF recientemente,
        // retransmitir en VHF/UHF con formato de tercer partido "}SRC>DST,TCPIP,MYCALL*:INFO"
        char myFullCall[12];
        formatFullCallsign(_cfg.callsign, _cfg.ssid, myFullCall, sizeof(myFullCall));
        bool isForMe = (strcasecmp(addressee, myFullCall) == 0) ||
                       (strcasecmp(addressee, _cfg.callsign) == 0);

        if (!isForMe && _cfg.igateIsToRfMsg && wasStationHeardRecentlyOnRf(addressee)) {
            char thirdPartyInfo[180];
            snprintf(thirdPartyInfo, sizeof(thirdPartyInfo),
                     "}%s>%s,TCPIP,%s*:%s",
                     pkt.srcCall, pkt.dstCall, myFullCall, pkt.infoText);

            uint8_t frame[MAX_AX25_FRAME_LEN];
            uint16_t frameLen = buildAx25Frame(
                frame,
                _cfg.callsign, _cfg.ssid,
                _cfg.destCall, 0,
                _cfg.path1, _cfg.path1Ssid,
                nullptr, 0,
                thirdPartyInfo
            );
            Modem.transmitFrame(frame, frameLen, false);
            Serial.printf("[IGATE IS->RF] Retransmitido a estacion local %s: %s\n", addressee, thirdPartyInfo);
            return true;
        }
    }

    return false;
}

bool AprsEngine::decodeAx25Frame(const RawAx25Frame& raw, DecodedPacket* outPkt) {
    if (raw.len < 16) return false;

    memset(outPkt, 0, sizeof(DecodedPacket));
    outPkt->timestampMs = millis();
    outPkt->audioPeak = raw.audioPeak;

    auto extractAddr = [&](uint16_t offset, char* outStr, size_t maxLen, bool* isLast, bool* hasRepeated) {
        char call[7];
        int cLen = 0;
        for (int i = 0; i < 6; i++) {
            char c = (char)(raw.data[offset + i] >> 1);
            if (c != ' ' && c >= 32 && c <= 126) {
                call[cLen++] = c;
            }
        }
        call[cLen] = '\0';
        uint8_t ssidByte = raw.data[offset + 6];
        uint8_t ssid = (ssidByte >> 1) & 0x0F;
        *isLast = (ssidByte & 0x01) != 0;
        *hasRepeated = (ssidByte & 0x80) != 0;
        formatFullCallsign(call, ssid, outStr, maxLen);
    };

    bool isLast = false;
    bool hasRep = false;
    extractAddr(0, outPkt->dstCall, sizeof(outPkt->dstCall), &isLast, &hasRep);
    extractAddr(7, outPkt->srcCall, sizeof(outPkt->srcCall), &isLast, &hasRep);

    uint16_t pos = 14;
    outPkt->pathStr[0] = '\0';

    while (!isLast && pos + 7 <= raw.len) {
        char digiCall[12];
        extractAddr(pos, digiCall, sizeof(digiCall), &isLast, &hasRep);
        pos += 7;

        if (strlen(outPkt->pathStr) > 0) {
            strlcat(outPkt->pathStr, ",", sizeof(outPkt->pathStr));
        }
        strlcat(outPkt->pathStr, digiCall, sizeof(outPkt->pathStr));
        if (hasRep) {
            strlcat(outPkt->pathStr, "*", sizeof(outPkt->pathStr));
        }
    }

    // Verificar bytes Control (0x03) y PID (0xF0)
    if (pos + 2 > raw.len) return false;
    uint8_t ctrl = raw.data[pos++];
    uint8_t pid  = raw.data[pos++];
    if ((ctrl & 0xEF) != 0x03 || pid != 0xF0) {
        return false;
    }

    uint16_t infoLen = raw.len - pos;
    if (infoLen >= sizeof(outPkt->infoText)) {
        infoLen = sizeof(outPkt->infoText) - 1;
    }
    memcpy(outPkt->infoText, &raw.data[pos], infoLen);
    outPkt->infoText[infoLen] = '\0';

    // Limpiar caracteres no imprimibles
    for (uint16_t i = 0; i < infoLen; i++) {
        if ((uint8_t)outPkt->infoText[i] < 32 || (uint8_t)outPkt->infoText[i] > 126) {
            outPkt->infoText[i] = ' ';
        }
    }

    parseAprsInfo(outPkt);
    return true;
}

void AprsEngine::maybeDigipeat(const RawAx25Frame& raw, const DecodedPacket& decoded) {
    char myFullCall[12];
    formatFullCallsign(_cfg.callsign, _cfg.ssid, myFullCall, sizeof(myFullCall));
    if (strcasecmp(decoded.srcCall, myFullCall) == 0) return;

    // Buscar el primer digipeater aún no repetido en la cabecera AX.25
    if (raw.len < 23) return;
    bool srcLast = (raw.data[13] & 0x01) != 0;
    if (srcLast) return;

    uint16_t pos = 14;
    while (pos + 7 <= raw.len) {
        uint8_t ssidByte = raw.data[pos + 6];
        bool isLast = (ssidByte & 0x01) != 0;
        bool hasRep = (ssidByte & 0x80) != 0;

        if (!hasRep) {
            // Verificar si este salto es WIDE1-1
            char hopCall[7];
            int cLen = 0;
            for (int i = 0; i < 6; i++) {
                char c = (char)(raw.data[pos + i] >> 1);
                if (c != ' ') hopCall[cLen++] = c;
            }
            hopCall[cLen] = '\0';
            uint8_t hopSsid = (ssidByte >> 1) & 0x0F;

            if (strcasecmp(hopCall, "WIDE1") == 0 && hopSsid == 1) {
                uint8_t txCopy[MAX_AX25_FRAME_LEN];
                memcpy(txCopy, raw.data, raw.len);
                // Marcar como repetido (H-bit = 1, SSID = 0)
                txCopy[pos + 6] = (ssidByte & 0xE1) | 0x80;
                Modem.transmitFrame(txCopy, raw.len, false);
            }
            return;
        }

        if (isLast) break;
        pos += 7;
    }
}

void AprsEngine::parseAprsInfo(DecodedPacket* pkt) {
    const char* info = pkt->infoText;
    size_t len = strlen(info);
    if (len == 0) {
        pkt->type = APRS_PKT_OTHER;
        return;
    }

    char dti = info[0];

    // 1. Mensajes APRS y ACKs (":DESTINO  :mensaje")
    if (dti == ':' && len >= 11 && info[10] == ':') {
        char addressee[10];
        memcpy(addressee, &info[1], 9);
        addressee[9] = '\0';
        // Quitar espacios a la derecha
        for (int i = 8; i >= 0 && addressee[i] == ' '; i--) {
            addressee[i] = '\0';
        }

        const char* body = &info[11];
        if (strncmp(body, "ack", 3) == 0) {
            pkt->type = APRS_PKT_ACK;
            snprintf(pkt->comment, sizeof(pkt->comment), "ACK para %s (%s)", addressee, body + 3);
        } else {
            pkt->type = APRS_PKT_MESSAGE;
            snprintf(pkt->comment, sizeof(pkt->comment), "Para %s: %s", addressee, body);
        }
        handleIncomingMessage(*pkt, addressee, body);
        return;
    }

    // 2. Paquetes de Posición ('!', '=', '/', '@')
    if (dti == '!' || dti == '=' || dti == '/' || dti == '@') {
        pkt->type = APRS_PKT_POSITION;
        int offset = (dti == '/' || dti == '@') ? 8 : 1; // Saltar timestamp de 7 bytes si existe

        // Formato sin comprimir: DDMM.mmN/DDDMM.mmW$
        if (len >= (size_t)(offset + 19) &&
            (info[offset + 7] == 'N' || info[offset + 7] == 'S') &&
            (info[offset + 17] == 'E' || info[offset + 17] == 'W')) {

            char latDegStr[3] = { info[offset], info[offset + 1], '\0' };
            char latMinStr[6];
            memcpy(latMinStr, &info[offset + 2], 5);
            latMinStr[5] = '\0';

            double lat = atof(latDegStr) + (atof(latMinStr) / 60.0);
            if (info[offset + 7] == 'S') lat = -lat;

            pkt->symbolTable = info[offset + 8];

            char lonDegStr[4] = { info[offset + 9], info[offset + 10], info[offset + 11], '\0' };
            char lonMinStr[6];
            memcpy(lonMinStr, &info[offset + 12], 5);
            lonMinStr[5] = '\0';

            double lon = atof(lonDegStr) + (atof(lonMinStr) / 60.0);
            if (info[offset + 17] == 'W') lon = -lon;

            pkt->symbolCode = info[offset + 18];
            pkt->hasPosition = true;
            pkt->lat = lat;
            pkt->lon = lon;

            pkt->distanceKm = calcDistanceKm(getCurrentLat(), getCurrentLon(), lat, lon);
            pkt->bearingDeg = calcBearingDeg(getCurrentLat(), getCurrentLon(), lat, lon);

            const char* rest = &info[offset + 19];
            // Buscar altitud /A=xxxxxx si está presente
            const char* altTag = strstr(rest, "/A=");
            if (altTag && strlen(altTag) >= 9) {
                char altFeetStr[7];
                memcpy(altFeetStr, altTag + 3, 6);
                altFeetStr[6] = '\0';
                pkt->altitudeM = (int16_t)roundf(atol(altFeetStr) * 0.3048f);
                const char* afterAlt = altTag + 9;
                while (*afterAlt == ' ') afterAlt++;
                strlcpy(pkt->comment, afterAlt, sizeof(pkt->comment));
            } else {
                strlcpy(pkt->comment, rest, sizeof(pkt->comment));
            }
            return;
        }
    }

    // 3. Paquetes de Estado ('>')
    if (dti == '>') {
        pkt->type = APRS_PKT_STATUS;
        strlcpy(pkt->comment, info + 1, sizeof(pkt->comment));
        return;
    }

    pkt->type = APRS_PKT_OTHER;
    strlcpy(pkt->comment, info, sizeof(pkt->comment));
}

void AprsEngine::handleIncomingMessage(const DecodedPacket& pkt, const char* addressee, const char* body) {
    char myFullCall[12];
    formatFullCallsign(_cfg.callsign, _cfg.ssid, myFullCall, sizeof(myFullCall));

    bool isForMe = (strcasecmp(addressee, myFullCall) == 0) ||
                   (strcasecmp(addressee, _cfg.callsign) == 0);

    // ¿Es un ACK dirigido a nosotros?
    if (strncmp(body, "ack", 3) == 0) {
        if (!isForMe) return;
        const char* ackId = body + 3;
        for (int i = 0; i < _msgCount; i++) {
            if (_messages[i].active && !_messages[i].incoming &&
                strcmp(_messages[i].msgId, ackId) == 0) {
                _messages[i].state = MSG_STATE_TX_ACK;
            }
        }
        return;
    }

    // Extraer texto y número de secuencia {id si existe
    char cleanText[68];
    char msgId[8] = "";
    strlcpy(cleanText, body, sizeof(cleanText));
    char* brace = strrchr(cleanText, '{');
    if (brace) {
        *brace = '\0';
        strlcpy(msgId, brace + 1, sizeof(msgId));
    }

    // Si es para nosotros (o queremos guardar todos los mensajes dirigidos a nuestra estación)
    if (isForMe) {
        if (_cfg.autoAckMessages && strlen(msgId) > 0) {
            sendAckPacket(pkt.srcCall, msgId);
        }

        // Evitar duplicados si el remitente retransmitió el mismo msgId
        for (int i = 0; i < _msgCount; i++) {
            if (_messages[i].active && _messages[i].incoming &&
                strcasecmp(_messages[i].peerCall, pkt.srcCall) == 0 &&
                strlen(msgId) > 0 && strcmp(_messages[i].msgId, msgId) == 0) {
                return;
            }
        }

        for (int i = MAX_MESSAGES_LIST - 1; i > 0; i--) {
            _messages[i] = _messages[i - 1];
        }
        AprsMessageEntry& m = _messages[0];
        m.active = true;
        m.incoming = true;
        strlcpy(m.peerCall, pkt.srcCall, sizeof(m.peerCall));
        strlcpy(m.text, cleanText, sizeof(m.text));
        strlcpy(m.msgId, msgId, sizeof(m.msgId));
        m.state = MSG_STATE_RX;
        m.retriesLeft = 0;
        m.timestampMs = millis();
        if (_msgCount < MAX_MESSAGES_LIST) _msgCount++;
        _unreadMsgCount++;
    }
}

void AprsEngine::updateStationTable(const DecodedPacket& pkt) {
    int targetIdx = -1;
    for (int i = 0; i < MAX_STATIONS_LIST; i++) {
        if (_stations[i].active && strcasecmp(_stations[i].callsign, pkt.srcCall) == 0) {
            targetIdx = i;
            break;
        }
    }

    if (targetIdx < 0) {
        for (int i = 0; i < MAX_STATIONS_LIST; i++) {
            if (!_stations[i].active) {
                targetIdx = i;
                _stations[i].pktCount = 0;
                break;
            }
        }
    }

    if (targetIdx < 0) {
        // Reemplazar la estación más antigua
        targetIdx = MAX_STATIONS_LIST - 1;
        _stations[targetIdx].pktCount = 0;
    }

    StationEntry& st = _stations[targetIdx];
    st.active = true;
    strlcpy(st.callsign, pkt.srcCall, sizeof(st.callsign));
    st.lastHeardMs = pkt.timestampMs;
    st.pktCount++;
    if (pkt.hasPosition) {
        st.hasPos = true;
        st.lat = pkt.lat;
        st.lon = pkt.lon;
        st.distanceKm = pkt.distanceKm;
        st.bearingDeg = pkt.bearingDeg;
        st.symbolTable = pkt.symbolTable;
        st.symbolCode  = pkt.symbolCode;
    }
    if (strlen(pkt.comment) > 0) {
        strlcpy(st.lastComment, pkt.comment, sizeof(st.lastComment));
    }
}

const DecodedPacket& AprsEngine::getPacketLog(int index) const {
    if (index < 0) index = 0;
    if (index >= _packetCount) index = _packetCount - 1;
    int pos = (_packetHead - 1 - index + MAX_PACKET_LOG) % MAX_PACKET_LOG;
    return _packetLog[pos];
}

int AprsEngine::getStationCount() const {
    int c = 0;
    for (int i = 0; i < MAX_STATIONS_LIST; i++) {
        if (_stations[i].active) c++;
    }
    return c;
}

const AprsMessageEntry& AprsEngine::getMessage(int index) const {
    if (index < 0) index = 0;
    if (index >= _msgCount) index = _msgCount - 1;
    return _messages[index];
}

float AprsEngine::calcDistanceKm(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0;
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dLon / 2.0) * sin(dLon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return (float)(R * c);
}

int16_t AprsEngine::calcBearingDeg(double lat1, double lon1, double lat2, double lon2) {
    double phi1 = lat1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double y = sin(dLon) * cos(phi2);
    double x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(dLon);
    double brng = atan2(y, x) * 180.0 / M_PI;
    return (int16_t)((int)round(brng + 360.0) % 360);
}

const char* AprsEngine::bearingToCardinal(int16_t deg) {
    static const char* dirs[] = { "N", "NE", "E", "SE", "S", "SO", "O", "NO" };
    int idx = ((deg + 22) % 360) / 45;
    return dirs[idx & 7];
}
