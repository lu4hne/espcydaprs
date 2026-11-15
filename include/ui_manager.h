#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "cst820_touch.h"
#include "ax25_aprs.h"

enum UiTab : uint8_t {
    TAB_DASHBOARD = 0,
    TAB_MONITOR   = 1,
    TAB_MESSAGES  = 2,
    TAB_STATIONS  = 3,
    TAB_CONFIG    = 4
};

enum KeyboardTarget : uint8_t {
    KB_NONE = 0,
    KB_MSG_TO,
    KB_MSG_TEXT,
    KB_CFG_CALLSIGN,
    KB_CFG_COMMENT,
    KB_CFG_LAT,
    KB_CFG_LON,
    KB_CFG_ALT,
    KB_CFG_WIFI_SSID,
    KB_CFG_WIFI_PASS,
    KB_CFG_APRS_SERVER,
    KB_CFG_APRS_PASSCODE,
    KB_CFG_OTA_URL
};

class UiManager {
public:
    UiManager();

    void begin();
    void update();
    void applyDisplayHardware();

    TFT_eSPI& tft() { return _tft; }
    CST820Touch& touch() { return _touch; }
    void requestRedraw() { _needsFullRedraw = true; }

private:
    TFT_eSPI _tft;
    CST820Touch _touch;

    UiTab _currentTab = TAB_DASHBOARD;
    uint8_t _cfgPage = 0; // 0 = Modo/Estación, 1 = Radio/Baliza, 2 = WiFi/IGate APRS-IS, 3 = SD/OTA Update
    bool _needsFullRedraw = true;
    uint32_t _lastDynamicUpdateMs = 0;
    uint32_t _lastTouchMs = 0;

    // Estado de redacción de mensajes
    char _composeTo[12] = "LU4HNE-9";
    char _composeText[68] = "73 desde ESP32 CYD APRS!";
    bool _composeLoopback = false;

    // Estado del teclado táctil en pantalla
    KeyboardTarget _kbTarget = KB_NONE;
    char _kbBuffer[98];
    char _kbTitle[36];
    uint8_t _kbMaxLen = 64;
    bool _kbShift = true;
    bool _kbSymbols = false;

    // Caché para evitar parpadeo en actualizaciones dinámicas
    uint32_t _lastDrawnRxCount = 0xFFFFFFFF;
    uint32_t _lastDrawnTxCount = 0xFFFFFFFF;
    uint32_t _lastDrawnIgCount = 0xFFFFFFFF;
    uint32_t _lastDrawnUiRev   = 0xFFFFFFFF;

    void drawTopBar();
    void drawBottomTabs();
    void drawCurrentTab(bool fullRedraw);

    void drawDashboard(bool fullRedraw);
    void drawMonitor(bool fullRedraw);
    void drawMessages(bool fullRedraw);
    void drawStations(bool fullRedraw);
    void drawConfig(bool fullRedraw);

    // Teclado QWERTY en pantalla
    void openKeyboard(KeyboardTarget target, const char* title, const char* initialVal, uint8_t maxLen);
    void drawKeyboard();
    void handleKeyboardTouch(uint16_t tx, uint16_t ty);
    void commitKeyboardValue();

    // Manejadores de toque por pestaña
    void handleTouch(uint16_t tx, uint16_t ty);
    void handleDashboardTouch(uint16_t tx, uint16_t ty);
    void handleMonitorTouch(uint16_t tx, uint16_t ty);
    void handleMessagesTouch(uint16_t tx, uint16_t ty);
    void handleStationsTouch(uint16_t tx, uint16_t ty);
    void handleConfigTouch(uint16_t tx, uint16_t ty);

    void showToast(const char* msg, uint16_t bgColor);
};

extern UiManager Ui;
