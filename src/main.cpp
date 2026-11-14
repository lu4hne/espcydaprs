#include <Arduino.h>
#include "config.h"
#include "afsk_modem.h"
#include "ax25_aprs.h"
#include "igate_manager.h"
#include "update_manager.h"
#include "ui_manager.h"

static uint32_t s_lastSerialLogCount = 0;

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println();
    Serial.println(F("============================================================"));
    Serial.printf("  ESP32-2432S024C (CYD) - APRS 1200 Tracker / IGate v%s\n", FIRMWARE_VERSION);
    Serial.println(F("  Modem AFSK Bell 202 + AX.25 + IGate APRS-IS + SD/Web OTA  "));
    Serial.println(F("============================================================"));
    Serial.printf("  Pines Radio -> TX(DAC): GPIO %d | RX(ADC): GPIO %d | PTT: GPIO %d\n",
                  AFSK_TX_PIN, AFSK_RX_PIN, RADIO_PTT_PIN);
    Serial.printf("  Pines GPS   -> RX2: GPIO %d | TX2: GPIO %d (%d bps)\n",
                  GPS_UART_RX_PIN, GPS_UART_TX_PIN, GPS_BAUD_RATE);
    Serial.printf("  Pines SD    -> CS: GPIO %d | SCK: %d | MISO: %d | MOSI: %d\n",
                  SD_PIN_CS, SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI);
    Serial.println(F("  Comandos USB: 'b'=Baliza, 'l'=Loopback, 'm'=Modo, 'w'=WebOTA, 's'=Estado"));
    Serial.println(F("============================================================"));

    // 1. Iniciar Interfaz Gráfica Táctil (ILI9341 + CST820)
    Ui.begin();

    // 2. Verificar si existe /update.bin o /firmware.bin en la MicroSD al arrancar
    Updater.checkAndApplySdUpdateOnBoot(Ui.tft());

    // 3. Iniciar Módem DSP AFSK 1200 bps en Core 0
    Modem.begin();

    // 4. Iniciar Motor AX.25 / APRS y cargar configuración desde NVS
    Aprs.begin();

    // 5. Iniciar Cliente WiFi / IGate APRS-IS y Gestor OTA
    Igate.begin();
    Updater.begin();
    Ui.requestRedraw();
}

void loop() {
    // 1. Actualizar decodificación de paquetes, GPS, baliza periódica y reintentos ACK
    Aprs.update();

    // 2. Actualizar conexión WiFi y socket TCP APRS-IS (en modo Estación Fija + IGate)
    Igate.update();

    // 3. Actualizar servidor Web OTA / ArduinoOTA si está activo
    Updater.update();

    // 4. Actualizar pantalla y touch capacitivo
    Ui.update();

    // Imprimir por puerto serie USB cualquier paquete nuevo decodificado
    uint32_t currentPktCount = Modem.getRxPacketCount();
    if (currentPktCount != s_lastSerialLogCount && Aprs.getPacketLogCount() > 0) {
        s_lastSerialLogCount = currentPktCount;
        const DecodedPacket& p = Aprs.getPacketLog(0);
        Serial.printf("[APRS RX #%lu] %s>%s,%s: %s %s\n",
                      (unsigned long)currentPktCount,
                      p.srcCall, p.dstCall, p.pathStr, p.infoText,
                      p.gatedToIs ? "[-> INYECTADO A APRS-IS]" : "");
        if (p.hasPosition) {
            Serial.printf("   -> Pos: %.4f, %.4f | Dist: %.2f km | Rumbo: %d deg (%s) | Alt: %d m\n",
                          p.lat, p.lon, p.distanceKm, p.bearingDeg,
                          AprsEngine::bearingToCardinal(p.bearingDeg), p.altitudeM);
        }
    }

    // Comandos rápidos por consola serie para pruebas de laboratorio
    if (Serial.available() > 0) {
        char cmd = (char)Serial.read();
        if (cmd == 'b' || cmd == 'B') {
            Serial.println(F("[CMD] Transmitiendo baliza APRS..."));
            Aprs.sendPositionBeacon(false);
        } else if (cmd == 'l' || cmd == 'L') {
            Serial.println(F("[CMD] Ejecutando prueba de Loopback AFSK 1200 bps..."));
            Aprs.sendPositionBeacon(true);
        } else if (cmd == 'm' || cmd == 'M') {
            StationMode next = (StationMode)((Aprs.config().stationMode + 1) % 3);
            Aprs.applyStationMode(next);
            Aprs.saveConfig();
            Igate.reconnectNow();
            Ui.requestRedraw();
            Serial.printf("[CMD] Modo cambiado a: %u (0=Tracker, 1=Fija+IGate, 2=Fija RF)\n", (unsigned)next);
        } else if (cmd == 'w' || cmd == 'W') {
            Updater.startWebPortal(true);
            Ui.requestRedraw();
        } else if (cmd == 'u' || cmd == 'U') {
            Updater.scanSdBinaries();
            Serial.printf("[CMD] Escaneo SD: %s\n", Updater.getStatusMsg());
            Ui.requestRedraw();
        } else if (cmd == 's' || cmd == 'S') {
            AprsConfig& c = Aprs.config();
            const char* modeName = (c.stationMode == STATION_MODE_FIXED_IGATE) ? "FIJA+IGATE" :
                                   (c.stationMode == STATION_MODE_TRACKER)     ? "TRACKER" : "FIJA-RF";
            Serial.printf("[STATUS] Call: %s-%u | Modo: %s | Pos: %.4f, %.4f | TX: %lu | RX: %lu | IGATE: %s (%lu pkts, Pass=%u) | AudioRX: %u%%\n",
                          c.callsign, c.ssid, modeName,
                          Aprs.getCurrentLat(), Aprs.getCurrentLon(),
                          (unsigned long)Modem.getTxPacketCount(),
                          (unsigned long)Modem.getRxPacketCount(),
                          Igate.getStateText(),
                          (unsigned long)Igate.getRfToIsCount(),
                          Igate.getEffectivePasscode(),
                          Modem.getRxAudioLevel());
        }
    }

    delay(10);
}
