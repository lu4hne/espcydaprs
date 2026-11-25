#include "sensor_manager.h"
#include "ax25_aprs.h"
#include <Wire.h>
#include <math.h>

SensorManager Sensors;

SensorManager::SensorManager() {
    memset(&_reading, 0, sizeof(_reading));
    strlcpy(_reading.chipName, "SIN SENSOR", sizeof(_reading.chipName));
    strlcpy(_reading.statusText, "Buscando sensor...", sizeof(_reading.statusText));
}

void SensorManager::getActivePins(int8_t* outSdaOrData, int8_t* outScl) const {
    const AprsConfig& cfg = Aprs.config();
    switch (cfg.sensorPinProfile) {
        case SENSOR_PINS_21_22:
            *outSdaOrData = 21;
            *outScl       = 22;
            break;
        case SENSOR_PINS_27_22:
            *outSdaOrData = 27;
            *outScl       = 22;
            break;
        case SENSOR_PINS_16_17:
        default:
            *outSdaOrData = 16;
            *outScl       = 17;
            break;
    }
}

const char* SensorManager::getPinProfileText() const {
    switch (Aprs.config().sensorPinProfile) {
        case SENSOR_PINS_21_22:
            return "I2C: SDA=21 SCL=22";
        case SENSOR_PINS_27_22:
            return "I2C: SDA=27 SCL=22";
        case SENSOR_PINS_16_17:
        default:
            return "I2C: SDA=16 SCL=17";
    }
}

const char* SensorManager::getModelConfigText() const {
    switch (Aprs.config().sensorModel) {
        case SENSOR_BMP_BME280: return "Sensor: BME/BMP280";
        case SENSOR_DISABLED:
        default:                return "Sensor: APAGADO";
    }
}

const char* SensorManager::getBeaconModeText() const {
    switch (Aprs.config().sensorBeaconMode) {
        case SENSOR_BCN_COMMENT: return "Baliza: COMENTARIO";
        case SENSOR_BCN_WX_PACK: return "Baliza: WX OFICIAL (/_)";
        case SENSOR_BCN_OFF:
        default:                 return "Baliza: NO INFORMAR";
    }
}

void SensorManager::begin() {
    forceDetectAndRead();
}

void SensorManager::update() {
    // Solo re-leer cada 10 s si hay un sensor conectado.
    // Si no se detecto sensor al arrancar, no destruir/recrear Wire.begin()/Wire.end()
    // ni ledcSetup() repetidamente (el usuario puede re-escanear con [DETECTAR] en CONF 5/7).
    if (!_reading.connected) {
        return;
    }
    uint32_t now = millis();
    if (now - _lastPollMs >= 10000UL) {
        forceDetectAndRead();
    }
}

bool SensorManager::forceDetectAndRead() {
    _lastPollMs = millis();
    const AprsConfig& cfg = Aprs.config();

    if (cfg.sensorModel != SENSOR_BMP_BME280) {
        _reading.connected = false;
        _bmpInitialized = false;
        strlcpy(_reading.chipName, "APAGADO", sizeof(_reading.chipName));
        strlcpy(_reading.statusText, "Sensor apagado (Sin datos WX)", sizeof(_reading.statusText));
        return false;
    }

    // Bloqueo estricto: NO se permite usar sensor meteorológico junto con GPS externo
    if (cfg.posMode == POS_MODE_GPS) {
        _reading.connected = false;
        _bmpInitialized = false;
        strlcpy(_reading.chipName, "BLOQ GPS", sizeof(_reading.chipName));
        strlcpy(_reading.statusText, "GPS activo: pasa Pos a FIJA (CONF 1/7)", sizeof(_reading.statusText));
        return false;
    }

    int8_t sda = 16, scl = 17;
    getActivePins(&sda, &scl);

    // Evitar pisar el pin de Backlight activo con I2C si coincide con 27 o 21
    if (cfg.boardModel == BOARD_MODEL_ST7789 && (sda == 27 || scl == 27)) {
        _reading.connected = false;
        strlcpy(_reading.chipName, "PIN BL!", sizeof(_reading.chipName));
        strlcpy(_reading.statusText, "IO27 es Backlight en ST7789 (Usa 16/17)", sizeof(_reading.statusText));
        return false;
    }

    // Soltar PWM del LED Azul si usamos I2C en IO16/IO17
    if (sda == CYD_LED_B || scl == CYD_LED_B) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
        ledcDetach(CYD_LED_B);
#else
        ledcDetachPin(CYD_LED_B);
#endif
    }

    bool ok = initAndReadBmpBme280(sda, scl);

    if (!ok) {
        _reading.connected = false;
        _bmpInitialized = false;
        strlcpy(_reading.chipName, "SIN SENSOR", sizeof(_reading.chipName));
        snprintf(_reading.statusText, sizeof(_reading.statusText),
                 "No detectado en %s (Omitido en baliza)",
                 cfg.sensorPinProfile == SENSOR_PINS_16_17 ? "IO16/17" :
                 cfg.sensorPinProfile == SENSOR_PINS_21_22 ? "IO21/22" : "IO27/22");

        // Si usamos IO16/17 y no hay sensor conectado, cerrar Wire y restaurar el pin 17 (LED Azul tenue/apagado)
        if (cfg.sensorPinProfile == SENSOR_PINS_16_17 && cfg.posMode == POS_MODE_FIXED) {
            Wire.end();
            pinMode(CYD_LED_G, OUTPUT);
            digitalWrite(CYD_LED_G, HIGH);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
            ledcAttach(CYD_LED_B, 5000, 8);
            ledcWrite(CYD_LED_B, 255);
#else
            ledcSetup(2, 5000, 8);
            ledcAttachPin(CYD_LED_B, 2);
            ledcWrite(2, 255);
#endif
        }
    }

    return ok;
}

bool SensorManager::i2cWriteReg(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return (Wire.endTransmission() == 0);
}

bool SensorManager::i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    size_t got = Wire.requestFrom((int)addr, (int)len);
    if (got != len) return false;
    for (size_t i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }
    return true;
}

bool SensorManager::initAndReadBmpBme280(int8_t sda, int8_t scl) {
    Wire.begin(sda, scl, 100000);
    Wire.setTimeOut(50);

    static const uint8_t addrs[2] = { 0x76, 0x77 };
    uint8_t foundAddr = 0;
    uint8_t chipId = 0;

    for (uint8_t a : addrs) {
        uint8_t id = 0;
        if (i2cReadBytes(a, 0xD0, &id, 1)) {
            if (id == 0x58 || id == 0x56 || id == 0x57 || id == 0x60) {
                foundAddr = a;
                chipId = id;
                break;
            }
        }
    }

    if (foundAddr == 0) {
        return false;
    }

    if (!_bmpInitialized || _bmpAddr != foundAddr || _bmpChipId != chipId) {
        _bmpAddr = foundAddr;
        _bmpChipId = chipId;

        uint8_t cal[24];
        if (!i2cReadBytes(foundAddr, 0x88, cal, 24)) return false;

        _dig_T1 = (uint16_t)cal[0]  | ((uint16_t)cal[1]  << 8);
        _dig_T2 = (int16_t)(cal[2]  | ((uint16_t)cal[3]  << 8));
        _dig_T3 = (int16_t)(cal[4]  | ((uint16_t)cal[5]  << 8));
        _dig_P1 = (uint16_t)cal[6]  | ((uint16_t)cal[7]  << 8);
        _dig_P2 = (int16_t)(cal[8]  | ((uint16_t)cal[9]  << 8));
        _dig_P3 = (int16_t)(cal[10] | ((uint16_t)cal[11] << 8));
        _dig_P4 = (int16_t)(cal[12] | ((uint16_t)cal[13] << 8));
        _dig_P5 = (int16_t)(cal[14] | ((uint16_t)cal[15] << 8));
        _dig_P6 = (int16_t)(cal[16] | ((uint16_t)cal[17] << 8));
        _dig_P7 = (int16_t)(cal[18] | ((uint16_t)cal[19] << 8));
        _dig_P8 = (int16_t)(cal[20] | ((uint16_t)cal[21] << 8));
        _dig_P9 = (int16_t)(cal[22] | ((uint16_t)cal[23] << 8));

        if (chipId == 0x60) {
            // Calibración de humedad exclusiva de BME280
            i2cReadBytes(foundAddr, 0xA1, &_dig_H1, 1);
            uint8_t hcal[7];
            if (i2cReadBytes(foundAddr, 0xE1, hcal, 7)) {
                _dig_H2 = (int16_t)(hcal[0] | ((uint16_t)hcal[1] << 8));
                _dig_H3 = hcal[2];
                _dig_H4 = (int16_t)((hcal[3] << 4) | (hcal[4] & 0x0F));
                _dig_H5 = (int16_t)((hcal[5] << 4) | (hcal[4] >> 4));
                _dig_H6 = (int8_t)hcal[6];
            }
            i2cWriteReg(foundAddr, 0xF2, 0x01); // ctrl_hum: oversampling x1
        }

        i2cWriteReg(foundAddr, 0xF5, 0xA0); // config: standby 1000ms, filter off
        i2cWriteReg(foundAddr, 0xF4, 0x27); // ctrl_meas: temp x1, press x1, normal mode
        delay(40);
        _bmpInitialized = true;
    } else {
        // Forzar conversión si el sensor estaba en sleep
        if (chipId == 0x60) i2cWriteReg(foundAddr, 0xF2, 0x01);
        i2cWriteReg(foundAddr, 0xF4, 0x27);
    }

    uint8_t raw[8];
    size_t bytesToRead = (chipId == 0x60) ? 8 : 6;
    if (!i2cReadBytes(foundAddr, 0xF7, raw, bytesToRead)) {
        return false;
    }

    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);

    if (adc_T == 0x80000 || adc_P == 0x80000) {
        return false;
    }

    // Fórmulas de compensación oficiales Bosch BMP280/BME280
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)_dig_T1 << 1))) * ((int32_t)_dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)_dig_T1)) * ((adc_T >> 4) - ((int32_t)_dig_T1))) >> 12) * ((int32_t)_dig_T3)) >> 14;
    int32_t t_fine = var1 + var2;
    float tempC = ((t_fine * 5 + 128) >> 8) / 100.0f;

    int64_t pvar1 = ((int64_t)t_fine) - 128000;
    int64_t pvar2 = pvar1 * pvar1 * (int64_t)_dig_P6;
    pvar2 = pvar2 + ((pvar1 * (int64_t)_dig_P5) << 17);
    pvar2 = pvar2 + (((int64_t)_dig_P4) << 35);
    pvar1 = ((pvar1 * pvar1 * (int64_t)_dig_P3) >> 8) + ((pvar1 * (int64_t)_dig_P2) << 12);
    pvar1 = (((((int64_t)1) << 47) + pvar1)) * ((int64_t)_dig_P1) >> 33;

    float pressHpa = 1013.25f;
    if (pvar1 != 0) {
        int64_t p = 1048576 - adc_P;
        p = (((p << 31) - pvar2) * 3125) / pvar1;
        pvar1 = (((int64_t)_dig_P9) * (p >> 13) * (p >> 13)) >> 25;
        pvar2 = (((int64_t)_dig_P8) * p) >> 19;
        p = ((p + pvar1 + pvar2) >> 8) + (((int64_t)_dig_P7) << 4);
        pressHpa = (float)p / 25600.0f;
    }

    const AprsConfig& cfg = Aprs.config();
    _reading.connected    = true;
    _reading.hasPressure  = (pressHpa > 300.0f && pressHpa < 1200.0f);
    _reading.temperatureC = tempC + cfg.tempOffsetC;
    _reading.pressureHpa  = pressHpa;
    _reading.i2cAddr      = foundAddr;
    _reading.lastReadMs   = millis();

    if (chipId == 0x60) {
        int32_t adc_H = ((int32_t)raw[6] << 8) | raw[7];
        int32_t v_x1 = (t_fine - ((int32_t)76800));
        v_x1 = (((((adc_H << 14) - (((int32_t)_dig_H4) << 20) - (((int32_t)_dig_H5) * v_x1)) + ((int32_t)16384)) >> 15) *
                (((((((v_x1 * ((int32_t)_dig_H6)) >> 10) * (((v_x1 * ((int32_t)_dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) *
                  ((int32_t)_dig_H2) + 8192) >> 14));
        v_x1 = (v_x1 - (((((v_x1 >> 15) * (v_x1 >> 15)) >> 7) * ((int32_t)_dig_H1)) >> 4));
        if (v_x1 < 0) v_x1 = 0;
        if (v_x1 > 419430400) v_x1 = 419430400;
        float hum = (float)(v_x1 >> 12) / 1024.0f;
        _reading.hasHumidity = true;
        _reading.humidityPct = constrain(hum + cfg.humOffsetPct, 0.0f, 100.0f);
        strlcpy(_reading.chipName, "BME280", sizeof(_reading.chipName));
        snprintf(_reading.statusText, sizeof(_reading.statusText), "BME280 OK (0x%02X T+H+P)", foundAddr);
    } else {
        _reading.hasHumidity = false;
        _reading.humidityPct = 0.0f;
        strlcpy(_reading.chipName, "BMP280", sizeof(_reading.chipName));
        snprintf(_reading.statusText, sizeof(_reading.statusText), "BMP280 OK (0x%02X Temp+Pres)", foundAddr);
    }

    return true;
}

bool SensorManager::formatCommentTelemetry(char* outBuf, size_t maxLen) const {
    if (!_reading.connected || Aprs.config().sensorBeaconMode == SENSOR_BCN_OFF) {
        outBuf[0] = '\0';
        return false;
    }

    if (_reading.hasHumidity && _reading.hasPressure) {
        snprintf(outBuf, maxLen, "%.1fC %.0f%% %.0fhPa",
                 _reading.temperatureC, _reading.humidityPct, _reading.pressureHpa);
    } else if (_reading.hasPressure) {
        snprintf(outBuf, maxLen, "%.1fC %.1fhPa",
                 _reading.temperatureC, _reading.pressureHpa);
    } else if (_reading.hasHumidity) {
        snprintf(outBuf, maxLen, "%.1fC %.0f%%",
                 _reading.temperatureC, _reading.humidityPct);
    } else {
        snprintf(outBuf, maxLen, "%.1fC", _reading.temperatureC);
    }
    return true;
}

bool SensorManager::formatAprsWxPayload(char* outBuf, size_t maxLen) const {
    if (!_reading.connected || Aprs.config().sensorBeaconMode != SENSOR_BCN_WX_PACK) {
        outBuf[0] = '\0';
        return false;
    }

    // Estándar APRS WX:
    // _c...s...g...tXXX (Temperatura en grados Fahrenheit, 3 dígitos)
    // hXX (Humedad 01..99, 00 = 100%)
    // bXXXXX (Presión barométrica en décimas de hPa/mbar, 5 dígitos, ej. 10132 = 1013.2 hPa)
    int tempF = (int)roundf(_reading.temperatureC * 1.8f + 32.0f);
    char tStr[8];
    if (tempF < 0) {
        snprintf(tStr, sizeof(tStr), "t-%02d", abs(tempF) % 100);
    } else {
        snprintf(tStr, sizeof(tStr), "t%03d", tempF % 1000);
    }

    char hStr[8] = "";
    if (_reading.hasHumidity) {
        int h = (int)roundf(_reading.humidityPct);
        if (h >= 100) h = 0; // En APRS WX, h00 representa 100%
        else if (h < 1) h = 1;
        snprintf(hStr, sizeof(hStr), "h%02d", h);
    }

    char bStr[12] = "";
    if (_reading.hasPressure) {
        int baroTenths = (int)roundf(_reading.pressureHpa * 10.0f);
        if (baroTenths > 0 && baroTenths < 20000) {
            snprintf(bStr, sizeof(bStr), "b%05d", baroTenths);
        }
    }

    snprintf(outBuf, maxLen, ".../...g...%s%s%s", tStr, hStr, bStr);
    return true;
}
