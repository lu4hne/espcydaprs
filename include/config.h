#pragma once

#include <Arduino.h>

// ============================================================================
// PLACA: ESP32-2432S024C (2.4" 240x320 ILI9341 + Touch Capacitivo CST820)
// ============================================================================

// --- Pantalla TFT (Definidos también en platformio.ini para TFT_eSPI) ---
#define CYD_TFT_BL          27

// --- Touch Capacitivo CST820 (I2C) ---
#define CST820_I2C_SDA      33
#define CST820_I2C_SCL      32
#define CST820_I2C_RST      25
#define CST820_I2C_INT      21
#define CST820_I2C_ADDR     0x15

// --- LED RGB Incorporado en la placa (Activo en BAJO / LOW) ---
#define CYD_LED_R           4
#define CYD_LED_G           16
#define CYD_LED_B           17

// --- Interfaz de Radio VHF/UHF (AFSK 1200 baudios + PTT) ---
// GPIO 26 = DAC2 (Salida de onda senoidal AFSK hacia MIC del radio)
#define AFSK_TX_PIN         26

// GPIO 35 = ADC1_CH7 (Entrada de audio desde SPK/EAR del radio, polarizado a 1.65V)
#define AFSK_RX_PIN         35

// GPIO 22 = Salida de control PTT (En conector CN1/P3 -> Base de transistor 2N2222 / Optoacoplador)
// Activo en ALTO (HIGH = Transmitiendo, LOW = Recibiendo)
#define RADIO_PTT_PIN       22

// --- Puerto GPS Externo Opcional (UART2) ---
// Por defecto usa GPIO 17 (RX2) y GPIO 16 (TX2) cuando se habilita modo GPS,
// o puedes conectarlo en el conector de expansión.
#define GPS_UART_RX_PIN     17
#define GPS_UART_TX_PIN     16
#define GPS_BAUD_RATE       9600

// ============================================================================
// PARÁMETROS DEL MÓDEM AFSK 1200 BAUDIOS (BELL 202)
// ============================================================================
#define AFSK_BAUD_RATE      1200
#define AFSK_SAMPLE_RATE    9600    // 8 muestras exactas por bit a 1200 baudios
#define AFSK_SAMPLES_PER_BIT (AFSK_SAMPLE_RATE / AFSK_BAUD_RATE)
#define AFSK_MARK_FREQ      1200    // Bit '1' = 1200 Hz
#define AFSK_SPACE_FREQ     2200    // Bit '0' = 2200 Hz

// ============================================================================
// PERFILES DE RADIO Y CONFIGURACIÓN PERSISTENTE (NVS)
// ============================================================================
enum RadioType : uint8_t {
    RADIO_BAOFENG = 0,  // Baofeng UV-5R / UV-82 (requiere TX Delay mayor ~450ms si se usa ahorro de batería o VOX)
    RADIO_YAESU   = 1,  // Yaesu FT-60 / FT-65 / FT-4X / FT-2980 (PTT rápido ~250ms, mic más sensible)
    RADIO_CUSTOM  = 2
};

enum PositionMode : uint8_t {
    POS_MODE_FIXED = 0, // Coordenadas fijas configuradas en pantalla
    POS_MODE_GPS   = 1  // Coordenadas leídas desde módulo GPS UART NMEA
};

struct AprsConfig {
    char callsign[10];      // Ej: "LU4HNE"
    uint8_t ssid;           // 0 a 15 (Ej: 7 para Handy, 9 para Móvil)
    char destCall[10];      // Destino AX.25: "APECYD" (APRS ESP32 CYD)
    char path1[10];         // Ej: "WIDE1"
    uint8_t path1Ssid;      // Ej: 1
    char path2[10];         // Ej: "WIDE2"
    uint8_t path2Ssid;      // Ej: 1
    char symbolTable;       // '/' (Primaria) o '\' (Alternativa)
    char symbolCode;        // '[' (Persona/Handy), '>' (Auto), '-' (Casa), 'k' (Camioneta)
    char comment[44];       // Comentario de baliza APRS
    double fixedLat;        // Latitud en grados decimales (Sur negativo, ej. -34.6037)
    double fixedLon;        // Longitud en grados decimales (Oeste negativo, ej. -58.3816)
    int16_t fixedAltMeters; // Altitud fija en metros
    uint8_t posMode;        // POS_MODE_FIXED o POS_MODE_GPS
    bool autoBeacon;        // Baliza periódica automática activada
    uint16_t beaconIntervalSec; // Intervalo de baliza en segundos (ej. 120, 300, 600, 900)
    uint8_t radioType;      // RADIO_BAOFENG, RADIO_YAESU, RADIO_CUSTOM
    uint16_t txDelayMs;     // Preámbulo de banderas 0x7E en ms (Baofeng: 450ms, Yaesu: 250ms)
    uint16_t txTailMs;      // Cola antes de soltar PTT en ms (default 50ms)
    uint8_t txVolume;       // Amplitud DAC de salida (10 a 100%)
    bool autoAckMessages;   // Responder ACK automáticamente al recibir mensaje propio
    bool digipeatWide1;     // Repetir (Fill-in Digipeater WIDE1-1) opcional
};

// Límites de almacenamiento en RAM para la interfaz gráfica
#define MAX_PACKET_LOG      24
#define MAX_STATIONS_LIST   20
#define MAX_MESSAGES_LIST   20

