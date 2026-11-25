#pragma once

#include <Arduino.h>

// Callback para procesar comandos recibidos por BLE (y también desde USB Serial)
typedef void (*ConsoleCmdCallback)(const char* cmdLine);

class BleConsole {
public:
    void begin(const char* deviceName, ConsoleCmdCallback cmdCb);
    void update();
    void sendLine(const char* line);
    bool isConnected() const { return _connected; }

    // Uso interno desde callbacks NimBLE
    void onClientConnect() { _connected = true; }
    void onClientDisconnect() { _connected = false; }
    void onRxData(const uint8_t* data, size_t len);

private:
    volatile bool _connected = false;
    ConsoleCmdCallback _cmdCb = nullptr;

    char   _rxBuf[160];
    size_t _rxLen = 0;
    volatile bool _cmdReady = false;
    char   _pendingCmd[160];
};

extern BleConsole Ble;
