#include "cst820_touch.h"
#include <driver/gpio.h>

CST820Touch::CST820Touch(int8_t sdaPin, int8_t sclPin, int8_t rstPin, int8_t intPin)
    : _sda(sdaPin), _scl(sclPin), _rst(rstPin), _int(intPin) {}

void CST820Touch::begin(TFT_eSPI* tft, bool forceResistive) {
    _tft = tft;
    _isCapacitive = false;

    // Si la placa se declara como ILI9341 (CYD 2.8" ESP32-2432S028R) o se fuerza resistivo,
    // OMITIR la prueba I2C en los pines 33/32 para evitar enviar pulsos a CS y MOSI del XPT2046
    // y evitar falsas detecciones por bus I2C sin pull-ups.
    if (!forceResistive) {
        // 1. Pulso de reset en GPIO 25 por si la placa tiene Touch Capacitivo CST820
        if (_rst >= 0) {
            pinMode(_rst, OUTPUT);
            digitalWrite(_rst, LOW);
            delay(10);
            digitalWrite(_rst, HIGH);
            delay(50);
        }

        // 2. Probar en Wire1 si existe controlador Capacitivo I2C (CST820 0x15 / FT6336 0x38 / GT911 0x5D)
        Wire1.end();
        if (Wire1.begin(_sda, _scl, 400000)) {
            static const uint8_t ADDR_CANDIDATES[3] = { 0x15, 0x38, 0x5D };
            for (uint8_t addr : ADDR_CANDIDATES) {
                Wire1.beginTransmission(addr);
                if (Wire1.endTransmission() == 0) {
                    _i2cAddr = addr;
                    uint8_t dummy = 0;
                    // Validar que realmente responda con lectura de registro para evitar falsos positivos
                    if (readRegisters(0x00, &dummy, 1)) {
                        _isCapacitive = true;
                        if (addr == 0x15) {
                            writeRegister(0xFE, 0xFF); // Desactivar auto-sleep en CST820
                        }
                        Serial.printf("[TOUCH] Detectado panel Capacitivo I2C (addr=0x%02X en SDA=%d, SCL=%d)\n",
                                      addr, _sda, _scl);
                        return;
                    }
                }
            }
        }
        Wire1.end();
    }

    // 3. Configurar pines dedicados para Touch Resistivo XPT2046 / HR2046
    //    Limpiar el modo Open-Drain que el driver I2C del ESP32 deja en GPIO 32 y 33
    gpio_reset_pin((gpio_num_t)XPT2046_CLK_PIN);
    gpio_reset_pin((gpio_num_t)XPT2046_MOSI_PIN);
    gpio_reset_pin((gpio_num_t)XPT2046_CS_PIN);
    gpio_reset_pin((gpio_num_t)XPT2046_IRQ_PIN);
    gpio_reset_pin((gpio_num_t)XPT2046_MISO_PIN);

    pinMode(XPT2046_IRQ_PIN, INPUT);
    pinMode(XPT2046_MISO_PIN, INPUT);
    pinMode(XPT2046_MOSI_PIN, OUTPUT);
    pinMode(XPT2046_CLK_PIN, OUTPUT);
    pinMode(XPT2046_CS_PIN, OUTPUT);

    digitalWrite(XPT2046_CS_PIN, HIGH);
    digitalWrite(XPT2046_CLK_PIN, LOW);
    digitalWrite(XPT2046_MOSI_PIN, LOW);

    // Lectura dummy a modo reposo (PD[1:0]=00) para dejar PENIRQ habilitado
    xptRead12(0xD0);

    Serial.println(F("[TOUCH] Modo Resistivo XPT2046/HR2046 activo (CLK=25, MOSI=32, MISO=39, CS=33, IRQ=36)"));
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

// Lectura de 12 bits de un canal del XPT2046 con protocolo SPI directo
uint16_t CST820Touch::xptRead12(uint8_t cmd) {
    digitalWrite(XPT2046_CS_PIN, LOW);
    digitalWrite(XPT2046_CLK_PIN, LOW);
    delayMicroseconds(2);

    // 1. Enviar los 8 bits de comando (Clocks 1..8)
    for (int i = 7; i >= 0; i--) {
        digitalWrite(XPT2046_MOSI_PIN, (cmd >> i) & 0x01);
        delayMicroseconds(1);
        digitalWrite(XPT2046_CLK_PIN, HIGH);
        delayMicroseconds(2);
        digitalWrite(XPT2046_CLK_PIN, LOW);
        delayMicroseconds(1);
    }

    // 2. CRITICO: Bajar MOSI antes de la lectura de datos para no enviar nuevos START bits
    digitalWrite(XPT2046_MOSI_PIN, LOW);
    delayMicroseconds(1);

    // 3. Clock 9: Pulso de reloj para el bit BUSY del convertidor SAR
    digitalWrite(XPT2046_CLK_PIN, HIGH);
    delayMicroseconds(2);
    digitalWrite(XPT2046_CLK_PIN, LOW);
    delayMicroseconds(1);

    // 4. Clocks 10 a 21: Leer los 12 bits de datos (D11 a D0, MSB primero)
    uint16_t val = 0;
    for (int i = 0; i < 12; i++) {
        digitalWrite(XPT2046_CLK_PIN, HIGH);
        delayMicroseconds(2);
        val = (val << 1) | (digitalRead(XPT2046_MISO_PIN) ? 1 : 0);
        digitalWrite(XPT2046_CLK_PIN, LOW);
        delayMicroseconds(1);
    }

    // 4. Finalizar conversion deseleccionando el chip (CS a HIGH)
    digitalWrite(XPT2046_CS_PIN, HIGH);
    delayMicroseconds(2);

    return val;
}

bool CST820Touch::readDedicatedXpt2046(uint16_t* rawX, uint16_t* rawY, uint16_t* rawZ) {
    // 1. Pin de hardware T_IRQ (GPIO 36):
    // Cuando no se toca la pantalla, T_IRQ esta en HIGH (pull-up a 3.3V).
    // Al presionar la pantalla, PENIRQ del XPT2046 conmuta a LOW.
    bool irqActive = (digitalRead(XPT2046_IRQ_PIN) == LOW);

    // 2. Medir presion Z1 y Z2
    uint16_t z1 = xptRead12(0xB0);
    uint16_t z2 = xptRead12(0xC0);
    int32_t z = (int32_t)z1 + 4095 - (int32_t)z2;

    // Si IRQ esta en HIGH y la presion es nula o muy baja (< 150), no hay toque fisico
    if (!irqActive && z < 150) {
        return false;
    }
    // Descartar toques con presion inferior al umbral minimo
    if (z < 80) {
        return false;
    }

    // 3. Muestreo de posicion X e Y (5 muestras con descarte de extremos)
    uint16_t samplesX[5];
    uint16_t samplesY[5];
    for (int i = 0; i < 5; i++) {
        samplesX[i] = xptRead12(0xD0); // Canal X
        samplesY[i] = xptRead12(0x90); // Canal Y
    }

    // Ordenar para filtro de mediana
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

    uint16_t rx = samplesX[2]; // Mediana X
    uint16_t ry = samplesY[2]; // Mediana Y

    // Descartar lecturas en corto o fuera de rango
    if (rx < 80 || rx > 4050 || ry < 80 || ry > 4050) {
        return false;
    }

    *rawX = rx;
    *rawY = ry;
    *rawZ = (z > 0) ? (uint16_t)z : 0;
    return true;
}

bool CST820Touch::readXpt2046(uint16_t* outX, uint16_t* outY, uint8_t rotation) {
    uint16_t rawX = 0, rawY = 0, rawZ = 0;

    if (!readDedicatedXpt2046(&rawX, &rawY, &rawZ)) {
        return false;
    }

    int32_t mx = 0;
    int32_t my = 0;

    // Calibración exacta CYD 2.8" (ESP32-2432S028R) en modo Apaisado (rotation=1):
    // El eje horizontal (Pantalla X: 0..319) varía con el canal Y (0x90): ~200 (Izq) a ~3800 (Der)
    // El eje vertical (Pantalla Y: 0..239) varía con el canal X (0xD0): ~250 (Arriba) a ~3850 (Abajo)
    if (_swapAxes) {
        mx = map((long)rawX, 200, 3800, 0, 319);
        my = map((long)rawY, 250, 3850, 0, 239);
    } else {
        mx = map((long)rawY, 200, 3800, 0, 319);
        my = map((long)rawX, 250, 3850, 0, 239);
    }

    mx = constrain(mx, 0, 319);
    my = constrain(my, 0, 239);

    if (_invX) mx = 319 - mx;
    if (_invY) my = 239 - my;

    *outX = (uint16_t)mx;
    *outY = (uint16_t)my;

    Serial.printf("[TOUCH] screen=(%u,%u) raw=(X=%u, Y=%u, Z=%u) [swap=%d, invX=%d, invY=%d]\n",
                  *outX, *outY, rawX, rawY, rawZ, _swapAxes ? 1 : 0, _invX ? 1 : 0, _invY ? 1 : 0);
    return true;
}

void CST820Touch::printDiagnostics() {
    bool irq = (digitalRead(XPT2046_IRQ_PIN) == LOW);
    uint16_t z1 = xptRead12(0xB0);
    uint16_t z2 = xptRead12(0xC0);
    int32_t z = (int32_t)z1 + 4095 - (int32_t)z2;
    uint16_t x = xptRead12(0xD0);
    uint16_t y = xptRead12(0x90);

    Serial.printf("[TOUCH DIAG] IRQ36=%d (contacto=%s) | rawX=%u, rawY=%u, Z1=%u, Z2=%u (z=%d) | CapI2C=%d | swap=%d, invX=%d, invY=%d\n",
                  digitalRead(XPT2046_IRQ_PIN),
                  irq ? "SI" : "NO",
                  x, y, z1, z2, z,
                  _isCapacitive ? 1 : 0,
                  _swapAxes ? 1 : 0, _invX ? 1 : 0, _invY ? 1 : 0);
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
