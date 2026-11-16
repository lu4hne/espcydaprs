#pragma once

#include <Arduino.h>
#include "config.h"
#include "afsk_modem.h"

enum AprsPacketType : uint8_t {
    APRS_PKT_POSITION = 0,
    APRS_PKT_MESSAGE  = 1,
    APRS_PKT_ACK      = 2,
    APRS_PKT_STATUS   = 3,
    APRS_PKT_TELEMETRY= 4,
    APRS_PKT_OTHER    = 5
};

struct DecodedPacket {
    uint32_t timestampMs;
    char srcCall[12];       // Ej: "LU4HNE-7"
    char dstCall[12];       // Ej: "APECYD"
    char pathStr[42];       // Ej: "WIDE1-1*,WIDE2-1"
    char infoText[140];     // Payload APRS crudo
    AprsPacketType type;
    bool hasPosition;
    bool gatedToIs;         // true si este paquete fue inyectado exitosamente en APRS-IS
    bool fromAprsIs;        // true si provino de APRS-IS en vez de RF
    double lat;
    double lon;
    float distanceKm;
    int16_t bearingDeg;
    int16_t altitudeM;
    char symbolTable;
    char symbolCode;
    char comment[72];
    int16_t audioPeak;
};

struct StationEntry {
    bool active;
    char callsign[12];
    double lat;
    double lon;
    bool hasPos;
    float distanceKm;
    int16_t bearingDeg;
    char symbolTable;
    char symbolCode;
    char lastComment[48];
    uint32_t lastHeardMs;
    uint16_t pktCount;
};

enum MsgState : uint8_t {
    MSG_STATE_RX      = 0,  // Recibido
    MSG_STATE_TX_WAIT = 1,  // Enviado, esperando ACK
    MSG_STATE_TX_ACK  = 2,  // Confirmado con ACK
    MSG_STATE_TX_FAIL = 3   // Agotó reintentos sin ACK
};

struct AprsMessageEntry {
    bool active;
    bool incoming;          // true = recibido, false = enviado por nosotros
    bool sentViaIs;         // true = inyectado en servidor APRS-IS por IGate
    char peerCall[12];      // Indicativo remoto (ej. "LU1XYZ-9")
    char text[68];          // Texto del mensaje
    char msgId[8];          // ID de secuencia (ej. "12")
    MsgState state;
    uint8_t retriesLeft;
    uint32_t lastTxMs;
    uint32_t timestampMs;
};

struct GpsState {
    bool validFix;
    double lat;
    double lon;
    int16_t altMeters;
    uint16_t speedKnots;
    uint16_t courseDeg;
    uint8_t satellites;
    uint32_t charsProcessed;
};

class AprsEngine {
public:
    AprsEngine();

    void begin();
    void update();

    // Gestión de Configuración NVS
    AprsConfig& config() { return _cfg; }
    void loadConfig();
    void saveConfig();
    void applyRadioPreset(RadioType type);
    void applyStationMode(StationMode mode);
    void applyBoardModel(BoardModel model);

    // Acciones APRS
    bool sendPositionBeacon(bool loopbackTest = false);
    bool sendTextMessage(const char* destCallsign, const char* messageText, bool loopbackTest = false);
    bool sendCustomStatus(const char* statusText, bool loopbackTest = false);

    // Control de Emergencia / SOS (Exclusivo para MODO TRACKER)
    bool isEmergencyActive() const { return _emergState.active; }
    const EmergencyState& getEmergencyState() const { return _emergState; }
    void triggerEmergency(uint8_t emergType);
    void cancelEmergency();
    bool sendEmergencyBeacon(bool loopbackTest = false);
    bool sendEmergencyCancelBeacon(bool loopbackTest = false);
    static const char* getEmergencyTypeStr(uint8_t type);

    // Recepción de línea TNC2 desde APRS-IS (para mensajes entrantes o IGate IS->RF)
    bool handleAprsIsIncomingLine(const char* tnc2Line);

    // Posición activa actual (Fija o GPS según configuración)
    double getCurrentLat() const;
    double getCurrentLon() const;
    int16_t getCurrentAlt() const;
    const GpsState& getGpsState() const { return _gpsState; }

    // Acceso a listas para la UI
    int getPacketLogCount() const { return _packetCount; }
    const DecodedPacket& getPacketLog(int index) const; // 0 = más reciente

    int getStationCount() const;
    const StationEntry* getStations() const { return _stations; }

    int getMessageCount() const { return _msgCount; }
    const AprsMessageEntry& getMessage(int index) const; // 0 = más reciente
    uint16_t getUnreadMessageCount() const { return _unreadMsgCount; }
    void clearUnreadMessages() { _unreadMsgCount = 0; }
    uint32_t getUiRevision() const { return _uiRevision; }

    uint32_t getSecondsUntilNextBeacon() const;

    // Utilidades geográficas
    static float calcDistanceKm(double lat1, double lon1, double lat2, double lon2);
    static int16_t calcBearingDeg(double lat1, double lon1, double lat2, double lon2);
    static const char* bearingToCardinal(int16_t deg);

private:
    AprsConfig _cfg;
    GpsState _gpsState;
    EmergencyState _emergState;
    uint32_t _lastEmergSoundMs = 0;

    DecodedPacket _packetLog[MAX_PACKET_LOG];
    int _packetHead = 0;
    int _packetCount = 0;

    StationEntry _stations[MAX_STATIONS_LIST];
    AprsMessageEntry _messages[MAX_MESSAGES_LIST];
    int _msgCount = 0;
    uint16_t _unreadMsgCount = 0;
    uint16_t _nextMsgSeq = 1;
    uint32_t _uiRevision = 0;

    uint32_t _lastBeaconMs = 0;

    uint16_t buildAx25Frame(uint8_t* outBuf,
                            const char* srcCall, uint8_t srcSsid,
                            const char* dstCall, uint8_t dstSsid,
                            const char* path1, uint8_t path1Ssid,
                            const char* path2, uint8_t path2Ssid,
                            const char* infoField);

    bool decodeAx25Frame(const RawAx25Frame& raw, DecodedPacket* outPkt);
    void maybeDigipeat(const RawAx25Frame& raw, const DecodedPacket& decoded);
    void parseAprsInfo(DecodedPacket* pkt);
    void updateStationTable(const DecodedPacket& pkt);
    void handleIncomingMessage(const DecodedPacket& pkt, const char* addressee, const char* body);
    void sendAckPacket(const char* toCallsign, const char* msgId);
    void processMessageRetries();
    void pollGps();
    bool wasStationHeardRecentlyOnRf(const char* callsign, uint32_t maxAgeMs = 1800000UL) const;

    static void parseCallsignSsid(const char* fullStr, char* outCall, uint8_t* outSsid);
    static void formatFullCallsign(const char* call, uint8_t ssid, char* outStr, size_t maxLen);
};

extern AprsEngine Aprs;
