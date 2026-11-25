#include "wifi_portal.h"
#include "aprs_tracker.h"
#include <WiFi.h>
#include <Update.h>

WifiPortal Portal;

void WifiPortal::start(uint32_t timeoutSec) {
    if (_active) {
        _lastActivityMs = millis();
        return;
    }

    _timeoutMs = timeoutSec * 1000UL;
    snprintf(_apSsid, sizeof(_apSsid), "MicroTracker-%s", Tracker.config().callsign);

    WiFi.mode(WIFI_AP);
    // En ESP32-C3 SuperMini con antena cerámica, 8.5 dBm da máxima estabilidad y bajo consumo
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.softAP(_apSsid);

    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/save", HTTP_POST, [this]() { handleSave(); });
    _server.on("/bcn", HTTP_GET, [this]() {
        _lastActivityMs = millis();
        Tracker.sendPositionBeacon(false);
        _server.sendHeader("Location", "/");
        _server.send(302, "text/plain", "Baliza enviada");
    });
    _server.on("/stop", HTTP_GET, [this]() {
        _server.send(200, "text/html", "<html><body style='background:#111;color:#0f0;font-family:sans-serif;text-align:center;padding:40px'><h2>WiFi apagado</h2><p>MicroTracker operando en modo RF + BLE.</p></body></html>");
        delay(300);
        stop();
    });
    _server.on("/update", HTTP_POST,
        [this]() {
            bool ok = !Update.hasError();
            _server.send(200, "text/plain", ok ? "OTA OK! Reiniciando..." : "ERROR OTA!");
            delay(500);
            if (ok) ESP.restart();
        },
        [this]() { handleOtaUpload(); }
    );

    _server.begin();
    _active = true;
    _lastActivityMs = millis();
    Serial.printf("[WIFI] Portal AP activo: SSID='%s' -> http://%s (auto-apagado en %lus)\n",
                  _apSsid, WiFi.softAPIP().toString().c_str(), (unsigned long)timeoutSec);
}

void WifiPortal::stop() {
    if (!_active) return;
    _server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    _active = false;
    Serial.println(F("[WIFI] Portal AP apagado. Modo RF + BLE puro activo."));
}

void WifiPortal::update() {
    if (!_active) return;
    _server.handleClient();
    if (millis() - _lastActivityMs >= _timeoutMs) {
        Serial.println(F("[WIFI] Timeout de inactividad. Apagando Portal AP..."));
        stop();
    }
}

void WifiPortal::handleRoot() {
    _lastActivityMs = millis();
    TrackerConfig& c = Tracker.config();
    const GpsStatus& g = Tracker.gps();

    String h;
    h.reserve(4600);
    h += F("<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
    h += F("<title>ESP32-C3 MicroTracker APRS</title><style>");
    h += F("body{font-family:system-ui,sans-serif;background:#0d131a;color:#e6eef7;margin:0;padding:14px}");
    h += F(".card{max-width:540px;margin:0 auto 14px;background:#16202b;border:1px solid #2a3a4d;border-radius:10px;padding:16px}");
    h += F("h1{margin:0 0 6px;font-size:19px;color:#38bdf8}h2{margin:14px 0 8px;font-size:15px;color:#fbbf24;border-bottom:1px solid #2a3a4d;padding-bottom:4px}");
    h += F("label{display:block;font-size:13px;color:#94a3b8;margin-top:8px}");
    h += F("input,select{width:100%;box-sizing:border-box;padding:9px;margin-top:3px;border-radius:6px;border:1px solid #334155;background:#0d131a;color:#fff;font-size:14px}");
    h += F(".row{display:grid;grid-template-columns:1fr 1fr;gap:10px}");
    h += F(".btn{width:100%;padding:12px;margin-top:14px;border:none;border-radius:8px;font-weight:700;font-size:15px;cursor:pointer}");
    h += F(".green{background:#22c55e;color:#052e16}.blue{background:#38bdf8;color:#082f49}.gray{background:#334155;color:#e2e8f0}");
    h += F(".stat{font-size:13px;color:#cbd5e1;background:#0d131a;padding:8px 10px;border-radius:6px;margin-top:8px}");
    h += F("</style></head><body><div class='card'>");
    h += F("<h1>ESP32-C3 MicroTracker APRS v");
    h += MT_FIRMWARE_VERSION;
    h += F("</h1><div class='stat'>GPS: <b>");
    h += g.validFix ? F("FIX OK") : F("Buscando...");
    h += F("</b> | Sats: "); h += String(g.satellites);
    h += F(" | Pos: "); h += String(Tracker.getCurrentLat(), 4) + ", " + String(Tracker.getCurrentLon(), 4);
    h += F("<br>TX: "); h += String(Modem.getTxPacketCount());
    h += F(" | RX: "); h += String(Modem.getRxPacketCount());
    h += F(" | AudioRX: "); h += String(Modem.getRxAudioLevel()) + "%</div>";

    h += F("<form method='POST' action='/save'>");
    h += F("<h2>1. Estación e Icono</h2><div class='row'>");
    h += "<div><label>Indicativo:</label><input name='call' maxlength='6' value='" + String(c.callsign) + "'></div>";
    h += "<div><label>SSID (0..15):</label><input type='number' name='ssid' min='0' max='15' value='" + String(c.ssid) + "'></div></div>";

    h += F("<div class='row'><div><label>Icono APRS:</label><select name='sym'>");
    String curSym = String(c.symbolTable) + String(c.symbolCode);
    h += "<option value='/>'" + String(curSym == "/>" ? " selected" : "") + ">Auto (/>)</option>";
    h += "<option value='/['" + String(curSym == "/[" ? " selected" : "") + ">Portátil / Caminante (/[)</option>";
    h += "<option value='/b'" + String(curSym == "/b" ? " selected" : "") + ">Bicicleta (/b)</option>";
    h += "<option value='/k'" + String(curSym == "/k" ? " selected" : "") + ">Camioneta (/k)</option>";
    h += "<option value='/<'" + String(curSym == "/<" ? " selected" : "") + ">Moto (/<)</option>";
    h += "<option value='/v'" + String(curSym == "/v" ? " selected" : "") + ">Van / Motorhome (/v)</option>";
    h += F("</select></div>");
    h += "<div><label>Ruta (Path):</label><input name='path' value='" + String(c.path1) + "-" + String(c.path1Ssid) + "," + String(c.path2) + "-" + String(c.path2Ssid) + "'></div></div>";
    h += "<label>Comentario:</label><input name='comm' maxlength='40' value='" + String(c.comment) + "'>";

    h += F("<h2>2. GPS y Baliza</h2><div class='row'>");
    h += "<div><label>Fuente Posición:</label><select name='useGps'><option value='1'" + String(c.useGps ? " selected" : "") + ">GPS Externo</option><option value='0'" + String(!c.useGps ? " selected" : "") + ">Posición Fija</option></select></div>";
    h += "<div><label>Baliza Automática:</label><select name='autoBcn'><option value='1'" + String(c.autoBeacon ? " selected" : "") + ">ACTIVADA</option><option value='0'" + String(!c.autoBeacon ? " selected" : "") + ">APAGADA</option></select></div></div>";

    h += "<div class='row'><div><label>SmartBeaconing:</label><select name='sbEn'><option value='1'" + String(c.smartBeacon ? " selected" : "") + ">ON (Velocidad+Giros)</option><option value='0'" + String(!c.smartBeacon ? " selected" : "") + ">OFF (Intervalo Fijo)</option></select></div>";
    h += "<div><label>Intervalo Fijo (seg):</label><input type='number' name='bcnInt' min='30' max='3600' value='" + String(c.beaconIntervalSec) + "'></div></div>";

    h += "<div class='row'><div><label>Latitud respaldo:</label><input name='lat' value='" + String(c.fixedLat, 5) + "'></div>";
    h += "<div><label>Longitud respaldo:</label><input name='lon' value='" + String(c.fixedLon, 5) + "'></div></div>";

    h += F("<h2>3. Radio VHF y Audio TX</h2><div class='row'>");
    h += "<div><label>Preset Radio:</label><select name='radio'><option value='0'" + String(c.radioPreset == 0 ? " selected" : "") + ">Baofeng (450ms)</option><option value='1'" + String(c.radioPreset == 1 ? " selected" : "") + ">Yaesu (250ms)</option><option value='2'" + String(c.radioPreset == 2 ? " selected" : "") + ">Personalizado</option></select></div>";
    h += "<div><label>Auto-ACK Mensajes:</label><select name='autoAck'><option value='1'" + String(c.autoAck ? " selected" : "") + ">SI</option><option value='0'" + String(!c.autoAck ? " selected" : "") + ">NO</option></select></div></div>";

    h += "<div class='row'><div><label>TX Delay (ms):</label><input type='number' name='txDelay' min='100' max='1000' value='" + String(c.txDelayMs) + "'></div>";
    h += "<div><label>Volumen TX (%):</label><input type='number' name='txVol' min='10' max='100' value='" + String(c.txVolume) + "'></div></div>";

    h += F("<h2>4. Emergencia SOS (Botón GPIO 4)</h2><div class='row'>");
    h += "<div><label>Intervalo SOS (seg):</label><input type='number' name='sosInt' min='30' max='300' value='" + String(c.sosIntervalSec) + "'></div>";
    h += "<div><label>Mensaje Directo SOS:</label><select name='sosMsg'><option value='0'" + String(!c.sosSendMsg ? " selected" : "") + ">Solo Baliza \\!</option><option value='1'" + String(c.sosSendMsg ? " selected" : "") + ">Baliza + Mensaje</option></select></div></div>";
    h += "<label>Destinatario SOS (Callsign-SSID):</label><input name='sosDst' maxlength='10' value='" + String(c.sosMsgDest) + "'>";
    h += "<label>Texto SOS:</label><input name='sosTxt' maxlength='34' value='" + String(c.sosText) + "'>";

    h += F("<button class='btn green' type='submit'>GUARDAR Y APAGAR WIFI</button></form>");
    h += F("<div class='row'><a href='/bcn'><button class='btn blue' type='button'>TX BALIZA AHORA</button></a>");
    h += F("<a href='/stop'><button class='btn gray' type='button'>CERRAR PORTAL</button></a></div></div>");

    h += F("<div class='card'><h2>Actualización Firmware (Web OTA)</h2>");
    h += F("<form method='POST' action='/update' enctype='multipart/form-data'>");
    h += F("<input type='file' name='update' accept='.bin'><button class='btn blue' type='submit'>SUBIR FIRMWARE .BIN</button></form></div></body></html>");

    _server.send(200, "text/html", h);
}

void WifiPortal::handleSave() {
    _lastActivityMs = millis();
    TrackerConfig& c = Tracker.config();

    if (_server.hasArg("call")) {
        strlcpy(c.callsign, _server.arg("call").c_str(), sizeof(c.callsign));
        for (int i = 0; c.callsign[i]; i++) c.callsign[i] = (char)toupper((unsigned char)c.callsign[i]);
    }
    if (_server.hasArg("ssid"))    c.ssid = (uint8_t)constrain(_server.arg("ssid").toInt(), 0, 15);
    if (_server.hasArg("sym")) {
        String s = _server.arg("sym");
        if (s.length() >= 2) { c.symbolTable = s[0]; c.symbolCode = s[1]; }
    }
    if (_server.hasArg("comm"))    strlcpy(c.comment, _server.arg("comm").c_str(), sizeof(c.comment));
    if (_server.hasArg("useGps"))  c.useGps = (_server.arg("useGps").toInt() == 1);
    if (_server.hasArg("autoBcn")) c.autoBeacon = (_server.arg("autoBcn").toInt() == 1);
    if (_server.hasArg("sbEn"))    c.smartBeacon = (_server.arg("sbEn").toInt() == 1);
    if (_server.hasArg("bcnInt"))  c.beaconIntervalSec = (uint16_t)constrain(_server.arg("bcnInt").toInt(), 30, 3600);
    if (_server.hasArg("lat"))     c.fixedLat = _server.arg("lat").toDouble();
    if (_server.hasArg("lon"))     c.fixedLon = _server.arg("lon").toDouble();

    if (_server.hasArg("radio")) {
        uint8_t r = (uint8_t)constrain(_server.arg("radio").toInt(), 0, 2);
        if (r != RADIO_CUSTOM && r != c.radioPreset) {
            Tracker.applyRadioPreset((RadioPreset)r);
        } else {
            c.radioPreset = r;
        }
    }
    if (_server.hasArg("txDelay")) c.txDelayMs = (uint16_t)constrain(_server.arg("txDelay").toInt(), 100, 1000);
    if (_server.hasArg("txVol"))   c.txVolume  = (uint8_t)constrain(_server.arg("txVol").toInt(), 10, 100);
    if (_server.hasArg("autoAck")) c.autoAck   = (_server.arg("autoAck").toInt() == 1);

    if (_server.hasArg("sosInt"))  c.sosIntervalSec = (uint16_t)constrain(_server.arg("sosInt").toInt(), 30, 300);
    if (_server.hasArg("sosMsg"))  c.sosSendMsg = (_server.arg("sosMsg").toInt() == 1);
    if (_server.hasArg("sosDst")) {
        strlcpy(c.sosMsgDest, _server.arg("sosDst").c_str(), sizeof(c.sosMsgDest));
        for (int i = 0; c.sosMsgDest[i]; i++) c.sosMsgDest[i] = (char)toupper((unsigned char)c.sosMsgDest[i]);
    }
    if (_server.hasArg("sosTxt"))  strlcpy(c.sosText, _server.arg("sosTxt").c_str(), sizeof(c.sosText));

    Tracker.saveConfig();

    _server.send(200, "text/html",
        "<html><body style='background:#0d131a;color:#22c55e;font-family:sans-serif;text-align:center;padding:40px'>"
        "<h2>Configuración Guardada en Flash NVS!</h2>"
        "<p style='color:#cbd5e1'>Apagando WiFi para operar en modo RF + BLE de bajo consumo...</p></body></html>");
    delay(600);
    stop();
}

void WifiPortal::handleOtaUpload() {
    _lastActivityMs = millis();
    HTTPUpload& upload = _server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA] Iniciando carga: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) Serial.printf("[OTA] OK: %u bytes\n", upload.totalSize);
        else Update.printError(Serial);
    }
}
