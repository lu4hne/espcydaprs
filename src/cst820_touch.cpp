#include "cst820_touch.h"

CST820Touch::CST820Touch(int8_t sdaPin, int8_t sclPin, int8_t rstPin, int8_t intPin)
    : _sda(sdaPin), _scl(sclPin), _rst(rstPin), _int(intPin) {}

void CST820Touch::begin() {
    // 1. Probar primero si existe controlador Touch Capacitivo I2C (CST820 0x15 / FT6336 0x38)
    if (_rst >= 0) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, LOW);
        delay(10);
        digitalWrite(_rst, HIGH);
        delay(50);
    }

    _isCapacitive = false;
    Wire.begin(_sda, _scl, 400000);

    static const uint8_t ADDR_CANDIDATES[2] = { 0x15, 0x38 };
    for (uint8_t addr : ADDR_CANDIDATES) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            _isCapacitive = true;
            _i2cAddr = addr;
            if (addr == 0x15) {
                writeRegister(0xFE, 0xFF); // Desactivar auto-sleep en CST820
            }
            Serial.printf("[TOUCH] Detectado panel Capacitivo I2C (addr=0x%02X)\n", addr);
            return;
        }
    }

    // 2. Si no respondió ningún dispositivo I2C, liberar pines 33/32 y configurar XPT2046 Resistivo
    Wire.end();
    pinMode(XPT2046_IRQ_PIN, INPUT);
    pinMode(XPT2046_MISO_PIN, INPUT);
    pinMode(XPT2046_MOSI_PIN, OUTPUT);
    pinMode(XPT2046_CLK_PIN, OUTPUT);
    pinMode(XPT2046_CS_PIN, OUTPUT);

    digitalWrite(XPT2046_CS_PIN, HIGH);
    digitalWrite(XPT2046_CLK_PIN, LOW);
    digitalWrite(XPT2046_MOSI_PIN, LOW);
    Serial.println(F("[TOUCH] Modo Resistivo XPT2046 activo (CLK=25, MOSI=32, MISO=39, CS=33)"));
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
        delayMicroseconds(1);
        digitalWrite(XPT2046_CLK_PIN, LOW);
        delayMicroseconds(1);
    }

    uint16_t val = 0;
    for (int i = 15; i >= 0; i--) {
        digitalWrite(XPT2046_CLK_PIN, HIGH);
        delayMicroseconds(1);
        if (digitalRead(XPT2046_MISO_PIN)) {
            val |= (1 << i);
        }
        digitalWrite(XPT2046_CLK_PIN, LOW);
        delayMicroseconds(1);
    }
    return (val >> 3) & 0x0FFF;
}

bool CST820Touch::readXpt2046(uint16_t* outX, uint16_t* outY, uint8_t rotation) {
    digitalWrite(XPT2046_CS_PIN, LOW);
    delayMicroseconds(2);

    uint16_t z1 = xptTransfer16(0xB1);
    uint16_t z2 = xptTransfer16(0xC1);

    if (z1 < 90 || z1 >= 3980) {
        xptTransfer16(0xD0);
        digitalWrite(XPT2046_CS_PIN, HIGH);
        return false;
    }

    int32_t z = (int32_t)z1 + 4095 - (int32_t)z2;
    if (z < 400) {
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

    uint16_t rawX = (samplesX[1] + samplesX[2] + samplesX[3]) / 3;
    uint16_t rawY = (samplesY[1] + samplesY[2] + samplesX[3] * 0 + samplesY[3]) / 3;

    if (rawX <= 120 || rawX >= 3960 || rawY <= 120 || rawY >= 3960) {
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
    return true;
}

bool CST820Touch::readRegisters(uint8_t reg, uint8_t* data, uint8_t len) {
    Wire.beginTransmission(_i2cAddr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    uint8_t readCount = Wire.requestFrom(_i2cAddr, len);
    if (readCount != len) {
        return false;
    }
    for (uint8_t i = 0; i < len; i++) {
        data[i] = Wire.read();
    }
    return true;
}

void CST820Touch::writeRegister(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_i2cAddr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}
