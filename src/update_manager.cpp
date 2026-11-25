#include "update_manager.h"
#include "afsk_modem.h"
#include "ax25_aprs.h"
#include "igate_manager.h"
#include "sensor_manager.h"
#include "ui_manager.h"
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include <Update.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

UpdateManager Updater;

// Instancia VSPI dedicada para el lector MicroSD de la CYD (no interfiere con HSPI del TFT)
static SPIClass s_sdSpi(VSPI);

UpdateManager::UpdateManager() : _server(80) {
    memset(_binFiles, 0, sizeof(_binFiles));
}

void UpdateManager::setStatusMsg(const char* msg) {
    if (!msg) return;
    strlcpy(_statusMsg, msg, sizeof(_statusMsg));
}

int UpdateManager::compareSemVer(const String& vA, const String& vB) {
    int majA = 0, minA = 0, patA = 0;
    int majB = 0, minB = 0, patB = 0;
    const char* a = vA.c_str();
    const char* b = vB.c_str();
    if (*a == 'v' || *a == 'V') a++;
    if (*b == 'v' || *b == 'V') b++;
    sscanf(a, "%d.%d.%d", &majA, &minA, &patA);
    sscanf(b, "%d.%d.%d", &majB, &minB, &patB);
    if (majA != majB) return majA - majB;
    if (minA != minB) return minA - minB;
    return patA - patB;
}

void UpdateManager::drawUpdateProgress(TFT_eSPI& tft, const char* title, const char* subtitle, int pct, uint16_t barColor) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    static int s_lastPct = -2;
    static char s_lastTitle[48] = "";

    bool fullDraw = (pct == 0 || strcmp(s_lastTitle, title ? title : "") != 0);
    if (fullDraw) {
        strlcpy(s_lastTitle, title ? title : "", sizeof(s_lastTitle));
        tft.fillScreen(0x0842);
        tft.fillRoundRect(14, 24, 292, 192, 8, 0x10E4);
        tft.drawRoundRect(14, 24, 292, 192, 8, 0x05FF);

        tft.setTextColor(0x05FF, 0x10E4);
        tft.drawCentreString(title ? title : "ACTUALIZANDO FIRMWARE", 160, 40, 2);

        if (subtitle && strlen(subtitle) > 0) {
            tft.setTextColor(0xFFFF, 0x10E4);
            tft.drawCentreString(subtitle, 160, 68, 2);
        }

        tft.drawRoundRect(34, 110, 252, 28, 5, 0xFFFF);
        tft.fillRect(36, 112, 248, 24, 0x0842);
    }

    int barW = (pct * 244) / 100;
    if (barW > 0) {
        tft.fillRoundRect(38, 114, barW, 20, 3, barColor);
    }

    char pctBuf[36];
    snprintf(pctBuf, sizeof(pctBuf), "Progreso Flash: %d%%", pct);
    tft.fillRect(40, 152, 240, 24, 0x10E4);
    tft.setTextColor(0xFFE0, 0x10E4);
    tft.drawCentreString(pctBuf, 160, 155, 2);

    s_lastPct = pct;
}

bool UpdateManager::performStreamUpdate(Stream& stream, size_t totalSize, const char* title, const char* subtitle, TFT_eSPI& tft) {
    if (totalSize == 0) {
        setStatusMsg("Error: Firmware de 0 bytes");
        return false;
    }

    Serial.printf("[UPDATE] Iniciando grabacion Flash (%u bytes) - %s\n", (unsigned)totalSize, title);
    drawUpdateProgress(tft, title, subtitle, 0);

    if (!Update.begin(totalSize, U_FLASH)) {
        Serial.printf("[UPDATE ERROR] Update.begin fallo: %s\n", Update.errorString());
        setStatusMsg("Error: Particion OTA insuficiente");
        drawUpdateProgress(tft, title, "Error: Sin espacio en Flash OTA", 0, 0xF800);
        delay(2000);
        return false;
    }

    uint8_t buffer[1024];
    size_t written = 0;
    int lastPercent = -1;
    uint32_t lastDataMs = millis();

    while (written < totalSize) {
        size_t available = stream.available();
        if (available > 0) {
            size_t toRead = (available < sizeof(buffer)) ? available : sizeof(buffer);
            size_t bytesRead = stream.readBytes(buffer, toRead);
            if (bytesRead > 0) {
                if (Update.write(buffer, bytesRead) != bytesRead) {
                    Serial.println(F("[UPDATE ERROR] Fallo de escritura en Flash"));
                    break;
                }
                written += bytesRead;
                lastDataMs = millis();

                int percent = (int)((written * 100ULL) / totalSize);
                if (percent != lastPercent) {
                    lastPercent = percent;
                    drawUpdateProgress(tft, title, subtitle, percent);
                    Serial.printf("[UPDATE] Progreso: %d%%\n", percent);
                }
            }
        } else {
            if (millis() - lastDataMs > 15000UL) {
                Serial.println(F("[UPDATE ERROR] Timeout leyendo stream de firmware"));
                break;
            }
            delay(5);
        }
    }

    if (written == totalSize && Update.end() && Update.isFinished()) {
        Serial.println(F("[UPDATE] Grabacion en Flash completada con exito!"));
        setStatusMsg("Actualizacion OK! Reiniciando...");
        drawUpdateProgress(tft, "ACTUALIZACION EXITOSA!", "Reiniciando ESP32 CYD...", 100, 0x07E0);
        return true;
    }

    Serial.printf("[UPDATE ERROR] Fallo al finalizar: %s\n", Update.errorString());
    Update.abort();
    setStatusMsg("Error al verificar firma/CRC");
    drawUpdateProgress(tft, "ERROR DE ACTUALIZACION", "Fallo al verificar Firmware", 100, 0xF800);
    delay(2200);
    return false;
}

bool UpdateManager::checkAndApplySdUpdateOnBoot(TFT_eSPI& tft) {
    pinMode(SD_PIN_MISO, INPUT_PULLUP);
    pinMode(SD_PIN_CS, OUTPUT);
    digitalWrite(SD_PIN_CS, HIGH);
    delay(30);

    s_sdSpi.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);

    if (!SD.begin(SD_PIN_CS, s_sdSpi, 4000000)) {
        Serial.println(F("[SD BOOT] Sin tarjeta MicroSD o no formateada en FAT32."));
        s_sdSpi.end();
        return false;
    }

    static const char* const CANDIDATE_FILES[] = {
        "/update.bin",
        "/firmware.bin",
        "/update.bin.bin",
        "/firmware.bin.bin",
        "/UPDATE.BIN",
        "/FIRMWARE.BIN"
    };

    String foundPath = "";
    for (const char* path : CANDIDATE_FILES) {
        if (SD.exists(path)) {
            foundPath = String(path);
            break;
        }
    }

    if (foundPath.length() == 0) {
        SD.end();
        s_sdSpi.end();
        return false;
    }

    File updateFile = SD.open(foundPath.c_str(), FILE_READ);
    if (!updateFile || updateFile.isDirectory()) {
        SD.end();
        s_sdSpi.end();
        return false;
    }

    size_t fileSize = updateFile.size();
    if (fileSize < 65536) {
        Serial.printf("[SD BOOT] Archivo %s demasiado chico (%u B), ignorando.\n", foundPath.c_str(), (unsigned)fileSize);
        updateFile.close();
        SD.end();
        s_sdSpi.end();
        return false;
    }

    Serial.printf("[SD BOOT] Firmware %s detectado (%u bytes)! Actualizando...\n", foundPath.c_str(), (unsigned)fileSize);
    bool ok = performStreamUpdate(updateFile, fileSize, "ACTUALIZANDO DESDE MICROSD", foundPath.c_str(), tft);
    updateFile.close();

    if (ok) {
        if (SD.exists("/update.bak")) {
            SD.remove("/update.bak");
        }
        if (!SD.rename(foundPath.c_str(), "/update.bak")) {
            SD.remove(foundPath.c_str());
        }
        SD.end();
        s_sdSpi.end();
        delay(1500);
        ESP.restart();
        return true;
    }

    SD.end();
    s_sdSpi.end();
    return false;
}

bool UpdateManager::scanSdBinaries() {
    _binCount = 0;
    _sdMounted = false;
    _cardSizeMb = 0;

    pinMode(SD_PIN_MISO, INPUT_PULLUP);
    pinMode(SD_PIN_CS, OUTPUT);
    digitalWrite(SD_PIN_CS, HIGH);

    s_sdSpi.end();
    s_sdSpi.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);

    if (!SD.begin(SD_PIN_CS, s_sdSpi, 8000000)) {
        setStatusMsg("Sin tarjeta MicroSD detectada");
        s_sdSpi.end();
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        SD.end();
        s_sdSpi.end();
        setStatusMsg("Tarjeta MicroSD no valida");
        return false;
    }

    _sdMounted = true;
    _cardSizeMb = (uint32_t)(SD.cardSize() / (1024ULL * 1024ULL));

    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        SD.end();
        s_sdSpi.end();
        setStatusMsg("Error al abrir raiz de SD");
        return false;
    }

    File file = root.openNextFile();
    while (file && _binCount < MAX_SD_BIN_FILES) {
        if (!file.isDirectory()) {
            String fname = String(file.name());
            String lower = fname;
            lower.toLowerCase();
            if (lower.endsWith(".bin") && file.size() > 65536) {
                String fullPath = fname.startsWith("/") ? fname : ("/" + fname);
                strlcpy(_binFiles[_binCount].path, fullPath.c_str(), sizeof(_binFiles[_binCount].path));
                _binFiles[_binCount].sizeBytes = (uint32_t)file.size();
                _binCount++;
            }
        }
        file = root.openNextFile();
    }
    root.close();
    SD.end();
    s_sdSpi.end();

    if (_binCount > 0) {
        snprintf(_statusMsg, sizeof(_statusMsg), "SD %luMB: %u archivo(s) .bin listo(s)",
                 (unsigned long)_cardSizeMb, _binCount);
    } else {
        snprintf(_statusMsg, sizeof(_statusMsg), "SD %luMB OK (Sin .bin en raiz)",
                 (unsigned long)_cardSizeMb);
    }
    return true;
}

bool UpdateManager::installSdBinary(const char* binPath, TFT_eSPI& tft) {
    if (!binPath || strlen(binPath) == 0) return false;

    pinMode(SD_PIN_MISO, INPUT_PULLUP);
    pinMode(SD_PIN_CS, OUTPUT);
    digitalWrite(SD_PIN_CS, HIGH);

    s_sdSpi.end();
    s_sdSpi.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);

    if (!SD.begin(SD_PIN_CS, s_sdSpi, 8000000)) {
        setStatusMsg("Fallo al montar SD para flashear");
        s_sdSpi.end();
        return false;
    }

    File updateFile = SD.open(binPath, FILE_READ);
    if (!updateFile || updateFile.isDirectory()) {
        SD.end();
        s_sdSpi.end();
        setStatusMsg("No se pudo abrir archivo .bin");
        return false;
    }

    size_t fileSize = updateFile.size();
    bool ok = performStreamUpdate(updateFile, fileSize, "INSTALANDO FIRMWARE SD", binPath, tft);
    updateFile.close();

    if (ok) {
        // Si se llamaba update.bin, renombrarlo a update.bak para evitar reflasheo en el arranque
        String lower = String(binPath);
        lower.toLowerCase();
        if (lower == "/update.bin" || lower == "/firmware.bin") {
            if (SD.exists("/update.bak")) SD.remove("/update.bak");
            SD.rename(binPath, "/update.bak");
        }
        SD.end();
        s_sdSpi.end();
        delay(1500);
        ESP.restart();
        return true;
    }

    SD.end();
    s_sdSpi.end();
    return false;
}

bool UpdateManager::checkAndApplyWebOta(const char* urlOrServer, TFT_eSPI& tft) {
    if (!urlOrServer || strlen(urlOrServer) < 7) {
        setStatusMsg("Configure URL OTA o use Portal Web");
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        setStatusMsg("Conectando WiFi para OTA...");
        Igate.connectWifi();
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - t0 < 7000UL)) {
            delay(150);
        }
        if (WiFi.status() != WL_CONNECTED) {
            setStatusMsg("Error: Sin conexion WiFi para OTA");
            return false;
        }
    }

    String targetUrl = String(urlOrServer);
    targetUrl.trim();
    String lowerUrl = targetUrl;
    lowerUrl.toLowerCase();

    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    secureClient.setTimeout(10000);
    WiFiClient plainClient;
    plainClient.setTimeout(10000);

    String binUrl = targetUrl;
    String newVersion = "OTA";

    // Si la URL NO termina en .bin, consultar manifest JSON (estilo mpviewer)
    if (!lowerUrl.endsWith(".bin")) {
        String checkUrl = targetUrl;
        if (!lowerUrl.endsWith(".json") && checkUrl.indexOf("/api/") < 0) {
            if (checkUrl.endsWith("/")) checkUrl.remove(checkUrl.length() - 1);
            checkUrl += "/api/firmware/check?current=" + String(FIRMWARE_VERSION);
        }

        setStatusMsg("Consultando servidor OTA...");
        HTTPClient http;
        bool begun = checkUrl.startsWith("https://")
                     ? http.begin(secureClient, checkUrl)
                     : http.begin(plainClient, checkUrl);

        if (!begun) {
            setStatusMsg("URL de servidor OTA invalida");
            return false;
        }

        int code = http.GET();
        if (code != 200) {
            http.end();
            snprintf(_statusMsg, sizeof(_statusMsg), "Servidor OTA HTTP %d", code);
            return false;
        }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getStream());
        http.end();

        if (err) {
            setStatusMsg("Respuesta JSON OTA invalida");
            return false;
        }

        if (doc["update_available"].is<bool>() && !doc["update_available"].as<bool>()) {
            snprintf(_statusMsg, sizeof(_statusMsg), "Ya tienes la ultima version (v%s)", FIRMWARE_VERSION);
            return false;
        }

        newVersion = doc["version"] | "nueva";
        binUrl = doc["url"] | "";

        if (binUrl.length() == 0) {
            setStatusMsg("Manifest OTA sin campo 'url'");
            return false;
        }

        if (compareSemVer(newVersion, String(FIRMWARE_VERSION)) <= 0) {
            snprintf(_statusMsg, sizeof(_statusMsg), "Actual v%s >= Remota v%s", FIRMWARE_VERSION, newVersion.c_str());
            return false;
        }
    }

    Serial.printf("[WEB OTA] Descargando binario desde: %s\n", binUrl.c_str());
    setStatusMsg("Descargando firmware OTA...");

    HTTPClient otaHttp;
    otaHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    bool binBegun = binUrl.startsWith("https://")
                    ? otaHttp.begin(secureClient, binUrl)
                    : otaHttp.begin(plainClient, binUrl);

    if (!binBegun) {
        setStatusMsg("Error al iniciar descarga .bin");
        return false;
    }

    int binCode = otaHttp.GET();
    if (binCode != 200) {
        otaHttp.end();
        snprintf(_statusMsg, sizeof(_statusMsg), "Error HTTP %d descargando .bin", binCode);
        return false;
    }

    int totalSize = otaHttp.getSize();
    if (totalSize <= 65536) {
        otaHttp.end();
        setStatusMsg("Tamano de binario OTA invalido");
        return false;
    }

    String sub = "Version: " + newVersion;
    bool ok = performStreamUpdate(otaHttp.getStream(), (size_t)totalSize, "ACTUALIZACION WEB OTA", sub.c_str(), tft);
    otaHttp.end();

    if (ok) {
        delay(1500);
        ESP.restart();
        return true;
    }
    return false;
}

void UpdateManager::begin() {
    const AprsConfig& cfg = Aprs.config();
    if ((cfg.stationMode == STATION_MODE_FIXED_IGATE || cfg.igateEnabled) && strlen(cfg.wifiSsid) > 0) {
        // El servidor Web / OTA se activará automáticamente en cuanto conecte el WiFi
    }
}

void UpdateManager::startWebPortal(bool forceAccessPoint) {
    if (forceAccessPoint || WiFi.status() != WL_CONNECTED) {
        WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
        WiFi.mode(WIFI_AP_STA);
        WiFi.setTxPower(WIFI_POWER_15dBm);
        char apName[28];
        snprintf(apName, sizeof(apName), "CYD-APRS-%s", Aprs.config().callsign);
        WiFi.softAP(apName);
        _apActive = true;
        Serial.printf("[WEB OTA] Punto de Acceso WiFi iniciado: SSID='%s' -> http://%s\n",
                      apName, WiFi.softAPIP().toString().c_str());
    }

    if (!_portalRunning) {
        setupWebRoutes();
        _server.begin();
        _portalRunning = true;
    }

    if (!_arduinoOtaReady && (WiFi.status() == WL_CONNECTED || _apActive)) {
        ArduinoOTA.setHostname("esp32-cyd-aprs");
        ArduinoOTA.begin();
        _arduinoOtaReady = true;
    }

    snprintf(_statusMsg, sizeof(_statusMsg), "Web OTA activo: http://%s", getPortalAddress().c_str());
}

void UpdateManager::stopWebPortal() {
    if (_portalRunning) {
        _server.stop();
        _portalRunning = false;
    }
    if (_apActive) {
        WiFi.softAPdisconnect(true);
        _apActive = false;
    }
    setStatusMsg("Portal Web OTA detenido");
}

String UpdateManager::getPortalAddress() const {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    if (_apActive) {
        return WiFi.softAPIP().toString();
    }
    return String("192.168.4.1");
}

void UpdateManager::update() {
    // Auto-iniciar portal Web local en modo Estación Fija + IGate cuando conecte el WiFi
    if (!_portalRunning && WiFi.status() == WL_CONNECTED) {
        startWebPortal(false);
    }

    if (_portalRunning) {
        _server.handleClient();
    }
    if (_arduinoOtaReady) {
        ArduinoOTA.handle();
    }
}

void UpdateManager::setupWebRoutes() {
    _server.on("/", HTTP_GET, [this]() { handleHttpRoot(); });
    _server.on("/save", HTTP_POST, [this]() { handleHttpSaveConfig(); });
    _server.on("/update", HTTP_POST,
               [this]() { handleHttpOtaFinish(); },
               [this]() { handleHttpOtaUpload(); });
}

void UpdateManager::handleHttpRoot() {
    AprsConfig& c = Aprs.config();
    uint16_t autoPass = IgateManager::calcPasscode(c.callsign);

    String html = F("<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'>"
                    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                    "<title>ESP32 CYD APRS — Config & OTA</title>"
                    "<style>"
                    "body{font-family:system-ui,sans-serif;background:#0b132b;color:#f8fafc;margin:0;padding:16px;display:flex;justify-content:center;}"
                    ".wrap{max-width:520px;width:100%;}"
                    ".card{background:#1c2541;border:1px solid #3a506b;border-radius:12px;padding:18px;margin-bottom:16px;}"
                    "h1{font-size:20px;color:#5bc0be;margin:0 0 6px 0;}"
                    "h2{font-size:15px;color:#fde047;margin:0 0 12px 0;border-bottom:1px solid #3a506b;padding-bottom:6px;}"
                    "label{display:block;font-size:12px;color:#cbd5e1;margin:10px 0 4px;font-weight:600;}"
                    "input,select{width:100%;box-sizing:border-box;padding:9px 10px;border-radius:6px;border:1px solid #475569;background:#0f172a;color:#fff;font-size:14px;}"
                    ".row{display:flex;gap:10px;}.row>div{flex:1;}"
                    ".btn{width:100%;margin-top:14px;padding:12px;border:none;border-radius:8px;background:#10b981;color:#042f2e;font-weight:700;font-size:15px;cursor:pointer;}"
                    ".btn-ota{background:#38bdf8;color:#082f49;}"
                    ".badge{display:inline-block;padding:3px 8px;border-radius:6px;font-size:12px;background:#0f172a;color:#5bc0be;border:1px solid #3a506b;margin-right:6px;}"
                    "</style></head><body><div class='wrap'>");

    html += "<div class='card'><h1>ESP32-2432S024C APRS <span class='badge'>v" + String(FIRMWARE_VERSION) + "</span></h1>";
    html += "<p style='font-size:12px;color:#94a3b8;margin:4px 0 10px;'>Tracker VHF/UHF 1200 bps + Estación Fija + IGate APRS-IS</p>";
    html += "<div><span class='badge'>IGate: " + String(Igate.getStateText()) + "</span>";
    html += "<span class='badge'>RF->IS: " + String(Igate.getRfToIsCount()) + " pkts</span>";
    html += "<span class='badge'>Passcode: " + String(Igate.getEffectivePasscode()) + "</span></div></div>";

    html += "<form class='card' action='/save' method='POST'>";
    html += "<h2>1. Modelo de Placa, Pinout y Estación</h2>";
    html += "<div class='row'><div><label>Modelo de Placa CYD:</label><select name='brdMod'>";
    html += "<option value='0'" + String(c.boardModel == BOARD_MODEL_ILI9341 ? " selected" : "") + ">Modelo ILI9341 (IO35, IO22, IO21, IO27)</option>";
    html += "<option value='1'" + String(c.boardModel == BOARD_MODEL_ST7789 ? " selected" : "") + ">Modelo ST7789 1-USB (IO35, IO22, IO21, IO4, IO16, IO17)</option>";
    html += "</select></div>";
    html += "<div><label>Inversión Color Pantalla:</label><select name='dispInv'>";
    html += "<option value='0'" + String(!c.displayInvert ? " selected" : "") + ">Normal (TFT_INVOFF)</option>";
    html += "<option value='1'" + String(c.displayInvert ? " selected" : "") + ">Invertido (TFT_INVON - Variantes ST7789)</option>";
    html += "</select></div></div>";

    html += "<div><label>Pin Audio TX (Conector + IO26):</label><select name='txPin'>";
    html += "<option value='27'" + String(c.txAudioPin == 27 ? " selected" : "") + ">GPIO 27 (Conector en ILI9341 con BL=21)</option>";
    html += "<option value='21'" + String(c.txAudioPin == 21 ? " selected" : "") + ">GPIO 21 (Conector en ILI9341 con BL=27)</option>";
    html += "<option value='4'" + String(c.txAudioPin == 4 ? " selected" : "") + ">GPIO 4 (Conector en ST7789, deja GPS 16/17 libre)</option>";
    html += "<option value='17'" + String(c.txAudioPin == 17 ? " selected" : "") + ">GPIO 17 (Conector P3 en ST7789 sin GPS)</option>";
    html += "<option value='26'" + String(c.txAudioPin == 26 ? " selected" : "") + ">GPIO 26 (Solo conector SPK P4)</option>";
    html += "</select></div>";

    html += "<label>Rol del Equipo:</label><select name='mode'>";
    html += "<option value='0'" + String(c.stationMode == STATION_MODE_TRACKER ? " selected" : "") + ">Modo Tracker (Móvil / Portátil GPS + RF)</option>";
    html += "<option value='1'" + String(c.stationMode == STATION_MODE_FIXED_IGATE ? " selected" : "") + ">Modo Estación Fija + IGate (RF -> APRS-IS)</option>";
    html += "<option value='2'" + String(c.stationMode == STATION_MODE_FIXED_RF ? " selected" : "") + ">Modo Estación Fija Solo RF / Digipeater</option>";
    html += "</select>";

    html += "<div class='row'><div><label>Indicativo:</label><input name='call' maxlength='6' value='" + String(c.callsign) + "' required></div>";
    html += "<div><label>SSID (0-15):</label><input type='number' name='ssid' min='0' max='15' value='" + String(c.ssid) + "'></div></div>";

    html += "<div class='row'><div><label>Posición (GPS excluye Sensor):</label><select name='posMode'>";
    html += "<option value='0'" + String(c.posMode == POS_MODE_FIXED ? " selected" : "") + ">Coordenadas Fijas</option>";
    html += "<option value='1'" + String(c.posMode == POS_MODE_GPS ? " selected" : "") + ">GPS Externo UART (Apaga Sensor)</option></select></div>";
    html += "<div><label>Sensor Clima I2C (Solo Pos Fija):</label><select name='snsMod'>";
    html += "<option value='5'" + String(c.sensorModel != SENSOR_BMP_BME280 ? " selected" : "") + ">Apagado</option>";
    html += "<option value='1'" + String(c.sensorModel == SENSOR_BMP_BME280 ? " selected" : "") + ">BME280 / BMP280 (I2C)</option></select></div></div>";

    html += "<div class='row'><div><label>Intervalo Baliza (s):</label><input type='number' name='bcnInt' min='30' max='3600' value='" + String(c.beaconIntervalSec) + "'></div>";
    html += "<div><label>Comentario APRS:</label><input name='comm' maxlength='42' value='" + String(c.comment) + "'></div></div>";

    html += "<div class='row'><div><label>Latitud Fija (Sur -):</label><input name='lat' value='" + String(c.fixedLat, 5) + "'></div>";
    html += "<div><label>Longitud Fija (Oeste -):</label><input name='lon' value='" + String(c.fixedLon, 5) + "'></div></div>";

    html += "<h2 style='margin-top:18px;'>2. Red WiFi e IGate APRS-IS</h2>";
    html += "<div class='row'><div><label>WiFi SSID:</label><input name='wssid' maxlength='32' value='" + String(c.wifiSsid) + "'></div>";
    html += "<div><label>WiFi Clave:</label><input type='password' name='wpass' maxlength='64' value='" + String(c.wifiPass) + "'></div></div>";

    html += "<div class='row'><div><label>Servidor APRS-IS:</label><input name='isSrv' value='" + String(c.aprsIsServer) + "'></div>";
    html += "<div><label>Puerto APRS-IS:</label><input type='number' name='isPort' value='" + String(c.aprsIsPort) + "'></div></div>";

    html += "<div class='row'><div><label>Passcode APRS-IS (-1 = Auto: " + String(autoPass) + "):</label><input type='number' name='isPass' value='" + String(c.aprsIsPasscode) + "'></div>";
    html += "<div><label>Salida de Baliza en IGate:</label><select name='igBcn'>";
    html += "<option value='0'" + String(c.igateBeaconMode == IGATE_BCN_BOTH ? " selected" : "") + ">RF + APRS-IS</option>";
    html += "<option value='1'" + String(c.igateBeaconMode == IGATE_BCN_IS_ONLY ? " selected" : "") + ">Solo APRS-IS (Internet)</option>";
    html += "<option value='2'" + String(c.igateBeaconMode == IGATE_BCN_RF_ONLY ? " selected" : "") + ">Solo RF (VHF/UHF)</option></select></div></div>";

    html += "<label>URL Servidor / Binario OTA Remoto (Opcional):</label><input name='otaUrl' maxlength='94' value='" + String(c.otaServerUrl) + "'>";

    html += "<h2 style='margin-top:18px;'>3. Aviso de Emergencia (SOS / Auxilio) - Solo Tracker</h2>";
    html += "<div class='row'><div><label>Intervalo Baliza SOS (s):</label><select name='emgInt'>";
    html += "<option value='60'" + String(c.emergIntervalSec == 60 ? " selected" : "") + ">60 segundos</option>";
    html += "<option value='120'" + String(c.emergIntervalSec == 120 ? " selected" : "") + ">120 segundos (Recomendado)</option>";
    html += "<option value='180'" + String(c.emergIntervalSec == 180 ? " selected" : "") + ">180 segundos</option></select></div>";
    html += "<div><label>Enviar Mensaje APRS Directo:</label><select name='emgMsg'>";
    html += "<option value='0'" + String(!c.emergSendDirectMsg ? " selected" : "") + ">No</option>";
    html += "<option value='1'" + String(c.emergSendDirectMsg ? " selected" : "") + ">Si</option></select></div></div>";

    html += "<div class='row'><div><label>Destinatario Mensaje SOS (Callsign):</label><input name='emgDst' maxlength='9' value='" + String(c.emergMsgDest) + "'></div>";
    html += "<div><label>Alerta Sonora Parlante (IO26):</label><select name='emgSnd'>";
    html += "<option value='0'" + String(!c.emergSoundAlert ? " selected" : "") + ">Desactivada (Silencioso)</option>";
    html += "<option value='1'" + String(c.emergSoundAlert ? " selected" : "") + ">Activada (Sirena cada 15s)</option></select></div></div>";

    html += "<button class='btn' type='submit'>GUARDAR CONFIGURACIÓN EN FLASH NVS</button></form>";

    html += "<form class='card' method='POST' action='/update' enctype='multipart/form-data'>";
    html += "<h2>4. Actualización de Firmware Web OTA (.bin)</h2>";
    html += "<label>Selecciona el archivo compilado firmware.bin:</label>";
    html += "<input type='file' name='update' accept='.bin' required>";
    html += "<button class='btn btn-ota' type='submit'>SUBIR Y ACTUALIZAR FIRMWARE AHORA</button></form>";

    html += "</div></body></html>";
    _server.send(200, "text/html", html);
}

void UpdateManager::handleHttpSaveConfig() {
    AprsConfig& c = Aprs.config();

    if (_server.hasArg("brdMod")) {
        c.boardModel = (uint8_t)constrain(_server.arg("brdMod").toInt(), 0, 1);
    }
    if (_server.hasArg("dispInv")) {
        bool inv = (_server.arg("dispInv").toInt() == 1);
        Ui.setDisplayInvert(inv);
    }
    if (_server.hasArg("txPin")) {
        uint8_t p = (uint8_t)_server.arg("txPin").toInt();
        if (p == 27 || p == 21 || p == 4 || p == 17 || p == 26) c.txAudioPin = p;
    }
    if (_server.hasArg("mode")) {
        uint8_t newMode = (uint8_t)_server.arg("mode").toInt();
        Aprs.applyStationMode((StationMode)newMode);
    }
    if (_server.hasArg("call")) {
        String call = _server.arg("call");
        call.trim();
        call.toUpperCase();
        if (call.length() > 0) strlcpy(c.callsign, call.c_str(), sizeof(c.callsign));
    }
    if (_server.hasArg("ssid")) c.ssid = (uint8_t)constrain(_server.arg("ssid").toInt(), 0, 15);
    if (_server.hasArg("posMode")) c.posMode = (uint8_t)constrain(_server.arg("posMode").toInt(), 0, 1);
    if (_server.hasArg("snsMod")) {
        int sm = _server.arg("snsMod").toInt();
        c.sensorModel = (sm == SENSOR_BMP_BME280) ? SENSOR_BMP_BME280 : SENSOR_DISABLED;
    }
    // Exclusión mutua estricta: si se elige GPS externo, el sensor I2C queda apagado
    if (c.posMode == POS_MODE_GPS) {
        c.sensorModel = SENSOR_DISABLED;
    }
    if (_server.hasArg("bcnInt")) c.beaconIntervalSec = (uint16_t)constrain(_server.arg("bcnInt").toInt(), 30, 3600);
    if (_server.hasArg("lat")) c.fixedLat = _server.arg("lat").toDouble();
    if (_server.hasArg("lon")) c.fixedLon = _server.arg("lon").toDouble();
    if (_server.hasArg("comm")) strlcpy(c.comment, _server.arg("comm").c_str(), sizeof(c.comment));
    if (_server.hasArg("wssid")) strlcpy(c.wifiSsid, _server.arg("wssid").c_str(), sizeof(c.wifiSsid));
    if (_server.hasArg("wpass")) strlcpy(c.wifiPass, _server.arg("wpass").c_str(), sizeof(c.wifiPass));
    if (_server.hasArg("isSrv")) strlcpy(c.aprsIsServer, _server.arg("isSrv").c_str(), sizeof(c.aprsIsServer));
    if (_server.hasArg("isPort")) c.aprsIsPort = (uint16_t)_server.arg("isPort").toInt();
    if (_server.hasArg("isPass")) c.aprsIsPasscode = _server.arg("isPass").toInt();
    if (_server.hasArg("igBcn")) c.igateBeaconMode = (uint8_t)constrain(_server.arg("igBcn").toInt(), 0, 2);
    if (_server.hasArg("otaUrl")) strlcpy(c.otaServerUrl, _server.arg("otaUrl").c_str(), sizeof(c.otaServerUrl));

    if (_server.hasArg("emgInt")) {
        uint16_t ei = (uint16_t)_server.arg("emgInt").toInt();
        if (ei == 60 || ei == 120 || ei == 180) c.emergIntervalSec = ei;
    }
    if (_server.hasArg("emgMsg")) c.emergSendDirectMsg = (_server.arg("emgMsg").toInt() == 1);
    if (_server.hasArg("emgDst")) {
        String dst = _server.arg("emgDst");
        dst.trim();
        dst.toUpperCase();
        strlcpy(c.emergMsgDest, dst.c_str(), sizeof(c.emergMsgDest));
    }
    if (_server.hasArg("emgSnd")) c.emergSoundAlert = (_server.arg("emgSnd").toInt() == 1);

    Aprs.saveConfig();
    Sensors.forceDetectAndRead();
    Ui.applyDisplayHardware();
    Igate.reconnectNow();
    Ui.requestRedraw();

    _server.sendHeader("Location", "/", true);
    _server.send(302, "text/plain", "Guardado");
}

void UpdateManager::handleHttpOtaFinish() {
    _server.sendHeader("Connection", "close");
    if (_otaUploadOk && !Update.hasError()) {
        _server.send(200, "text/html",
                     "<html><body style='background:#0b132b;color:#10b981;font-family:sans-serif;text-align:center;padding:40px;'>"
                     "<h2>¡Actualización OTA Completada!</h2><p>Reiniciando ESP32 CYD...</p></body></html>");
        delay(1200);
        ESP.restart();
    } else {
        _server.send(500, "text/plain", "Error al actualizar firmware por Web OTA.");
    }
}

void UpdateManager::handleHttpOtaUpload() {
    HTTPUpload& upload = _server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        _otaUploadOk = false;
        Serial.printf("[WEB OTA] Recibiendo archivo: %s\n", upload.filename.c_str());
        setStatusMsg("Recibiendo .bin por Web OTA...");
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            _otaUploadOk = true;
            setStatusMsg("Web OTA OK! Reiniciando...");
            Serial.printf("[WEB OTA] Exito: %u bytes escritos.\n", (unsigned)upload.totalSize);
        } else {
            Update.printError(Serial);
            setStatusMsg("Fallo al finalizar Web OTA");
        }
    }
}

