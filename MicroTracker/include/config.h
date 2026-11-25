#pragma once

#include <Arduino.h>

#define MT_FIRMWARE_VERSION "1.0.0"

// ============================================================================
// PINOUT OPTIMIZADO: ESP32-C3 SuperMini (USB CDC nativo en GPIO 18/19)
// Sin conflictos de pines de strapping en PTT/SOS y usando RX/TX dedicados para GPS
// ============================================================================
#define MT_AFSK_RX_PIN      0   // ADC1_CH0: Audio RX desde SPK del handy (divisor 10k/10k + cap 1uF)
#define MT_AFSK_TX_PIN      1   // Sigma-Delta 312.5 kHz: Audio TX hacia MIC del handy (atenuador 10k/1k + cap 1uF)
#define MT_PTT_PIN          3   // Control PTT a transistor NPN 2N2222/BC547 (base con 1k)
#define MT_SOS_BTN_PIN      4   // Botón SOS por hardware a GND (INPUT_PULLUP, 3 seg activa/cancela)
#define MT_LED_PIN          8   // LED azul integrado en ESP32-C3 SuperMini (activo en LOW)
#define MT_BOOT_BTN_PIN     9   // Botón BOOT integrado (corto = Baliza manual, 3 seg = Portal WiFi)
#define MT_GPS_RX_PIN       20  // UART1 RX (conectar al TX del módulo GPS NMEA 9600 bps)
#define MT_GPS_TX_PIN       21  // UART1 TX (conectar al RX del módulo GPS)

#define MT_GPS_BAUD_RATE    9600

// ============================================================================
// PARÁMETROS DEL MÓDEM AFSK BELL 202 (1200 baudios)
// ============================================================================
#define AFSK_BAUD_RATE      1200
#define AFSK_SAMPLE_RATE    9600
#define AFSK_SAMPLES_PER_BIT (AFSK_SAMPLE_RATE / AFSK_BAUD_RATE) // 8 muestras/bit
#define AFSK_MARK_FREQ      1200
#define AFSK_SPACE_FREQ     2200

enum RadioPreset : uint8_t {
    RADIO_BAOFENG = 0,
    RADIO_YAESU   = 1,
    RADIO_CUSTOM  = 2
};

struct TrackerConfig {
    char     callsign[10];      // Ej: "LU4HNE"
    uint8_t  ssid;              // 0..15 (ej: 9 para móvil, 7 para portátil)
    char     destCall[8];       // Ej: "APEC3T"
    char     path1[8];          // Ej: "WIDE1"
    uint8_t  path1Ssid;         // 1
    char     path2[8];          // Ej: "WIDE2"
    uint8_t  path2Ssid;         // 1
    char     symbolTable;       // '/' o '\'
    char     symbolCode;        // '>' auto, '[' caminante, 'b' bici, 'k' camioneta
    char     comment[44];       // Comentario en baliza

    bool     useGps;            // true = GPS NMEA, false = posición fija de respaldo
    double   fixedLat;          // Respaldo si no hay fix GPS
    double   fixedLon;
    int16_t  fixedAltMeters;

    bool     autoBeacon;        // Habilita baliza automática
    uint16_t beaconIntervalSec; // Intervalo fijo (seg) si SmartBeaconing está OFF
    bool     smartBeacon;       // SmartBeaconing™ por velocidad y giros
    uint16_t sbFastRateSec;     // Ej: 60s a alta velocidad
    uint16_t sbSlowRateSec;     // Ej: 600s detenido
    uint8_t  sbFastSpeedKmh;    // Ej: 60 km/h
    uint8_t  sbSlowSpeedKmh;    // Ej: 5 km/h
    uint8_t  sbTurnAngleDeg;    // Ej: 28 grados

    uint8_t  radioPreset;       // RADIO_BAOFENG / RADIO_YAESU / RADIO_CUSTOM
    uint16_t txDelayMs;         // Preámbulo 0x7E (Baofeng ~450ms, Yaesu ~250ms)
    uint16_t txTailMs;          // Cola 0x7E (~60ms)
    uint8_t  txVolume;          // 10..100%
    bool     autoAck;           // Responder ACK automático a mensajes entrantes

    uint16_t sosIntervalSec;    // Intervalo de baliza SOS (60 / 120 / 180s)
    bool     sosSendMsg;        // Enviar además mensaje directo SOS
    char     sosMsgDest[12];    // Destinatario mensaje directo SOS (ej: "LU4HNE-10")
    char     sosText[36];       // Texto personalizado de emergencia
};
