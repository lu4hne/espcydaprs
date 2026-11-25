#include <Arduino.h>
#include <nvs_flash.h>
#include "config.h"
#include "afsk_modem.h"
#include "aprs_tracker.h"
#include "ble_console.h"
#include "wifi_portal.h"

static uint32_t s_sosPressMs   = 0;
static bool     s_sosHandled   = false;
static uint32_t s_bootPressMs  = 0;
static bool     s_bootHandled  = false;
static char     s_usbLineBuf[140];
static size_t   s_usbLineLen   = 0;

static void sendConsoleStatus() {
    char callFull[14];
    Tracker.formatMyFullCall(callFull, sizeof(callFull));
    const GpsStatus& g = Tracker.gps();
    char line[180];
    snprintf(line, sizeof(line),
             "[STATUS] %s | GPS:%s (%u sat) %.5f,%.5f %.0fkm/h %dm | TX:%lu RX:%lu | Aud:%u%% DC:%u | SOS:%s | WiFi:%s",
             callFull,
             g.validFix ? "FIX" : "NOFIX",
             (unsigned)g.satellites,
             Tracker.getCurrentLat(), Tracker.getCurrentLon(),
             g.speedKmh, (int)Tracker.getCurrentAlt(),
             (unsigned long)Modem.getTxPacketCount(),
             (unsigned long)Modem.getRxPacketCount(),
             (unsigned)Modem.getRxAudioLevel(),
             (unsigned)Modem.getRxDcRaw(),
             Tracker.isSosActive() ? "ON" : "OFF",
             Portal.isActive() ? Portal.getApSsid() : "OFF");
    Serial.println(line);
    Ble.sendLine(line);
}

static void handleConsoleCommand(const char* rawCmd) {
    if (!rawCmd) return;
    while (*rawCmd == ' ') rawCmd++;
    if (*rawCmd == '\0') return;

    if (strncasecmp(rawCmd, "msg ", 4) == 0) {
        const char* p = rawCmd + 4;
        while (*p == ' ') p++;
        const char* sp = strchr(p, ' ');
        if (!sp) {
            Ble.sendLine("[ERR] Uso: msg <INDICATIVO> <mensaje>");
            Serial.println(F("[ERR] Uso: msg <INDICATIVO> <mensaje>"));
            return;
        }
        char dest[12];
        size_t dLen = (size_t)(sp - p);
        if (dLen >= sizeof(dest)) dLen = sizeof(dest) - 1;
        memcpy(dest, p, dLen);
        dest[dLen] = '\0';

        const char* body = sp + 1;
        while (*body == ' ') body++;
        if (strlen(body) == 0) {
            Ble.sendLine("[ERR] Mensaje vacio");
            return;
        }
        Tracker.sendTextMessage(dest, body, false);
    } else if (strcasecmp(rawCmd, "bcn") == 0 || strcasecmp(rawCmd, "b") == 0) {
        Tracker.sendPositionBeacon(false);
    } else if (strcasecmp(rawCmd, "loop") == 0 || strcasecmp(rawCmd, "l") == 0) {
        Tracker.sendPositionBeacon(true);
    } else if (strcasecmp(rawCmd, "sos") == 0 || strcasecmp(rawCmd, "sos on") == 0) {
        Tracker.activateSos();
    } else if (strcasecmp(rawCmd, "sos off") == 0 || strcasecmp(rawCmd, "cancel") == 0) {
        Tracker.cancelSos();
    } else if (strcasecmp(rawCmd, "wifi") == 0 || strcasecmp(rawCmd, "w") == 0) {
        if (Portal.isActive()) {
            Portal.stop();
            Ble.sendLine("[WIFI] Portal AP apagado");
        } else {
            Portal.start(300);
            char msg[96];
            snprintf(msg, sizeof(msg), "[WIFI] AP '%s' activo en http://192.168.4.1 (5 min)", Portal.getApSsid());
            Ble.sendLine(msg);
        }
    } else if (strcasecmp(rawCmd, "status") == 0 || strcasecmp(rawCmd, "gps") == 0 || strcasecmp(rawCmd, "s") == 0) {
        sendConsoleStatus();
    } else if (strcasecmp(rawCmd, "gpstest") == 0 || strcasecmp(rawCmd, "gps test") == 0) {
        Tracker.runGpsCommandTest();
    } else if (strcasecmp(rawCmd, "gpsraw") == 0 || strcasecmp(rawCmd, "gps raw") == 0) {
        Tracker.startGpsRawDump(5000);
    } else if (strncasecmp(rawCmd, "gpscmd ", 7) == 0) {
        Tracker.sendGpsRawCommand(rawCmd + 7);
    } else if (strncasecmp(rawCmd, "gps send ", 9) == 0) {
        Tracker.sendGpsRawCommand(rawCmd + 9);
    } else if (strcasecmp(rawCmd, "help") == 0 || strcasecmp(rawCmd, "?") == 0) {
        const char* helpTxt = "[HELP] Comandos: msg <CALL> <txt> | bcn | loop | status | gpstest | gpsraw | gpscmd <NMEA> | sos | sos off | wifi";
        Serial.println(helpTxt);
        Ble.sendLine(helpTxt);
    } else {
        Ble.sendLine("[ERR] Comando desconocido. Escribe 'help'");
    }
}

static void onTrackerEvent(const char* eventLine) {
    Ble.sendLine(eventLine);
}

static void updateStatusLed() {
    // LED azul integrado en GPIO 8 (activo en LOW)
    uint32_t now = millis();
    bool ledOn = false;

    if (Modem.isTransmitting()) {
        ledOn = true; // Fijo encendido durante TX RF
    } else if (Tracker.isSosActive()) {
        ledOn = ((now / 150) % 2) == 0; // Parpadeo rápido continuo en SOS
    } else if (Portal.isActive()) {
        uint32_t m = now % 1500;
        ledOn = (m < 80) || (m >= 180 && m < 260) || (m >= 360 && m < 440); // Triple destello = WiFi Portal
    } else if (Tracker.gps().validFix) {
        ledOn = (now % 3000) < 60; // 1 destello breve cada 3 s = GPS Fix OK
    } else {
        uint32_t m = now % 2500;
        ledOn = (m < 60) || (m >= 180 && m < 240); // 2 destellos = Buscando GPS
    }

    digitalWrite(MT_LED_PIN, ledOn ? LOW : HIGH);
}

static void pollHardwareButtons() {
    uint32_t now = millis();

    // 1. Botón SOS por hardware (GPIO 4 a GND): mantener 3 s activa o cancela emergencia
    if (digitalRead(MT_SOS_BTN_PIN) == LOW) {
        if (s_sosPressMs == 0) {
            s_sosPressMs = now;
            s_sosHandled = false;
        } else if (!s_sosHandled && (now - s_sosPressMs >= 3000UL)) {
            s_sosHandled = true;
            if (Tracker.isSosActive()) Tracker.cancelSos();
            else                       Tracker.activateSos();
        }
    } else {
        s_sosPressMs = 0;
        s_sosHandled = false;
    }

    // 2. Botón BOOT integrado (GPIO 9): toque corto = Baliza manual | mantener 3 s = Portal WiFi
    if (digitalRead(MT_BOOT_BTN_PIN) == LOW) {
        if (s_bootPressMs == 0) {
            s_bootPressMs = now;
            s_bootHandled = false;
        } else if (!s_bootHandled && (now - s_bootPressMs >= 3000UL)) {
            s_bootHandled = true;
            if (Portal.isActive()) Portal.stop();
            else                   Portal.start(300);
        }
    } else {
        if (s_bootPressMs > 0 && !s_bootHandled && (now - s_bootPressMs >= 60UL)) {
            Tracker.sendPositionBeacon(false);
        }
        s_bootPressMs = 0;
        s_bootHandled = false;
    }
}

void setup() {
    Serial.begin(115200);
    delay(400);

    pinMode(MT_LED_PIN, OUTPUT);
    digitalWrite(MT_LED_PIN, HIGH); // Apagado (activo en LOW)
    pinMode(MT_SOS_BTN_PIN, INPUT_PULLUP);
    pinMode(MT_BOOT_BTN_PIN, INPUT_PULLUP);

    esp_err_t nvsErr = nvs_flash_init();
    if (nvsErr == ESP_ERR_NVS_NO_FREE_PAGES || nvsErr == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    Tracker.begin();
    Tracker.setEventCallback(onTrackerEvent);

    // Iniciar módem AFSK 1200 bps (prioridad máxima RX en Timer HW 0 + Sigma-Delta TX)
    Modem.begin();

    // Iniciar consola BLE Nordic UART Service (NUS)
    char bleName[24];
    snprintf(bleName, sizeof(bleName), "MT-APRS-%s", Tracker.config().callsign);
    Ble.begin(bleName, handleConsoleCommand);

    Serial.println(F("============================================================"));
    Serial.printf("  ESP32-C3 SuperMini - APRS MicroTracker v%s\n", MT_FIRMWARE_VERSION);
    Serial.println(F("  AFSK 1200 bps + GPS + BLE Consola + WiFi Portal + SOS HW  "));
    Serial.println(F("============================================================"));
    Serial.printf("  Pines -> RX:IO%d | TX:IO%d | PTT:IO%d | SOS:IO%d | GPS:IO%d/%d\n",
                  MT_AFSK_RX_PIN, MT_AFSK_TX_PIN, MT_PTT_PIN, MT_SOS_BTN_PIN, MT_GPS_RX_PIN, MT_GPS_TX_PIN);
    Serial.println(F("  Comandos: msg <CALL> <txt> | bcn | loop | status | sos | wifi"));

    // Si es la primera vez (indicativo N0CALL), abrir el Portal WiFi automáticamente 5 min
    if (strcasecmp(Tracker.config().callsign, "N0CALL") == 0) {
        Serial.println(F("[INIT] Indicativo N0CALL detectado -> Abriendo Portal WiFi de configuracion..."));
        Portal.start(300);
    }
}

void loop() {
    Tracker.update();
    Ble.update();
    Portal.update();
    pollHardwareButtons();
    updateStatusLed();

    // Comandos por USB Serial (mismos comandos que la consola BLE)
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            if (s_usbLineLen > 0) {
                s_usbLineBuf[s_usbLineLen] = '\0';
                handleConsoleCommand(s_usbLineBuf);
                s_usbLineLen = 0;
            }
        } else if (s_usbLineLen < sizeof(s_usbLineBuf) - 1) {
            s_usbLineBuf[s_usbLineLen++] = c;
        }
    }

    delay(10);
}
