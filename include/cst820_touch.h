#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include "config.h"

class CST820Touch {
public:
    CST820Touch(int8_t sdaPin = CST820_I2C_SDA,
                int8_t sclPin = CST820_I2C_SCL,
                int8_t rstPin = CST820_I2C_RST,
                int8_t intPin = CST820_I2C_INT);

    void begin(TFT_eSPI* tft = nullptr);
    bool getTouch(uint16_t* x, uint16_t* y, uint8_t rotation = 1);
    void setInvertAxes(bool invX, bool invY) { _invX = invX; _invY = invY; }
    bool isCapacitive() const { return _isCapacitive; }
    void printDiagnostics();

private:
    int8_t _sda, _scl, _rst, _int;
    uint8_t _i2cAddr = CST820_I2C_ADDR;
    bool _isCapacitive = false;
    bool _invX = false;
    bool _invY = false;
    TFT_eSPI* _tft = nullptr;

    bool readRegisters(uint8_t reg, uint8_t* data, uint8_t len);
    void writeRegister(uint8_t reg, uint8_t val);

    // Lectura de panel táctil resistivo XPT2046:
    // 1) Bus SPI dedicado (CLK=25, MOSI=32, MISO=39, CS=33, IRQ=36)
    // 2) Bus HSPI compartido con TFT (SCLK=14, MOSI=13, MISO=12, CS=33)
    uint16_t xptTransfer16(uint8_t cmd);
    bool readDedicatedXpt2046(uint16_t* rawX, uint16_t* rawY, uint16_t* rawZ);
    bool readHspiXpt2046(uint16_t* rawX, uint16_t* rawY, uint16_t* rawZ);
    bool readXpt2046(uint16_t* outX, uint16_t* outY, uint8_t rotation);
};
