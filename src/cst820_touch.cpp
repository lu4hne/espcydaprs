#include "cst820_touch.h"
#include <driver/gpio.h>

CST820Touch::CST820Touch(int8_t sdaPin, int8_t sclPin, int8_t rstPin, int8_t intPin)
    : _sda(sdaPin), _scl(sclPin), _rst(rstPin), _int(intPin) {}

void CST820Touch::begin(TFT_eSPI* tft) {
    _tft = tft;

    // 1. Pulso de reset en GPIO 25 por si la placa tiene Touch Capacitivo CST820
    if (_rst >= 0) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, LOW);
        delay(10);
        digitalWrite(_rst, HIGH);
        delay(50);
    }

    // 2. Probar en Wire1 si existe controlador Capacitivo I2C (CST820 0x15 / FT6336 0x38 / GT911 0x5D)
    _isCapacitive = false;
    Wire1.end();
    if (Wire1.begin(_sda, _scl, 400000)) {
        static const uint8_t ADDR_CANDIDATES[3] = { 0x15, 0x38, 0x5D };
        for (uint8_t addr : ADDR_CANDIDATES) {
            Wire1.beginTransmission(addr);
            if (Wire1.endTransmission() == 0) {
                _isCapacitive = true;
                _i2cAddr = addr;
                if (addr == 0x15) {
                    writeRegister(0xFE, 0xFF); // Desactivar auto-sleep en CST820
                }
                Serial.printf("[TOUCH] Detectado panel Capacitivo I2C (addr=0x%02X en SDA=%d, SCL=%d)\n",
                              addr, _sda, _scl);
                return;
            }
        }
    }

    // 3. Si no respondió ningún dispositivo I2C, cerrar Wire1 y LIMPIAR el modo Open-Drain
    //    que el driver I2C del ESP32 deja activado en GPIO 32 (MOSI) y GPIO 33 (CS).
    Wire1.end();
    gpio_reset_pin((gpio_num_t)XPT2046_CLK_PIN);
    gpio_reset_pin((gpio_num_t)XPT2046_MOSI_PIN);
    gpio_reset_pin((gpio_num_t)XPT2046_CS_PIN);

    pinMode(XPT2046_IRQ_PIN, INPUT);
    pinMode(XPT2046_MISO_PIN, INPUT);
    pinMode(XPT2046_MOSI_PIN, OUTPUT);
    pinMode(XPT2046_CLK_PIN, OUTPUT);
    pinMode(XPT2046_CS_PIN, OUTPUT);

    digitalWrite(XPT2046_CS_PIN, HIGH);
    digitalWrite(XPT2046_CLK_PIN, LOW);
    digitalWrite(XPT2046_MOSI_PIN, LOW);

    Serial.println(F("[TOUCH] Modo Resistivo XPT2046 Dual activo (SPI 25/32/39/33 + HSPI 14/13/12/33)"));
}

bool CST820Touch::getTouch(uint16_t* x, uint16_t* y, uint8_t rotation) {
    if (!_isCapacitive) {
        return readXpt2046(x, y, rotation);
    }

    uint8_t data[6] = {0};
    if (!readRegisters(0x02, data, 5)) {
        return false;
    }

    uint8_t fingerNum = data[0] & 0x0F;
    if (fingerNum == 0 || fingerNum > 2) {
        return false;
    }

    uint16_t rawX = ((uint16_t)(data[1] & 0x0F) << 8) | data[2];
    uint16_t rawY = ((uint16_t)(data[3] & 0x0F) << 8) | data[4];

    if (rawX >= 240) rawX = 239;
    if (rawY >= 320) rawY = 319;

    uint16_t outX = rawX;
    uint16_t outY = rawY;

    switch (rotation & 3) {
        case 0:
            outX = rawX;
            outY = rawY;
            break;
        case 1:
            outX = rawY;
            outY = 239 - rawX;
            break;
        case 2:
            outX = 239 - rawX;
            outY = 319 - rawY;
            break;
        case 3:
            outX = 319 - rawY;
            outY = rawX;
            break;
    }

    if (_invX) outX = 319 - outX;
    if (_invY) outY = 239 - outY;

    *x = outX;
    *y = outY;
    return true;
}

uint16_t CST820Touch::xptTransfer16(uint8_t cmd) {
    for (int i = 7; i >= 0; i--) {
        digitalWrite(XPT2046_MOSI_PIN, (cmd >> i) & 0x01);
        digitalWrite(XPT2046_CLK_PIN, HIGH);
        delayMicroseconds(2);
        digitalWrite(XPT2046_CLK_PIN, LOW);
        delayMicroseconds(2);
    }

    uint16_t val = 0;
    for (int i = 15; i >= 0; i--) {
        digitalWrite(XPT2046_CLK_PIN, HIGH);
        delayMicroseconds(2);
        if (digitalRead(XPT2046_MISO_PIN)) {
            val |= (1 << i);
        }
        digitalWrite(XPT2046_CLK_PIN, LOW);
        delayMicroseconds(2);
    }
    return (val >> 3) & 0x0FFF;
}

bool CST820Touch::readDedicatedXpt2046(uint16_t* rawX, uint16_t* rawY, uint16_t* rawZ) {
    digitalWrite(XPT2046_CS_PIN, LOW);
    delayMicroseconds(4);

    uint16_t z1 = xptTransfer16(0xB1);
    uint16_t z2 = xptTransfer16(0xC1);

    if (z1 < 60 || z1 >= 3980) {
        xptTransfer16(0xD0);
        digitalWrite(XPT2046_CS_PIN, HIGH);
        return false;
    }

    int32_t z = (int32_t)z1 + 4095 - (int32_t)z2;
    if (z < 300) {
        xptTransfer16(0xD0);
        digitalWrite(XPT2046_CS_PIN, HIGH);
        return false;
    }

    uint16_t samplesX[5];
    uint16_t samplesY[5];
    for (int i = 0; i < 5; i++) {
        samplesY[i] = xptTransfer16(0x91);
        samplesX[i] = xptTransfer16(0xD1);
    }
    xptTransfer16(0xD0);
    digitalWrite(XPT2046_CS_PIN, HIGH);

    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 5; j++) {
            if (samplesX[j] < samplesX[i]) {
                uint16_t t = samplesX[i]; samplesX[i] = samplesX[j]; samplesX[j] = t;
            }
            if (samplesY[j] < samplesY[i]) {
                uint16_t t = samplesY[i]; samplesY[i] = samplesY[j]; samplesY[j] = t;
            }
        }
    }

    uint16_t rx = (samplesX[1] + samplesX[2] + samplesX[3]) / 3;
    uint16_t ry = (samplesY[1] + samplesY[2] + samplesY[3]) / 3;

    if (rx <= 100 || rx >= 3980 || ry <= 100 || ry >= 3980) {
        return false;
    }

    *rawX = rx;
    *rawY = ry;
    *rawZ = (uint16_t)z;
    return true;
}

bool CST820Touch::readHspiXpt2046(uint16_t* rawX, uint16_t* rawY, uint16_t* rawZ) {
    if (!_tft) return false;

    uint16_t z = _tft->getTouchRawZ();
    if (z < 300 || z >= 3980) {
        return false;
    }

    uint16_t rx1 = 0, ry1 = 0, rx2 = 0, ry2 = 0;
    _tft->getTouchRaw(&rx1, &ry1);
    delayMicroseconds(200);
    _tft->getTouchRaw(&rx2, &ry2);

    if (rx1 <= 100 || rx1 >= 3980 || ry1 <= 100 || ry1 >= 3980) return false;
    if (abs((int)rx1 - (int)rx2) > 50 || abs((int)ry1 - (int)ry2) > 50) return false;

    // En TFT_eSPI getTouchRaw: x corresponde a 0xD0 e y corresponde a 0x90
    *rawX = (rx1 + rx2) / 2;
    *rawY = (ry1 + ry2) / 2;
    *rawZ = z;
    return true;
}

bool CST820Touch::readXpt2046(uint16_t* outX, uint16_t* outY, uint8_t rotation) {
    uint16_t rawX = 0, rawY = 0, rawZ = 0;
    const char* busName = nullptr;

    // 1. Intentar XPT2046 en bus SPI dedicado (CLK=25, MOSI=32, MISO=39, CS=33)
    if (readDedicatedXpt2046(&rawX, &rawY, &rawZ)) {
        busName = "SPI(25/32/39/33)";
    }
    // 2. Si no hubo toque allí, intentar XPT2046 en bus HSPI compartido (SCLK=14, MOSI=13, MISO=12, CS=33)
    else if (readHspiXpt2046(&rawX, &rawY, &rawZ)) {
        busName = "HSPI(14/13/12/33)";
    } else {
        return false;
    }

    int32_t mx = map((long)rawX, 240, 3800, 0, 319);
    int32_t my = map((long)rawY, 240, 3800, 0, 239);
    mx = constrain(mx, 0, 319);
    my = constrain(my, 0, 239);

    if (rotation == 3) {
        mx = 319 - mx;
        my = 239 - my;
    }
    if (_invX) mx = 319 - mx;
    if (_invY) my = 239 - my;

    *outX = (uint16_t)mx;
    *outY = (uint16_t)my;

    Serial.printf("[TOUCH] screen=(%u,%u) raw=(%u,%u,z=%u) bus=%s\n",
                  *outX, *outY, rawX, rawY, rawZ, busName);
    return true;
}

void CST820Touch::printDiagnostics() {
    digitalWrite(XPT2046_CS_PIN, LOW);
    delayMicroseconds(4);
    uint16_t z1_25 = xptTransfer16(0xB1);
    uint16_t z2_25 = xptTransfer16(0xC1);
    uint16_t y_25  = xptTransfer16(0x91);
    uint16_t x_25  = xptTransfer16(0xD0);
    digitalWrite(XPT2046_CS_PIN, HIGH);

    uint16_t z_hspi = 0, x_hspi = 0, y_hspi = 0;
    if (_tft) {
        z_hspi = _tft->getTouchRawZ();
        _tft->getTouchRaw(&x_hspi, &y_hspi);
    }

    Serial.printf("[TOUCH DIAG] IRQ36=%d | SPI25(x=%u,y=%u,z1=%u,z2=%u) | HSPI14(x=%u,y=%u,z=%u) | CapI2C=%d\n",
                  digitalRead(XPT2046_IRQ_PIN),
                  x_25, y_25, z1_25, z2_25,
                  x_hspi, y_hspi, z_hspi,
                  _isCapacitive ? 1 : 0);
}

bool CST820Touch::readRegisters(uint8_t reg, uint8_t* data, uint8_t len) {
    Wire1.beginTransmission(_i2cAddr);
    Wire1.write(reg);
    if (Wire1.endTransmission(false) != 0) {
        return false;
    }
    uint8_t readCount = Wire1.requestFrom(_i2cAddr, len);
    if (readCount != len) {
        return false;
    }
    for (uint8_t i = 0; i < len; i++) {
        data[i] = Wire1.read();
    }
    return true;
}

void CST820Touch::writeRegister(uint8_t reg, uint8_t val) {
    Wire1.beginTransmission(_i2cAddr);
    Wire1.write(reg);
    Wire1.write(val);
    Wire1.endTransmission();
}
