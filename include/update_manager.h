#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WebServer.h>
#include "config.h"

#define MAX_SD_BIN_FILES 4

struct SdBinEntry {
    char path[40];
    uint32_t sizeBytes;
};

class UpdateManager {
public:
    UpdateManager();

    void begin();
    void update();

    // 1. Auto-detección en arranque de /update.bin o /firmware.bin (estilo mpviewer)
    bool checkAndApplySdUpdateOnBoot(TFT_eSPI& tft);

    // 2. Escaneo interactivo de archivos .bin en la MicroSD desde la UI (estilo WeatherClock)
    bool scanSdBinaries();
    bool installSdBinary(const char* binPath, TFT_eSPI& tft);

    // 3. Actualización remota OTA por HTTP/HTTPS (URL directa a .bin o JSON manifest con SemVer)
    bool checkAndApplyWebOta(const char* urlOrServer, TFT_eSPI& tft);

    // 4. Portal Web Local (Configuración + Subida de .bin por navegador + ArduinoOTA)
    void startWebPortal(bool forceAccessPoint = false);
    void stopWebPortal();
    bool isWebPortalRunning() const { return _portalRunning; }
    bool isApModeActive() const { return _apActive; }
    String getPortalAddress() const;

    // Estado del lector MicroSD para mostrar en pantalla
    bool isSdMounted() const { return _sdMounted; }
    uint32_t getSdCardSizeMb() const { return _cardSizeMb; }
    uint8_t getSdBinCount() const { return _binCount; }
    const SdBinEntry& getSdBin(uint8_t idx) const { return _binFiles[idx]; }
    const char* getStatusMsg() const { return _statusMsg; }
    void setStatusMsg(const char* msg);

    // Pantalla de progreso en el TFT ILI9341
    static void drawUpdateProgress(TFT_eSPI& tft, const char* title, const char* subtitle, int pct, uint16_t barColor = 0x07E0);

private:
    WebServer _server;
    bool _portalRunning = false;
    bool _apActive = false;
    bool _arduinoOtaReady = false;
    bool _otaUploadOk = false;

    bool _sdMounted = false;
    uint32_t _cardSizeMb = 0;
    SdBinEntry _binFiles[MAX_SD_BIN_FILES];
    uint8_t _binCount = 0;
    char _statusMsg[56] = "SD/OTA listo para escanear";

    static int compareSemVer(const String& vA, const String& vB);
    bool performStreamUpdate(Stream& stream, size_t totalSize, const char* title, const char* subtitle, TFT_eSPI& tft);

    void setupWebRoutes();
    void handleHttpRoot();
    void handleHttpSaveConfig();
    void handleHttpOtaFinish();
    void handleHttpOtaUpload();
};

extern UpdateManager Updater;

