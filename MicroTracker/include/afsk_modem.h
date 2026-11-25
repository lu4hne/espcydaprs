#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "config.h"

#define MAX_AX25_FRAME_LEN 280

struct RawAx25Frame {
    uint8_t  data[MAX_AX25_FRAME_LEN];
    uint16_t len;
    int16_t  audioPeak;
};

class AfskModem {
public:
    AfskModem();

    void begin();
    void configure(uint16_t txDelayMs, uint16_t txTailMs, uint8_t txVolumePercent);

    // Encola una trama AX.25 (sin CRC ni banderas 0x7E)
    bool transmitFrame(const uint8_t* payload, uint16_t len, bool loopbackAlso = false);

    // Extrae una trama AX.25 recibida y validada por CRC-16
    bool receiveFrame(RawAx25Frame* outFrame);

    bool     isTransmitting() const { return _transmitting; }
    bool     isCarrierDetected() const { return _carrierDetected; }
    uint8_t  getRxAudioLevel() const { return _rxLevelPercent; }
    uint16_t getRxDcRaw() const { return (uint16_t)(_dcAcc >> 8); }
    uint8_t  getRxMarkPercent() const { return _rxMarkPct; }
    uint32_t getTxPacketCount() const { return _txCount; }
    uint32_t getRxPacketCount() const { return _rxCount; }

    static uint16_t calcCrc16(const uint8_t* data, uint16_t len);

    void rxTaskLoop();
    void txTaskLoop();
    void onRxSample();

private:
    QueueHandle_t _rxQueue = nullptr;
    QueueHandle_t _txQueue = nullptr;
    hw_timer_t*   _hwRxTimer = nullptr;

    volatile bool     _transmitting = false;
    volatile bool     _carrierDetected = false;
    volatile uint8_t  _rxLevelPercent = 0;
    volatile uint8_t  _rxMarkPct = 50;
    volatile uint32_t _txCount = 0;
    volatile uint32_t _rxCount = 0;

    uint32_t _levelWindowSamples = 0;
    int32_t  _peakInWindow = 0;
    uint16_t _markInWindow = 0;

    uint16_t _txDelayFlags = 68;
    uint16_t _txTailFlags  = 9;
    uint8_t  _txVolume     = 75;

    // Demodulador AFSK 100% punto fijo entero
    int16_t _sampleBuf[AFSK_SAMPLES_PER_BIT];
    uint8_t _sampleIdx = 0;
    int32_t _dcAcc = 2048L << 8;
    int32_t _lpfCorrInt = 0;
    bool    _lastToneMark = true;
    int16_t _pllCounter = 0;
    bool    _nrziLastTone = true;

    // Estado HDLC RX
    uint8_t  _rxOnesCount = 0;
    bool     _rxInFrame = false;
    uint8_t  _rxCurrentByte = 0;
    uint8_t  _rxBitIndex = 0;
    uint8_t  _rxFrameBuf[MAX_AX25_FRAME_LEN];
    uint16_t _rxFrameLen = 0;
    int16_t  _rxPeakTrack = 0;

    void processRxSample(int16_t centeredSample);
    void processRxBit(bool nrziBit);
    void setPtt(bool active);
};

extern AfskModem Modem;
