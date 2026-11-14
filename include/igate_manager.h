#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "ax25_aprs.h"

enum IgateState : uint8_t {
    IGATE_DISABLED      = 0,
    IGATE_WIFI_WAIT     = 1,
    IGATE_TCP_CONNECTING= 2,
    IGATE_CONNECTED     = 3,
    IGATE_VERIFIED      = 4,
    IGATE_ERROR         = 5
};

class IgateManager {
public:
    IgateManager();

    void begin();
    void update();

    // Gestión de conexión WiFi
    void connectWifi();
    void disconnectWifi();
    bool isWifiConnected() const;
    String getLocalIpString() const;

    // Gestión de conexión APRS-IS
    bool isAprsIsConnected() const { return _state == IGATE_CONNECTED || _state == IGATE_VERIFIED; }
    bool isAprsIsVerified() const { return _state == IGATE_VERIFIED; }
    IgateState getState() const { return _state; }
    const char* getStateText() const;

    // Inyección de paquetes a APRS-IS
    bool injectRfPacket(const DecodedPacket& pkt);
    bool sendSelfBeaconToAprsIs(const char* aprsInfoPayload);
    bool sendRawLineToAprsIs(const char* tnc2Line);

    // Estadísticas
    uint32_t getRfToIsCount() const { return _rfToIsCount; }
    uint32_t getIsToRfCount() const { return _isToRfCount; }
    uint32_t getSelfBeaconCount() const { return _selfBeaconCount; }
    const char* getLastInjectedCall() const { return _lastInjectedCall; }
    uint32_t getLastInjectedAgeSec() const;

    // Cálculo oficial de Passcode APRS-IS a partir del indicativo
    static uint16_t calcPasscode(const char* callsign);
    uint16_t getEffectivePasscode() const;

    // Fuerza reconexión tras cambiar parámetros
    void reconnectNow();

private:
    WiFiClient _client;
    IgateState _state = IGATE_DISABLED;

    uint32_t _lastWifiAttemptMs = 0;
    uint32_t _lastTcpAttemptMs = 0;
    uint32_t _lastServerActivityMs = 0;
    uint32_t _lastKeepAliveTxMs = 0;

    uint32_t _rfToIsCount = 0;
    uint32_t _isToRfCount = 0;
    uint32_t _selfBeaconCount = 0;
    char _lastInjectedCall[16] = "-";
    uint32_t _lastInjectedMs = 0;

    char _rxLineBuf[280];
    uint16_t _rxLineLen = 0;

    bool shouldGateRfPacket(const DecodedPacket& pkt) const;
    void processServerLine(const char* line);
};

extern IgateManager Igate;

