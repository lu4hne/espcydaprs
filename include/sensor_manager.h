#pragma once

#include <Arduino.h>
#include "config.h"

struct SensorReading {
    bool     connected;       // true SOLO si el sensor físico responde con datos válidos
    bool     hasHumidity;     // false en BMP280, true en BME280 / AHT20 / DHT22 / DHT11
    bool     hasPressure;     // true en BMP280 / BME280, false en AHT20 / DHT
    float    temperatureC;    // Temperatura en °C (con offset aplicado)
    float    humidityPct;     // Humedad relativa 0..100% (con offset aplicado)
    float    pressureHpa;     // Presión atmosférica en hPa (mbar)
    uint8_t  i2cAddr;         // Dirección I2C detectada (0x76, 0x77, 0x38 o 0 para DHT)
    char     chipName[16];    // Ej: "BMP280", "BME280", "AHT20", "DHT22", "DHT11", "SIN SENSOR"
    char     statusText[40];  // Estado detallado para mostrar en la pestaña SENS
    uint32_t lastReadMs;
};

class SensorManager {
public:
    SensorManager();

    void begin();
    void update();
    bool forceDetectAndRead();

    bool isConnected() const { return _reading.connected; }
    const SensorReading& reading() const { return _reading; }

    // Obtiene los pines SDA / SCL (o DATA) activos según el perfil configurado
    void getActivePins(int8_t* outSdaOrData, int8_t* outScl) const;
    const char* getPinProfileText() const;
    const char* getModelConfigText() const;
    const char* getBeaconModeText() const;

    // Formatea resumen corto para adjuntar al comentario de baliza (ej. "24.5C 48% 1013hPa")
    // Devuelve false si no hay sensor conectado o si sensorBeaconMode == SENSOR_BCN_OFF
    bool formatCommentTelemetry(char* outBuf, size_t maxLen) const;

    // Formatea campos meteorológicos APRS estándar (ej. ".../...g...t076h48b10132")
    bool formatAprsWxPayload(char* outBuf, size_t maxLen) const;

private:
    SensorReading _reading;
    uint32_t _lastPollMs = 0;
    bool _bmpInitialized = false;
    uint8_t _bmpAddr = 0;
    uint8_t _bmpChipId = 0;

    // Coeficientes de calibración de fábrica Bosch BMP280 / BME280
    uint16_t _dig_T1 = 0;
    int16_t  _dig_T2 = 0, _dig_T3 = 0;
    uint16_t _dig_P1 = 0;
    int16_t  _dig_P2 = 0, _dig_P3 = 0, _dig_P4 = 0, _dig_P5 = 0;
    int16_t  _dig_P6 = 0, _dig_P7 = 0, _dig_P8 = 0, _dig_P9 = 0;
    uint8_t  _dig_H1 = 0, _dig_H3 = 0;
    int16_t  _dig_H2 = 0, _dig_H4 = 0, _dig_H5 = 0;
    int8_t   _dig_H6 = 0;

    bool initAndReadBmpBme280(int8_t sda, int8_t scl);
    bool initAndReadAht20(int8_t sda, int8_t scl);
    bool readDhtSensor(int8_t pin, bool isDht22);

    bool i2cWriteReg(uint8_t addr, uint8_t reg, uint8_t val);
    bool i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len);
};

extern SensorManager Sensors;
