#include <Arduino.h>
#include "config.h"
#include "afsk_modem.h"
#include "ax25_aprs.h"
#include "ui_manager.h"

static uint32_t s_lastSerialLogCount = 0;

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println();
    Serial.println(F("============================================================"));
    Serial.println(F("  ESP32-2432S024C (CYD) - Estacion Transceptora APRS 1200   "));
    Serial.println(F("  Modem AFSK Bell 202 (1200/2200 Hz) + AX.25 UI + Touch GUI "));
    Serial.println(F("============================================================"));
    Serial.printf("  Pines Radio -> TX(DAC): GPIO %d | RX(ADC): GPIO %d | PTT: GPIO %d\n",
                  AFSK_TX_PIN, AFSK_RX_PIN, RADIO_PTT_PIN);
    Serial.printf("  Pines GPS   -> RX2: GPIO %d | TX2: GPIO %d (%d bps)\n",
                  GPS_UART_RX_PIN, GPS_UART_TX_PIN, GPS_BAUD_RATE);
    Serial.println(F("  Comandos USB Serial: 'b'=Baliza RF, 'l'=Test Loopback, 's'=Estado"));
    Serial.println(F("============================================================"));

    // 1. Iniciar Módem DSP AFSK 1200 bps en Core 0
    Modem.begin();

    // 2. Iniciar Motor AX.25 / APRS y cargar configuración desde NVS
    Aprs.begin();

    // 3. Iniciar Interfaz Gráfica Táctil (ILI9341 + CST820)
    Ui.begin();
}

void loop() {
    // Actualizar decodificación de paquetes, GPS, baliza periódica y reintentos ACK
    Aprs.update();

    // Actualizar pantalla y touch capacitivo
    Ui.update();

    // Imprimir por puerto serie USB cualquier paquete nuevo decodificado
    uint32_t currentPktCount = Modem.getRxPacketCount();
    if (currentPktCount != s_lastSerialLogCount && Aprs.getPacketLogCount() > 0) {
        s_lastSerialLogCount = currentPktCount;
        const DecodedPacket& p = Aprs.getPacketLog(0);
        Serial.printf("[APRS RX #%lu] %s>%s,%s: %s\n",
                      (unsigned long)currentPktCount,
                      p.srcCall, p.dstCall, p.pathStr, p.infoText);
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
            Serial.println(F("[CMD] Transmitiendo baliza APRS por RF..."));
            Aprs.sendPositionBeacon(false);
        } else if (cmd == 'l' || cmd == 'L') {
            Serial.println(F("[CMD] Ejecutando prueba de Loopback AFSK 1200 bps..."));
            Aprs.sendPositionBeacon(true);
        } else if (cmd == 's' || cmd == 'S') {
            AprsConfig& c = Aprs.config();
            Serial.printf("[STATUS] Call: %s-%u | Pos: %.4f, %.4f | TX: %lu | RX: %lu | AudioRX: %u%%\n",
                          c.callsign, c.ssid, Aprs.getCurrentLat(), Aprs.getCurrentLon(),
                          (unsigned long)Modem.getTxPacketCount(),
                          (unsigned long)Modem.getRxPacketCount(),
                          Modem.getRxAudioLevel());
        }
    }

    delay(10);
}

