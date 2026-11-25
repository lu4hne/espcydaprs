#include "aprs_tracker.h"
#include <Preferences.h>
#include <TinyGPS++.h>
#include <stdarg.h>
#include <math.h>

AprsTracker Tracker;
static TinyGPSPlus s_gps;

AprsTracker::AprsTracker() {
    memset(&_cfg, 0, sizeof(_cfg));
    memset(&_gps, 0, sizeof(_gps));
    memset(_pendingDest, 0, sizeof(_pendingDest));
    memset(_pendingText, 0, sizeof(_pendingText));
    memset(_pendingId, 0, sizeof(_pendingId));
}

void AprsTracker::begin() {
    loadConfig();
    Modem.configure(_cfg.txDelayMs, _cfg.txTailMs, _cfg.txVolume);

    // Iniciar GPS NMEA en UART1 (GPIO 20=RX, GPIO 21=TX)
    Serial1.begin(MT_GPS_BAUD_RATE, SERIAL_8N1, MT_GPS_RX_PIN, MT_GPS_TX_PIN);
    _lastBeaconMs = millis();
}

void AprsTracker::loadConfig() {
    Preferences prefs;
    if (!prefs.begin("mt_cfg", false)) {
        Serial.println(F("[CFG] ERROR: No se pudo abrir NVS 'mt_cfg'"));
        return;
    }

    if (!prefs.isKey("call")) {
        strlcpy(_cfg.callsign, "N0CALL", sizeof(_cfg.callsign));
        _cfg.ssid = 9;
        strlcpy(_cfg.destCall, "APEC3T", sizeof(_cfg.destCall));
        strlcpy(_cfg.path1, "WIDE1", sizeof(_cfg.path1));
        _cfg.path1Ssid = 1;
        strlcpy(_cfg.path2, "WIDE2", sizeof(_cfg.path2));
        _cfg.path2Ssid = 1;
        _cfg.symbolTable = '/';
        _cfg.symbolCode  = '>'; // Vehículo móvil
        strlcpy(_cfg.comment, "ESP32-C3 MicroTracker APRS", sizeof(_cfg.comment));

        _cfg.useGps = true;
        _cfg.fixedLat = -34.6037;
        _cfg.fixedLon = -58.3816;
        _cfg.fixedAltMeters = 25;

        _cfg.autoBeacon = true;
        _cfg.beaconIntervalSec = 120;
        _cfg.smartBeacon = true;
        _cfg.sbFastRateSec = 60;
        _cfg.sbSlowRateSec = 600;
        _cfg.sbFastSpeedKmh = 60;
        _cfg.sbSlowSpeedKmh = 5;
        _cfg.sbTurnAngleDeg = 28;

        _cfg.radioPreset = RADIO_BAOFENG;
        _cfg.txDelayMs = 450;
        _cfg.txTailMs  = 60;
        _cfg.txVolume  = 75;
        _cfg.autoAck   = true;

        _cfg.sosIntervalSec = 60;
        _cfg.sosSendMsg = false;
        strlcpy(_cfg.sosMsgDest, "", sizeof(_cfg.sosMsgDest));
        strlcpy(_cfg.sosText, "EMERGENCIA SOS! Necesito auxilio", sizeof(_cfg.sosText));

        prefs.end();
        saveConfig();
        return;
    }

    strlcpy(_cfg.callsign, prefs.getString("call", "N0CALL").c_str(), sizeof(_cfg.callsign));
    _cfg.ssid = prefs.getUChar("ssid", 9);
    strlcpy(_cfg.destCall, prefs.getString("dest", "APEC3T").c_str(), sizeof(_cfg.destCall));
    strlcpy(_cfg.path1, prefs.getString("path1", "WIDE1").c_str(), sizeof(_cfg.path1));
    _cfg.path1Ssid = prefs.getUChar("p1ssid", 1);
    strlcpy(_cfg.path2, prefs.getString("path2", "WIDE2").c_str(), sizeof(_cfg.path2));
    _cfg.path2Ssid = prefs.getUChar("p2ssid", 1);
    _cfg.symbolTable = (char)prefs.getUChar("symT", '/');
    _cfg.symbolCode  = (char)prefs.getUChar("symC", '>');
    strlcpy(_cfg.comment, prefs.getString("comm", "ESP32-C3 MicroTracker APRS").c_str(), sizeof(_cfg.comment));

    _cfg.useGps = prefs.getBool("useGps", true);
    _cfg.fixedLat = prefs.getDouble("lat", -34.6037);
    _cfg.fixedLon = prefs.getDouble("lon", -58.3816);
    _cfg.fixedAltMeters = prefs.getShort("alt", 25);

    _cfg.autoBeacon = prefs.getBool("autoBcn", true);
    _cfg.beaconIntervalSec = prefs.getUShort("bcnInt", 120);
    _cfg.smartBeacon = prefs.getBool("sbEn", true);
    _cfg.sbFastRateSec = prefs.getUShort("sbFast", 60);
    _cfg.sbSlowRateSec = prefs.getUShort("sbSlow", 600);
    _cfg.sbFastSpeedKmh = prefs.getUChar("sbFSpd", 60);
    _cfg.sbSlowSpeedKmh = prefs.getUChar("sbSSpd", 5);
    _cfg.sbTurnAngleDeg = prefs.getUChar("sbTurn", 28);

    _cfg.radioPreset = prefs.getUChar("radio", RADIO_BAOFENG);
    _cfg.txDelayMs = prefs.getUShort("txDelay", 450);
    _cfg.txTailMs  = prefs.getUShort("txTail", 60);
    _cfg.txVolume  = prefs.getUChar("txVol", 75);
    _cfg.autoAck   = prefs.getBool("autoAck", true);

    _cfg.sosIntervalSec = prefs.getUShort("sosInt", 60);
    if (_cfg.sosIntervalSec < 30 || _cfg.sosIntervalSec > 300) _cfg.sosIntervalSec = 60;
    _cfg.sosSendMsg = prefs.getBool("sosMsg", false);
    strlcpy(_cfg.sosMsgDest, prefs.getString("sosDst", "").c_str(), sizeof(_cfg.sosMsgDest));
    strlcpy(_cfg.sosText, prefs.getString("sosTxt", "EMERGENCIA SOS! Necesito auxilio").c_str(), sizeof(_cfg.sosText));

    prefs.end();
    Serial.printf("[CFG] Cargado OK: %s-%u | Bcn:%us | SmartBcn:%s | TX:%ums Vol:%u%%\n",
                  _cfg.callsign, _cfg.ssid, _cfg.beaconIntervalSec,
                  _cfg.smartBeacon ? "ON" : "OFF", _cfg.txDelayMs, _cfg.txVolume);
}

void AprsTracker::saveConfig() {
    Preferences prefs;
    if (!prefs.begin("mt_cfg", false)) {
        Serial.println(F("[CFG] ERROR al guardar en NVS"));
        return;
    }

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

    prefs.putBool("useGps", _cfg.useGps);
    prefs.putDouble("lat", _cfg.fixedLat);
    prefs.putDouble("lon", _cfg.fixedLon);
    prefs.putShort("alt", _cfg.fixedAltMeters);

    prefs.putBool("autoBcn", _cfg.autoBeacon);
    prefs.putUShort("bcnInt", _cfg.beaconIntervalSec);
    prefs.putBool("sbEn", _cfg.smartBeacon);
    prefs.putUShort("sbFast", _cfg.sbFastRateSec);
    prefs.putUShort("sbSlow", _cfg.sbSlowRateSec);
    prefs.putUChar("sbFSpd", _cfg.sbFastSpeedKmh);
    prefs.putUChar("sbSSpd", _cfg.sbSlowSpeedKmh);
    prefs.putUChar("sbTurn", _cfg.sbTurnAngleDeg);

    prefs.putUChar("radio", _cfg.radioPreset);
    prefs.putUShort("txDelay", _cfg.txDelayMs);
    prefs.putUShort("txTail", _cfg.txTailMs);
    prefs.putUChar("txVol", _cfg.txVolume);
    prefs.putBool("autoAck", _cfg.autoAck);

    prefs.putUShort("sosInt", _cfg.sosIntervalSec);
    prefs.putBool("sosMsg", _cfg.sosSendMsg);
    prefs.putString("sosDst", _cfg.sosMsgDest);
    prefs.putString("sosTxt", _cfg.sosText);

    prefs.end();
    Modem.configure(_cfg.txDelayMs, _cfg.txTailMs, _cfg.txVolume);
    Serial.printf("[CFG] Guardado OK en NVS: %s-%u\n", _cfg.callsign, _cfg.ssid);
}

void AprsTracker::applyRadioPreset(RadioPreset preset) {
    _cfg.radioPreset = preset;
    if (preset == RADIO_BAOFENG) {
        _cfg.txDelayMs = 450;
        _cfg.txTailMs  = 60;
        _cfg.txVolume  = 75;
    } else if (preset == RADIO_YAESU) {
        _cfg.txDelayMs = 250;
        _cfg.txTailMs  = 50;
        _cfg.txVolume  = 65;
    }
    Modem.configure(_cfg.txDelayMs, _cfg.txTailMs, _cfg.txVolume);
}

void AprsTracker::formatMyFullCall(char* out, size_t outSize) const {
    if (_cfg.ssid > 0) snprintf(out, outSize, "%s-%u", _cfg.callsign, _cfg.ssid);
    else               snprintf(out, outSize, "%s", _cfg.callsign);
}

double AprsTracker::getCurrentLat() const {
    return (_cfg.useGps && _gps.validFix) ? _gps.lat : _cfg.fixedLat;
}

double AprsTracker::getCurrentLon() const {
    return (_cfg.useGps && _gps.validFix) ? _gps.lon : _cfg.fixedLon;
}

int16_t AprsTracker::getCurrentAlt() const {
    return (_cfg.useGps && _gps.validFix) ? _gps.altMeters : _cfg.fixedAltMeters;
}

void AprsTracker::emitEvent(const char* fmt, ...) {
    char buf[180];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Serial.println(buf);
    if (_eventCb) {
        _eventCb(buf);
    }
}

void AprsTracker::update() {
    pollGps();

    // 1. Procesar tramas AX.25 recibidas por RF (mensajes y ACKs)
    RawAx25Frame raw;
    while (Modem.receiveFrame(&raw)) {
        processIncomingFrame(raw);
    }

    // 2. Temporizador de SOS o baliza de posición (Fija o SmartBeaconing)
    uint32_t now = millis();
    if (_sosActive) {
        if (now - _lastSosBeaconMs >= (uint32_t)_cfg.sosIntervalSec * 1000UL) {
            _lastSosBeaconMs = now;
            sendPositionBeacon(false);
            if (_cfg.sosSendMsg && strlen(_cfg.sosMsgDest) >= 3) {
                sendTextMessage(_cfg.sosMsgDest, _cfg.sosText, false);
            }
        }
    } else if (_cfg.autoBeacon) {
        evaluateBeaconTimer();
    }

    // 3. Reintentos de mensaje saliente pendiente de ACK (hasta 3 intentos cada 20 s)
    if (_pendingMsgActive && now >= _pendingNextTryMs) {
        if (_pendingRetries >= 3) {
            emitEvent("[MSG-TIMEOUT] Sin ACK de %s (id:%s)", _pendingDest, _pendingId);
            _pendingMsgActive = false;
        } else {
            char payload[110];
            snprintf(payload, sizeof(payload), ":%-9.9s:%s{%s", _pendingDest, _pendingText, _pendingId);
            buildAndSendUiFrame(payload, false);
            _pendingRetries++;
            _pendingNextTryMs = now + 20000UL;
            emitEvent("[MSG-RETRY %u/3] -> %s: %s", _pendingRetries, _pendingDest, _pendingText);
        }
    }
}

void AprsTracker::pollGps() {
    bool dumpRaw = (_gpsRawUntilMs != 0);
    if (dumpRaw && (int32_t)(millis() - _gpsRawUntilMs) >= 0) {
        _gpsRawUntilMs = 0;
        dumpRaw = false;
        emitEvent("[GPS-RAW] Fin de captura.");
    }

    while (Serial1.available() > 0) {
        char c = (char)Serial1.read();
        _gps.charsProcessed++;

        if (dumpRaw) {
            if (c == '\n') {
                if (_gpsLineLen > 0) {
                    _gpsLineBuf[_gpsLineLen] = '\0';
                    Serial.printf("[NMEA] %s\n", _gpsLineBuf);
                    _gpsLineLen = 0;
                }
            } else if (c != '\r' && c >= 32 && c <= 126 && _gpsLineLen < sizeof(_gpsLineBuf) - 1) {
                _gpsLineBuf[_gpsLineLen++] = c;
            }
        }

        if (s_gps.encode(c)) {
            if (s_gps.location.isValid() && s_gps.location.age() < 5000) {
                _gps.validFix = true;
                _gps.lat = s_gps.location.lat();
                _gps.lon = s_gps.location.lng();
            } else if (s_gps.location.age() > 15000) {
                _gps.validFix = false;
            }
            if (s_gps.altitude.isValid())   _gps.altMeters  = (int16_t)s_gps.altitude.meters();
            if (s_gps.speed.isValid())      _gps.speedKmh   = (float)s_gps.speed.kmph();
            if (s_gps.course.isValid())     _gps.courseDeg  = (uint16_t)s_gps.course.deg();
            if (s_gps.satellites.isValid()) _gps.satellites = (uint8_t)s_gps.satellites.value();
        }
    }
}

void AprsTracker::startGpsRawDump(uint16_t durationMs) {
    _gpsLineLen = 0;
    _gpsRawUntilMs = millis() + durationMs;
    emitEvent("[GPS-RAW] Mostrando tramas NMEA crudas por %u s (RX:IO%d TX:IO%d @ %u bps)...",
              (unsigned)(durationMs / 1000), MT_GPS_RX_PIN, MT_GPS_TX_PIN, (unsigned)MT_GPS_BAUD_RATE);
}

void AprsTracker::sendGpsRawCommand(const char* cmd) {
    if (!cmd || *cmd == '\0') return;

    char txBuf[128];
    if (cmd[0] == '$' && strchr(cmd, '*') == nullptr) {
        uint8_t csum = 0;
        for (const char* p = cmd + 1; *p; p++) csum ^= (uint8_t)(*p);
        snprintf(txBuf, sizeof(txBuf), "%s*%02X\r\n", cmd, csum);
    } else {
        snprintf(txBuf, sizeof(txBuf), "%s\r\n", cmd);
    }

    Serial1.print(txBuf);
    Serial1.flush();
    Serial.printf("[GPS-CMD] Enviado: %s", txBuf);
    startGpsRawDump(2500);
}

void AprsTracker::runGpsCommandTest() {
    emitEvent("[GPS-TEST] Iniciando prueba en UART1 (RX:IO%d TX:IO%d @ %u bps)...",
              MT_GPS_RX_PIN, MT_GPS_TX_PIN, (unsigned)MT_GPS_BAUD_RATE);
    Serial.printf("[GPS-TEST] Acumulado previo -> chars:%lu | NMEA OK:%lu | FailCRC:%lu | Sat:%u | Fix:%s\n",
                  (unsigned long)_gps.charsProcessed,
                  (unsigned long)s_gps.passedChecksum(),
                  (unsigned long)s_gps.failedChecksum(),
                  (unsigned)_gps.satellites,
                  _gps.validFix ? "SI" : "NO");

    // Limpiar buffer viejo
    while (Serial1.available() > 0) {
        s_gps.encode((char)Serial1.read());
        _gps.charsProcessed++;
    }

    // 1) Consulta u-blox UBX-MON-VER (NEO-6M / 7M / 8M / M10)
    static const uint8_t ubxMonVer[] = { 0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 0x0E, 0x34 };
    Serial1.write(ubxMonVer, sizeof(ubxMonVer));
    // 2) Consulta ATGM336H / CASIC
    Serial1.print("$PCAS06,0*1B\r\n");
    // 3) Consulta MTK / Quectel L80
    Serial1.print("$PMTK605*31\r\n");
    Serial1.flush();

    uint32_t bytesRx = 0;
    uint32_t nmeaLines = 0;
    bool ubxReply = false;
    bool nmeaCmdReply = false;
    char sampleNmea[96] = "";
    char replyInfo[96] = "";

    // Estado simple para detectar cabecera UBX (0xB5 0x62 0x0A 0x04) y extraer texto ASCII
    uint8_t ubxState = 0;
    uint16_t ubxPayloadLeft = 0;
    size_t ubxTextLen = 0;

    char lineBuf[120];
    size_t lineLen = 0;

    uint32_t tStart = millis();
    while (millis() - tStart < 2000UL) {
        while (Serial1.available() > 0) {
            uint8_t b = (uint8_t)Serial1.read();
            bytesRx++;
            _gps.charsProcessed++;
            s_gps.encode((char)b);

            // Detección de respuesta binaria UBX-MON-VER
            if (ubxState == 0 && b == 0xB5) { ubxState = 1; }
            else if (ubxState == 1) { ubxState = (b == 0x62) ? 2 : 0; }
            else if (ubxState == 2) { ubxState = (b == 0x0A) ? 3 : 0; }
            else if (ubxState == 3) { ubxState = (b == 0x04) ? 4 : 0; }
            else if (ubxState == 4) { ubxPayloadLeft = b; ubxState = 5; }
            else if (ubxState == 5) {
                ubxPayloadLeft |= ((uint16_t)b << 8);
                ubxReply = true;
                ubxState = 6;
                ubxTextLen = 0;
            } else if (ubxState == 6) {
                if (b >= 32 && b <= 126 && ubxTextLen < sizeof(replyInfo) - 1) {
                    replyInfo[ubxTextLen++] = (char)b;
                    replyInfo[ubxTextLen] = '\0';
                } else if (b == 0 && ubxTextLen > 0 && replyInfo[ubxTextLen - 1] != ' ' && ubxTextLen < sizeof(replyInfo) - 1) {
                    replyInfo[ubxTextLen++] = ' ';
                    replyInfo[ubxTextLen] = '\0';
                }
                if (ubxPayloadLeft > 0) ubxPayloadLeft--;
                if (ubxPayloadLeft == 0) ubxState = 0;
            }

            // Captura de líneas NMEA / respuestas ASCII
            if (b == '\n') {
                if (lineLen > 0) {
                    lineBuf[lineLen] = '\0';
                    if (lineBuf[0] == '$') {
                        nmeaLines++;
                        if (sampleNmea[0] == '\0') {
                            strlcpy(sampleNmea, lineBuf, sizeof(sampleNmea));
                        }
                        if (strstr(lineBuf, "TXT") || strstr(lineBuf, "PMTK705") || strstr(lineBuf, "PMTK001")) {
                            nmeaCmdReply = true;
                            strlcpy(replyInfo, lineBuf, sizeof(replyInfo));
                        }
                    }
                    lineLen = 0;
                }
            } else if (b != '\r' && b >= 32 && b <= 126 && lineLen < sizeof(lineBuf) - 1) {
                lineBuf[lineLen++] = (char)b;
            }
        }
        delay(5); // Cede CPU a AFSK_RX (prioridad 10)
    }

    Serial.printf("[GPS-TEST] En 2s -> Bytes RX:%lu | Tramas NMEA:%lu | Resp.UBX:%s | Resp.NMEA-CMD:%s\n",
                  (unsigned long)bytesRx, (unsigned long)nmeaLines,
                  ubxReply ? "SI" : "NO", nmeaCmdReply ? "SI" : "NO");
    if (sampleNmea[0] != '\0') {
        Serial.printf("[GPS-TEST] Ejemplo NMEA: %s\n", sampleNmea);
    }
    if (replyInfo[0] != '\0') {
        Serial.printf("[GPS-TEST] Respuesta comando: %s\n", replyInfo);
    }

    if (ubxReply || nmeaCmdReply) {
        emitEvent("[GPS-TEST] OK! GPS respondio a comandos (TX->RX y RX->TX OK).");
    } else if (bytesRx > 0) {
        emitEvent("[GPS-TEST] PARCIAL: Llegan %lu bytes (RX OK), pero sin respuesta a comandos. Revisa IO%d->GPS RX.",
                  (unsigned long)bytesRx, MT_GPS_TX_PIN);
    } else {
        emitEvent("[GPS-TEST] SIN DATOS en IO%d. Revisa 3.3V/5V, GND y que GPS TX vaya a IO%d.",
                  MT_GPS_RX_PIN, MT_GPS_RX_PIN);
    }
}

void AprsTracker::evaluateBeaconTimer() {
    // Si usa GPS y aún no tiene fix válido, no transmitir posición vieja automáticamente
    if (_cfg.useGps && !_gps.validFix) {
        return;
    }

    uint32_t now = millis();
    uint32_t elapsedSec = (now - _lastBeaconMs) / 1000UL;

    if (!_cfg.smartBeacon || !_cfg.useGps) {
        if (_cfg.beaconIntervalSec >= 30 && elapsedSec >= _cfg.beaconIntervalSec) {
            sendPositionBeacon(false);
        }
        return;
    }

    // SmartBeaconing™: intervalo dinámico según velocidad + disparo por giro (Corner Pegging)
    float spd = _gps.speedKmh;
    uint32_t targetRateSec = _cfg.sbSlowRateSec;

    if (spd >= _cfg.sbFastSpeedKmh) {
        targetRateSec = _cfg.sbFastRateSec;
    } else if (spd > _cfg.sbSlowSpeedKmh) {
        float ratio = (spd - _cfg.sbSlowSpeedKmh) / (float)(_cfg.sbFastSpeedKmh - _cfg.sbSlowSpeedKmh);
        targetRateSec = (uint32_t)(_cfg.sbSlowRateSec - ratio * (_cfg.sbSlowRateSec - _cfg.sbFastRateSec));
        if (targetRateSec < _cfg.sbFastRateSec) targetRateSec = _cfg.sbFastRateSec;
    }

    bool turnTriggered = false;
    if (spd > _cfg.sbSlowSpeedKmh && _hasLastBeaconCourse && elapsedSec >= 15) {
        int diff = abs((int)_gps.courseDeg - (int)_lastBeaconCourseDeg);
        if (diff > 180) diff = 360 - diff;
        if (diff >= _cfg.sbTurnAngleDeg) {
            turnTriggered = true;
        }
    }

    if (elapsedSec >= targetRateSec || turnTriggered) {
        sendPositionBeacon(false);
    }
}

void AprsTracker::encodeAx25Address(uint8_t* out7, const char* call, uint8_t ssid, bool isLast) {
    size_t len = strlen(call);
    for (int i = 0; i < 6; i++) {
        char c = (i < (int)len) ? (char)toupper((unsigned char)call[i]) : ' ';
        out7[i] = (uint8_t)(c << 1);
    }
    out7[6] = (uint8_t)(0x60 | ((ssid & 0x0F) << 1) | (isLast ? 0x01 : 0x00));
}

bool AprsTracker::buildAndSendUiFrame(const char* infoPayload, bool loopback) {
    uint8_t frame[MAX_AX25_FRAME_LEN];
    uint16_t pos = 0;

    bool hasP1 = (strlen(_cfg.path1) > 0);
    bool hasP2 = (strlen(_cfg.path2) > 0);

    encodeAx25Address(&frame[pos], _cfg.destCall, 0, false); pos += 7;
    encodeAx25Address(&frame[pos], _cfg.callsign, _cfg.ssid, !hasP1 && !hasP2); pos += 7;
    if (hasP1) { encodeAx25Address(&frame[pos], _cfg.path1, _cfg.path1Ssid, !hasP2); pos += 7; }
    if (hasP2) { encodeAx25Address(&frame[pos], _cfg.path2, _cfg.path2Ssid, true);   pos += 7; }

    frame[pos++] = 0x03; // Control: UI frame
    frame[pos++] = 0xF0; // PID: No Layer 3

    size_t infoLen = strlen(infoPayload);
    if (pos + infoLen + 2 >= MAX_AX25_FRAME_LEN) {
        infoLen = MAX_AX25_FRAME_LEN - pos - 3;
    }
    memcpy(&frame[pos], infoPayload, infoLen);
    pos += infoLen;

    return Modem.transmitFrame(frame, pos, loopback);
}

void AprsTracker::formatAprsLatLon(double lat, double lon, char* latOut, char* lonOut) const {
    char ns = (lat >= 0.0) ? 'N' : 'S';
    char ew = (lon >= 0.0) ? 'E' : 'W';
    double aLat = fabs(lat);
    double aLon = fabs(lon);
    int latDeg = (int)aLat;
    double latMin = (aLat - latDeg) * 60.0;
    int lonDeg = (int)aLon;
    double lonMin = (aLon - lonDeg) * 60.0;

    snprintf(latOut, 10, "%02d%05.2f%c", latDeg, latMin, ns);
    snprintf(lonOut, 11, "%03d%05.2f%c", lonDeg, lonMin, ew);
}

bool AprsTracker::sendPositionBeacon(bool loopback) {
    char latStr[10], lonStr[11];
    formatAprsLatLon(getCurrentLat(), getCurrentLon(), latStr, lonStr);

    int32_t altFeet = (int32_t)roundf(getCurrentAlt() * 3.28084f);
    if (altFeet < 0) altFeet = 0;

    char symT = _sosActive ? '\\' : _cfg.symbolTable;
    char symC = _sosActive ? '!'  : _cfg.symbolCode;

    char info[140];
    if (_cfg.useGps && _gps.validFix) {
        int spdKnots = (int)roundf(_gps.speedKmh / 1.852f);
        snprintf(info, sizeof(info), "!%s%c%s%c%03u/%03d/A=%06ld %s",
                 latStr, symT, lonStr, symC,
                 (unsigned)(_gps.courseDeg % 360), spdKnots, (long)altFeet,
                 _sosActive ? _cfg.sosText : _cfg.comment);
    } else {
        snprintf(info, sizeof(info), "!%s%c%s%c/A=%06ld %s",
                 latStr, symT, lonStr, symC, (long)altFeet,
                 _sosActive ? _cfg.sosText : _cfg.comment);
    }

    bool ok = buildAndSendUiFrame(info, loopback);
    if (ok) {
        _lastBeaconMs = millis();
        if (_gps.validFix) {
            _lastBeaconCourseDeg = _gps.courseDeg;
            _hasLastBeaconCourse = true;
        }
        emitEvent("[TX-BCN%s] %s", _sosActive ? "-SOS" : (loopback ? "-LOOP" : ""), info);
    }
    return ok;
}

bool AprsTracker::sendTextMessage(const char* destCall, const char* text, bool loopback) {
    if (!destCall || !text || strlen(destCall) < 2 || strlen(text) == 0) return false;

    char cleanDest[12];
    strlcpy(cleanDest, destCall, sizeof(cleanDest));
    for (int i = 0; cleanDest[i]; i++) cleanDest[i] = (char)toupper((unsigned char)cleanDest[i]);

    char msgId[6];
    snprintf(msgId, sizeof(msgId), "%02u", (unsigned)(_msgSeqCounter % 100));
    _msgSeqCounter++;

    char payload[110];
    snprintf(payload, sizeof(payload), ":%-9.9s:%s{%s", cleanDest, text, msgId);

    bool ok = buildAndSendUiFrame(payload, loopback);
    if (ok && !loopback) {
        strlcpy(_pendingDest, cleanDest, sizeof(_pendingDest));
        strlcpy(_pendingText, text, sizeof(_pendingText));
        strlcpy(_pendingId, msgId, sizeof(_pendingId));
        _pendingRetries = 1;
        _pendingNextTryMs = millis() + 20000UL;
        _pendingMsgActive = true;
        emitEvent("[TX-MSG id:%s] -> %s: %s", msgId, cleanDest, text);
    }
    return ok;
}

void AprsTracker::activateSos() {
    _sosActive = true;
    _lastSosBeaconMs = millis();
    emitEvent("[SOS] EMERGENCIA ACTIVADA! Emitiendo baliza de auxilio (\\!)...");
    sendPositionBeacon(false);
    if (_cfg.sosSendMsg && strlen(_cfg.sosMsgDest) >= 3) {
        sendTextMessage(_cfg.sosMsgDest, _cfg.sosText, false);
    }
}

void AprsTracker::cancelSos() {
    if (!_sosActive) return;
    _sosActive = false;
    emitEvent("[SOS] EMERGENCIA CANCELADA. Enviando baliza de normalizacion...");
    sendPositionBeacon(false);
}

void AprsTracker::processIncomingFrame(const RawAx25Frame& raw) {
    if (raw.len < 16) return;

    auto decodeCall = [](const uint8_t* ptr, char* out, size_t outSz) {
        char base[7];
        int len = 0;
        for (int i = 0; i < 6; i++) {
            char c = (char)(ptr[i] >> 1);
            if (c != ' ') base[len++] = c;
        }
        base[len] = '\0';
        uint8_t ssid = (ptr[6] >> 1) & 0x0F;
        if (ssid > 0) snprintf(out, outSz, "%s-%u", base, ssid);
        else          snprintf(out, outSz, "%s", base);
    };

    char dstCall[12], srcCall[12];
    decodeCall(&raw.data[0], dstCall, sizeof(dstCall));
    decodeCall(&raw.data[7], srcCall, sizeof(srcCall));

    uint16_t pos = 14;
    bool isLast = (raw.data[13] & 0x01) != 0;
    while (!isLast && pos + 7 <= raw.len) {
        isLast = (raw.data[pos + 6] & 0x01) != 0;
        pos += 7;
    }
    if (pos + 2 >= raw.len) return;
    if (raw.data[pos] != 0x03 || raw.data[pos + 1] != 0xF0) return;
    pos += 2;

    char info[160];
    uint16_t infoLen = raw.len - pos;
    if (infoLen >= sizeof(info)) infoLen = sizeof(info) - 1;
    memcpy(info, &raw.data[pos], infoLen);
    info[infoLen] = '\0';

    // Verificar si es un Mensaje APRS o ACK (":DESTINO  :cuerpo")
    if (info[0] == ':' && infoLen >= 11 && info[10] == ':') {
        char addressee[10];
        memcpy(addressee, &info[1], 9);
        addressee[9] = '\0';
        for (int i = 8; i >= 0 && addressee[i] == ' '; i--) addressee[i] = '\0';

        char myFull[12];
        formatMyFullCall(myFull, sizeof(myFull));
        bool forMe = (strcasecmp(addressee, myFull) == 0) || (strcasecmp(addressee, _cfg.callsign) == 0);

        const char* body = &info[11];
        if (forMe && strncmp(body, "ack", 3) == 0) {
            const char* ackId = body + 3;
            if (_pendingMsgActive && strcmp(ackId, _pendingId) == 0) {
                _pendingMsgActive = false;
            }
            emitEvent("[ACK] %s confirmo mensaje (id:%s)", srcCall, ackId);
            return;
        }

        if (forMe && strncmp(body, "rej", 3) != 0) {
            // Extraer texto y opcionalmente {msgId para responder ACK automático
            char msgBody[72];
            char rxId[8] = "";
            const char* brace = strchr(body, '{');
            if (brace) {
                size_t tLen = (size_t)(brace - body);
                if (tLen >= sizeof(msgBody)) tLen = sizeof(msgBody) - 1;
                memcpy(msgBody, body, tLen);
                msgBody[tLen] = '\0';
                strlcpy(rxId, brace + 1, sizeof(rxId));
            } else {
                strlcpy(msgBody, body, sizeof(msgBody));
            }

            emitEvent("[MSG] %s: %s", srcCall, msgBody);

            if (_cfg.autoAck && strlen(rxId) > 0 && strcasecmp(srcCall, myFull) != 0) {
                char ackPayload[32];
                snprintf(ackPayload, sizeof(ackPayload), ":%-9.9s:ack%s", srcCall, rxId);
                delay(150);
                buildAndSendUiFrame(ackPayload, false);
                emitEvent("[TX-ACK] -> %s (id:%s)", srcCall, rxId);
            }
            return;
        }
    }

    // Otros paquetes RF (balizas de estaciones cercanas)
    emitEvent("[RX] %s>%s: %s", srcCall, dstCall, info);
}
