#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "config.h"

#define MAX_AX25_FRAME_LEN  280

struct RawAx25Frame {
    uint8_t data[MAX_AX25_FRAME_LEN];
    uint16_t len;
    int16_t audioPeak;
};

class AfskModem {
public:
    AfskModem();

    void begin();
    void configure(uint16_t txDelayMs, uint16_t txTailMs, uint8_t txVolumePercent, uint8_t txAudioPin = AFSK_DEFAULT_TX_PIN);

    // Encola una trama AX.25 (sin CRC ni banderas 0x7E; el módem calcula CRC-16 y agrega 0x7E)
    bool transmitFrame(const uint8_t* payload, uint16_t len, bool loopbackAlso = false);

    // Extrae una trama AX.25 recibida y validada por CRC-16 (sin los 2 bytes de CRC finales)
    bool receiveFrame(RawAx25Frame* outFrame);

    // Estado en tiempo real para la UI
    bool isTransmitting() const { return _transmitting; }
    bool isCarrierDetected() const { return _carrierDetected; }
    uint8_t getRxAudioLevel() const { return _rxLevelPercent; }
    uint32_t getTxPacketCount() const { return _txCount; }
    uint32_t getRxPacketCount() const { return _rxCount; }
    uint8_t getTxAudioPin() const { return _txAudioPin; }

    // Calcula el CRC-16 CCITT (AX.25 estándar: init 0xFFFF, poly 0x8408 reflejado)
    static uint16_t calcCrc16(const uint8_t* data, uint16_t len);

    // Loop interno DSP ejecutado en Core 0
    void dspTaskLoop();

private:
    QueueHandle_t _rxQueue;
    QueueHandle_t _txQueue;

    volatile bool _transmitting = false;
    volatile bool _carrierDetected = false;
    volatile uint8_t _rxLevelPercent = 0;
    volatile uint32_t _txCount = 0;
    volatile uint32_t _rxCount = 0;

    uint16_t _txDelayFlags = 45; // Cantidad de bytes 0x7E de preámbulo
    uint16_t _txTailFlags = 6;   // Cantidad de bytes 0x7E de cola
    uint8_t _txVolume = 80;      // 10..100%
    uint8_t _txAudioPin = AFSK_DEFAULT_TX_PIN; // Pin en conector P3/CN1 (27, 21, 4, 17 o 26)

    // Estado del Demodulador AFSK (RX)
    int16_t _sampleBuf[AFSK_SAMPLES_PER_BIT];
    uint8_t _sampleIdx = 0;
    float _dcEstimate = 2048.0f;
    float _lpfCorr = 0.0f;
    bool _lastToneMark = true;
    int16_t _pllCounter = 0;
    bool _nrziLastTone = true;

    // Estado HDLC RX
    uint8_t _rxOnesCount = 0;
    bool _rxInFrame = false;
    uint8_t _rxCurrentByte = 0;
    uint8_t _rxBitIndex = 0;
    uint8_t _rxFrameBuf[MAX_AX25_FRAME_LEN];
    uint16_t _rxFrameLen = 0;
    int16_t _rxPeakTrack = 0;

    void processRxSample(int16_t centeredSample);
    void processRxBit(bool nrziBit);
    void setPtt(bool active);
};

extern AfskModem Modem;
