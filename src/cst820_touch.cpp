#include "cst820_touch.h"

CST820Touch::CST820Touch(int8_t sdaPin, int8_t sclPin, int8_t rstPin, int8_t intPin)
    : _sda(sdaPin), _scl(sclPin), _rst(rstPin), _int(intPin) {}

void CST820Touch::begin() {
    if (_int >= 0) {
        pinMode(_int, INPUT);
    }
    if (_rst >= 0) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, LOW);
        delay(10);
        digitalWrite(_rst, HIGH);
        delay(50);
    }

    Wire.begin(_sda, _scl, 400000);
    // Desactivar auto-sleep del CST820 para respuesta instantánea
    writeRegister(0xFE, 0xFF);
}

bool CST820Touch::getTouch(uint16_t* x, uint16_t* y, uint8_t rotation) {
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
        case 0: // Portrait 240x320
            outX = rawX;
            outY = rawY;
            break;
        case 1: // Landscape 320x240 (Estándar para 2432S024C)
            outX = rawY;
            outY = 239 - rawX;
            break;
        case 2: // Portrait invertido
            outX = 239 - rawX;
            outY = 319 - rawY;
            break;
        case 3: // Landscape invertido
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

bool CST820Touch::readRegisters(uint8_t reg, uint8_t* data, uint8_t len) {
    Wire.beginTransmission(CST820_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    uint8_t readCount = Wire.requestFrom((uint8_t)CST820_I2C_ADDR, len);
    if (readCount != len) {
        return false;
    }
    for (uint8_t i = 0; i < len; i++) {
        data[i] = Wire.read();
    }
    return true;
}

void CST820Touch::writeRegister(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(CST820_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

