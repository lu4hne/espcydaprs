#pragma once

#include <Arduino.h>

#define FIRMWARE_VERSION    "1.6.8"

// ============================================================================
// PLACA: ESP32 CYD (Soporte Dual: Modelo ST7789 1-USB y Modelo ILI9341)
// ============================================================================

// --- Pines de Backlight posibles en placas CYD ---
// En modelo ST7789 (1 USB) y 2432S028R el Backlight es GPIO 21 (dejando GPIO 27 libre).
// En algunas variantes 2432S024C el Backlight es GPIO 27 (dejando GPIO 21 libre).
#define CYD_BL_PIN_21       21
#define CYD_BL_PIN_27       27

// --- Touch Capacitivo CST820 (I2C) ---
#define CST820_I2C_SDA      33
#define CST820_I2C_SCL      32
#define CST820_I2C_RST      25
#define CST820_I2C_INT      -1  // No usar GPIO 21 como INPUT para no apagar el Backlight
#define CST820_I2C_ADDR     0x15

// --- Touch Resistivo XPT2046 (SPI dedicado en placas CYD ST7789 / ILI9341) ---
#define XPT2046_IRQ_PIN     36
#define XPT2046_MOSI_PIN    32
#define XPT2046_MISO_PIN    39
#define XPT2046_CLK_PIN     25
#define XPT2046_CS_PIN      33

// --- Botón físico BOOT en la placa ---
#define CYD_BOOT_BTN_PIN    0

// --- Lector de Tarjeta MicroSD Incorporado (Bus VSPI) ---
#define SD_PIN_CS           5
#define SD_PIN_SCK          18
#define SD_PIN_MISO         19
#define SD_PIN_MOSI         23

// --- LED RGB Incorporado en la placa (Activo en BAJO / LOW) ---
#define CYD_LED_R           4
#define CYD_LED_G           16
#define CYD_LED_B           17

// --- Interfaz de Radio VHF/UHF (AFSK 1200 baudios + PTT) ---
// GPIO 26 = DAC2 interno (conectado al amplificador de audio SC8002B -> conector SPK P4)
#define AFSK_DAC2_PIN       26

// Pin por defecto en conectores blancos P3/CN1 (configurable en pantalla: 27, 21, 4, 17 o 26)
// Se modula mediante el DAC Sigma-Delta por hardware del ESP32 (312.5 kHz) en paralelo con GPIO 26
#define AFSK_DEFAULT_TX_PIN 27

// GPIO 35 = ADC1_CH7 (Entrada de audio desde SPK/EAR del radio, presente en ambos modelos)
#define AFSK_RX_PIN         35

// GPIO 22 = Salida de control PTT (presente en los conectores blancos de ambos modelos)
// Activo en ALTO (HIGH = Transmitiendo, LOW = Recibiendo)
#define RADIO_PTT_PIN       22

// --- Puerto GPS Externo Opcional (UART2) ---
// En el modelo ST7789 están expuestos GPIO 17 (RX2) y GPIO 16 (TX2) en el conector P3.
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
// MODELOS DE PLACA, MODOS DE ESTACIÓN Y CONFIGURACIÓN PERSISTENTE (NVS)
// ============================================================================
enum BoardModel : uint8_t {
    BOARD_MODEL_ILI9341 = 0, // Placa con driver ILI9341 (Conectores: IO35, IO22, IO21, IO27, TX, RX -> TX Audio en IO27 o IO21)
    BOARD_MODEL_ST7789  = 1  // Placa con driver ST7789 1-USB (Conectores: IO35, IO22, IO21, IO4, IO16, IO17 -> TX Audio en IO4 o IO17)
};

enum StationMode : uint8_t {
    STATION_MODE_TRACKER     = 0, // Tracker Móvil / Portátil (GPS o Fija, RF TX, sin WiFi/IGate)
    STATION_MODE_FIXED_IGATE = 1, // Estación Fija + IGate (Pos Fija, WiFi + Inyección RF->APRS-IS)
    STATION_MODE_FIXED_RF    = 2  // Estación Fija Solo RF / Digipeater WIDE1-1 (sin WiFi)
};

enum IgateBeaconMode : uint8_t {
    IGATE_BCN_BOTH    = 0, // Transmitir baliza propia por RF y también inyectarla a APRS-IS
    IGATE_BCN_IS_ONLY = 1, // Inyectar baliza propia solo a APRS-IS (Internet)
    IGATE_BCN_RF_ONLY = 2  // Transmitir baliza propia solo por Radio VHF/UHF
};

enum RadioType : uint8_t {
    RADIO_BAOFENG = 0,  // Baofeng UV-5R / UV-82 (requiere TX Delay mayor ~450ms si se usa ahorro de batería o VOX)
    RADIO_YAESU   = 1,  // Yaesu FT-60 / FT-65 / FT-4X / FT-2980 (PTT rápido ~250ms, mic más sensible)
    RADIO_CUSTOM  = 2
};

enum PositionMode : uint8_t {
    POS_MODE_FIXED = 0, // Coordenadas fijas configuradas en pantalla
    POS_MODE_GPS   = 1  // Coordenadas leídas desde módulo GPS UART NMEA
};

enum SensorModel : uint8_t {
    SENSOR_BMP_BME280 = 1, // Único sensor soportado: BME280 (T+H+P) / BMP280 (T+P) por I2C hardware
    SENSOR_DISABLED   = 5  // Sensores desactivados (valor mantenido en 5 por compatibilidad NVS)
};

enum SensorPinProfile : uint8_t {
    SENSOR_PINS_16_17 = 0, // Conector P3 (ST7789): SDA=IO16, SCL=IO17 (incompatible con GPS UART2)
    SENSOR_PINS_21_22 = 1, // Conector P3/CN1: SDA=IO21, SCL=IO22
    SENSOR_PINS_27_22 = 2  // Conector CN1 (ILI9341): SDA=IO27, SCL=IO22
};

enum SensorBeaconMode : uint8_t {
    SENSOR_BCN_COMMENT = 0, // Agregar "24.5C 48% 1013hPa" al comentario (solo si el sensor está conectado)
    SENSOR_BCN_WX_PACK = 1, // Trama WX oficial APRS "/_...t...h...b..." (solo si el sensor está conectado)
    SENSOR_BCN_OFF     = 2  // No incluir lecturas en la baliza APRS
};

enum EmergencyType : uint8_t {
    EMERG_TYPE_SOS       = 0, // "SOS GENERAL / AUXILIO"
    EMERG_TYPE_MEDICAL   = 1, // "EMERGENCIA MEDICA"
    EMERG_TYPE_VEHICLE   = 2, // "AUXILIO MECANICO / VEHICULO"
    EMERG_TYPE_LOST      = 3  // "EXTRAVIO / RESCATE"
};

struct EmergencyState {
    bool     active;
    uint8_t  type;
    uint32_t startTimeMs;
    uint32_t lastBeaconMs;
    uint16_t beaconCount;
};

struct AprsConfig {
    uint8_t boardModel;     // BOARD_MODEL_ILI9341 o BOARD_MODEL_ST7789
    bool displayInvert;     // Inversión de color del panel (útil en variantes ST7789 / IPS)
    uint8_t txAudioPin;     // Pin en conector blanco para Audio TX Sigma-Delta: 27, 21, 4, 17 o 26 (además de DAC2 IO26)

    uint8_t stationMode;    // STATION_MODE_TRACKER, STATION_MODE_FIXED_IGATE, STATION_MODE_FIXED_RF
    char callsign[10];      // Ej: "LU4HNE"
    uint8_t ssid;           // 0 a 15 (Ej: 7 Handy, 9 Móvil/Tracker, 10 IGate/Estación Fija)
    char destCall[10];      // Destino AX.25: "APECYD" (APRS ESP32 CYD)
    char path1[10];         // Ej: "WIDE1"
    uint8_t path1Ssid;      // Ej: 1
    char path2[10];         // Ej: "WIDE2"
    uint8_t path2Ssid;      // Ej: 1
    char symbolTable;       // '/' (Primaria) o '\' (Alternativa) o 'I' (IGate overlay)
    char symbolCode;        // '[' (Handy), '>' (Auto), '-' (Casa), '&' (IGate), 'k' (Camioneta)
    char comment[44];       // Comentario de baliza APRS
    double fixedLat;        // Latitud en grados decimales (Sur negativo, ej. -34.6037)
    double fixedLon;        // Longitud en grados decimales (Oeste negativo, ej. -58.3816)
    int16_t fixedAltMeters; // Altitud fija en metros
    uint8_t posMode;        // POS_MODE_FIXED o POS_MODE_GPS
    bool autoBeacon;        // Baliza periódica automática activada
    uint16_t beaconIntervalSec; // Intervalo de baliza en segundos (ej. 60, 120, 300, 600, 900)
    uint8_t radioType;      // RADIO_BAOFENG, RADIO_YAESU, RADIO_CUSTOM
    uint16_t txDelayMs;     // Preámbulo de banderas 0x7E en ms (Baofeng: 450ms, Yaesu: 250ms)
    uint16_t txTailMs;      // Cola antes de soltar PTT en ms (default 50ms)
    uint8_t txVolume;       // Amplitud DAC de salida (10 a 100%)
    bool autoAckMessages;   // Responder ACK automáticamente al recibir mensaje propio
    bool digipeatWide1;     // Repetir (Fill-in Digipeater WIDE1-1) opcional

    // --- Red WiFi e IGate APRS-IS ---
    char wifiSsid[33];      // Nombre de red WiFi para Estación Fija + IGate / OTA
    char wifiPass[65];      // Contraseña WiFi
    bool igateEnabled;      // Inyección RF -> APRS-IS habilitada
    char aprsIsServer[48];  // Servidor APRS-IS (ej. "samerica.aprs2.net" o "rotate.aprs2.net")
    uint16_t aprsIsPort;    // Puerto APRS-IS (estándar: 14580)
    int32_t aprsIsPasscode; // -1 = calcular automáticamente desde callsign, o passcode manual
    uint8_t igateBeaconMode;// IGATE_BCN_BOTH, IGATE_BCN_IS_ONLY, IGATE_BCN_RF_ONLY
    bool igateIsToRfMsg;    // Reenviar mensajes de texto desde APRS-IS hacia RF para estaciones locales

    // --- Actualización OTA Remota y Brillo ---
    char otaServerUrl[96];  // URL base o JSON para actualización OTA por HTTP/HTTPS
    uint8_t ledBrightness;  // Brillo LED RGB Azul de estado: 0 = Apagado, 1 = Muy tenue (2%), 2 = Medio (10%)
    uint8_t tftBrightness;  // Brillo de pantalla TFT (20 a 100%)

    // --- Sensores Meteorológicos Locales (BMP280 / BME280 / AHT20 / DHT22 / DHT11) ---
    uint8_t sensorModel;      // SensorModel (por defecto SENSOR_AUTO_I2C)
    uint8_t sensorPinProfile; // SensorPinProfile (por defecto SENSOR_PINS_16_17)
    uint8_t sensorBeaconMode; // SensorBeaconMode (por defecto SENSOR_BCN_COMMENT)
    float   tempOffsetC;      // Calibración de temperatura en °C
    float   humOffsetPct;     // Calibración de humedad en %

    // --- Aviso de Emergencia (SOS) Exclusivo para Modo Tracker ---
    uint16_t emergIntervalSec;   // Intervalo baliza SOS en seg (120s por defecto; 60, 120, 180)
    bool     emergSendDirectMsg; // true = Enviar además mensaje APRS directo
    char     emergMsgDest[10];   // Indicativo destinatario (ej. "LU4HNE")
    bool     emergSoundAlert;    // true = Alerta sonora intermitente en altavoz
};

// Límites de almacenamiento en RAM para la interfaz gráfica
#define MAX_PACKET_LOG      24
#define MAX_STATIONS_LIST   20
#define MAX_MESSAGES_LIST   20
