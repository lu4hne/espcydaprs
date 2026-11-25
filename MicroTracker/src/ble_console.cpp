#include "ble_console.h"
#include <NimBLEDevice.h>

// UUIDs estándar Nordic UART Service (NUS) — compatible con Web Bluetooth, Serial Bluetooth Terminal y Bluefruit
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHAR_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_CHAR_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BleConsole Ble;

static NimBLEServer*         s_server = nullptr;
static NimBLECharacteristic* s_txChar = nullptr;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        Ble.onClientConnect();
        Serial.println(F("[BLE] Telefono conectado a consola NUS"));
    }
    void onDisconnect(NimBLEServer* pServer) override {
        Ble.onClientDisconnect();
        Serial.println(F("[BLE] Telefono desconectado. Reiniciando advertising..."));
        NimBLEDevice::startAdvertising();
    }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar) override {
        std::string val = pChar->getValue();
        if (!val.empty()) {
            Ble.onRxData(reinterpret_cast<const uint8_t*>(val.data()), val.size());
        }
    }
};

void BleConsole::begin(const char* deviceName, ConsoleCmdCallback cmdCb) {
    _cmdCb = cmdCb;
    _rxLen = 0;
    _cmdReady = false;

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P3); // Bajo consumo, ideal para SuperMini

    s_server = NimBLEDevice::createServer();
    s_server->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = s_server->createService(NUS_SERVICE_UUID);

    s_txChar = pService->createCharacteristic(
        NUS_TX_CHAR_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    NimBLECharacteristic* pRxChar = pService->createCharacteristic(
        NUS_RX_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pRxChar->setCallbacks(new RxCallbacks());

    pService->start();

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(NUS_SERVICE_UUID);
    pAdv->setScanResponse(true);
    pAdv->start();

    Serial.printf("[BLE] Consola NUS activa como '%s'\n", deviceName);
}

void BleConsole::onRxData(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if (c == '\r' || c == '\n') {
            if (_rxLen > 0) {
                _rxBuf[_rxLen] = '\0';
                strlcpy(_pendingCmd, _rxBuf, sizeof(_pendingCmd));
                _cmdReady = true;
                _rxLen = 0;
            }
        } else if (_rxLen < sizeof(_rxBuf) - 1) {
            _rxBuf[_rxLen++] = c;
        }
    }
}

void BleConsole::update() {
    if (_cmdReady) {
        char cmdCopy[160];
        strlcpy(cmdCopy, _pendingCmd, sizeof(cmdCopy));
        _cmdReady = false;
        if (_cmdCb) {
            _cmdCb(cmdCopy);
        }
    }
}

void BleConsole::sendLine(const char* line) {
    if (!_connected || !s_txChar || !line) return;

    // Enviar en bloques seguros <= 120 bytes terminados en '\n'
    size_t len = strlen(line);
    char chunk[128];
    size_t offset = 0;
    while (offset < len) {
        size_t n = len - offset;
        if (n > 118) n = 118;
        memcpy(chunk, line + offset, n);
        offset += n;
        if (offset >= len) {
            chunk[n++] = '\n';
        }
        s_txChar->setValue(reinterpret_cast<const uint8_t*>(chunk), n);
        s_txChar->notify();
        if (offset < len) delay(8);
    }
}
