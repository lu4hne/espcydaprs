#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

class CST820Touch {
public:
    CST820Touch(int8_t sdaPin = CST820_I2C_SDA,
                int8_t sclPin = CST820_I2C_SCL,
                int8_t rstPin = CST820_I2C_RST,
                int8_t intPin = CST820_I2C_INT);

    void begin();
    bool getTouch(uint16_t* x, uint16_t* y, uint8_t rotation = 1);
    void setInvertAxes(bool invX, bool invY) { _invX = invX; _invY = invY; }

private:
    int8_t _sda, _scl, _rst, _int;
    bool _invX = false;
    bool _invY = false;

    bool readRegisters(uint8_t reg, uint8_t* data, uint8_t len);
    void writeRegister(uint8_t reg, uint8_t val);
};

