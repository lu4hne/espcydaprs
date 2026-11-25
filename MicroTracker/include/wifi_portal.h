#pragma once

#include <Arduino.h>
#include <WebServer.h>

class WifiPortal {
public:
    void start(uint32_t timeoutSec = 300);
    void stop();
    void update();

    bool isActive() const { return _active; }
    const char* getApSsid() const { return _apSsid; }

private:
    WebServer _server{80};
    bool      _active = false;
    uint32_t  _lastActivityMs = 0;
    uint32_t  _timeoutMs = 300000UL;
    char      _apSsid[32] = "";

    void handleRoot();
    void handleSave();
    void handleOtaUpload();
};

extern WifiPortal Portal;
