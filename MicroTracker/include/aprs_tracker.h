#pragma once

#include <Arduino.h>
#include "config.h"
#include "afsk_modem.h"

struct GpsStatus {
    bool     validFix;
    double   lat;
    double   lon;
    int16_t  altMeters;
    float    speedKmh;
    uint16_t courseDeg;
    uint8_t  satellites;
    uint32_t charsProcessed;
};

// Callback para notificar eventos/mensajes recibidos a la consola BLE y Serial
typedef void (*TrackerEventCallback)(const char* eventLine);

class AprsTracker {
public:
    AprsTracker();

    void begin();
    void update();

    void loadConfig();
    void saveConfig();
    void applyRadioPreset(RadioPreset preset);

    TrackerConfig& config() { return _cfg; }
    const GpsStatus& gps() const { return _gps; }

    double   getCurrentLat() const;
    double   getCurrentLon() const;
    int16_t  getCurrentAlt() const;
    bool     isSosActive() const { return _sosActive; }

    void setEventCallback(TrackerEventCallback cb) { _eventCb = cb; }

    // Transmisiones APRS
    bool sendPositionBeacon(bool loopback = false);
    bool sendTextMessage(const char* destCall, const char* text, bool loopback = false);
    void activateSos();
    void cancelSos();

    void formatMyFullCall(char* out, size_t outSize) const;

private:
    TrackerConfig _cfg;
    GpsStatus     _gps;
    TrackerEventCallback _eventCb = nullptr;

    uint32_t _lastBeaconMs = 0;
    uint16_t _lastBeaconCourseDeg = 0;
    bool     _hasLastBeaconCourse = false;

    bool     _sosActive = false;
    uint32_t _lastSosBeaconMs = 0;

    // Reintento de mensaje saliente pendiente de ACK
    bool     _pendingMsgActive = false;
    char     _pendingDest[12];
    char     _pendingText[68];
    char     _pendingId[6];
    uint8_t  _pendingRetries = 0;
    uint32_t _pendingNextTryMs = 0;
    uint16_t _msgSeqCounter = 1;

    void pollGps();
    void evaluateBeaconTimer();
    void processIncomingFrame(const RawAx25Frame& raw);
    bool buildAndSendUiFrame(const char* infoPayload, bool loopback);
    void encodeAx25Address(uint8_t* out7, const char* call, uint8_t ssid, bool isLast);
    void formatAprsLatLon(double lat, double lon, char* latOut, char* lonOut) const;
    void emitEvent(const char* fmt, ...);
};

extern AprsTracker Tracker;
