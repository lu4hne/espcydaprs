#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_timer.h>
#include "config.h"

#define MAX_AX25_FRAME_LEN  280

struct RawAx25Frame {
    uint8_t data[MAX_AX25_FRAME_LEN];
    uint16_t len;
    int16_t audioPeak;
};

enum RxDiagError : uint8_t {
    RX_ERR_NONE = 0,
    RX_ERR_CLIPPING,    // ADC saturado (<=30 o >=4065)
    RX_ERR_CRC_FAIL,    // Trama >=16B recibida pero fallo CRC-16
    RX_ERR_ABORT,       // Trama abortada por 7 unos (perdida de sincronismo)
    RX_ERR_SHORT,       // Trama truncada (<16B entre flags)
    RX_ERR_NO_FLAGS     // Audio presente (>20%) sin detectar banderas 0x7E
};

class AfskModem {
public:
    AfskModem();

    void begin();
    void configure(uint16_t txDelayMs, uint16_t txTailMs, uint8_t txVolumePercent, uint8_t txAudioPin = AFSK_DEFAULT_TX_PIN);

    // Pausa temporalmente el muestreo ADC RX durante la calibración RF PHY de WiFi.begin()
    void setPaused(bool paused) { _paused = paused; }
    bool isPaused() const { return _paused; }

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

    // Diagnóstico de decodificación RX en tiempo real
    uint32_t getRxFlagCount() const { return _rxFlagCount; }
    uint32_t getRxCrcFailCount() const { return _rxCrcFailCount; }
    uint32_t getRxAbortCount() const { return _rxAbortCount; }
    uint32_t getRxShortCount() const { return _rxShortCount; }
    uint16_t getRxDcRaw() const { return (uint16_t)(_dcAcc >> 8); }
    uint8_t  getRxMarkPercent() const { return _rxMarkPct; }
    int16_t  getRxLastPeak() const { return _lastWindowPeak; }
    uint16_t getRxLastFailLen() const { return _lastFailLen; }
    RxDiagError getLastRxError() const { return _lastErr; }
    uint32_t getLastRxErrorMs() const { return _lastErrMs; }
    uint32_t getLastRxOkMs() const { return _lastRxOkMs; }
    void resetRxDiag();

    // Calcula el CRC-16 CCITT (AX.25 estándar: init 0xFFFF, poly 0x8408 reflejado)
    static uint16_t calcCrc16(const uint8_t* data, uint16_t len);

    // Loop de tarea TX, tarea RX (Core 1, aislada del WiFi en Core 0) y muestreo 9600 Hz
    void dspTaskLoop();
    void rxTaskLoop();
    void onRxTimerSample();

private:
    QueueHandle_t _rxQueue;
    QueueHandle_t _txQueue;
    hw_timer_t* _hwRxTimer = nullptr;

    volatile bool _transmitting = false;
    volatile bool _paused = false;
    volatile bool _carrierDetected = false;
    volatile uint8_t _rxLevelPercent = 0;
    volatile uint32_t _txCount = 0;
    volatile uint32_t _rxCount = 0;

    uint32_t _levelWindowSamples = 0;
    int32_t _peakInWindow = 0;
    uint16_t _clipInWindow = 0;
    uint16_t _markInWindow = 0;
    uint8_t  _carrierNoFlagTicks = 0;
    volatile int16_t  _lastWindowPeak = 0;
    volatile uint8_t  _rxMarkPct = 50;
    volatile uint32_t _rxFlagCount = 0;
    volatile uint32_t _rxCrcFailCount = 0;
    volatile uint32_t _rxAbortCount = 0;
    volatile uint32_t _rxShortCount = 0;
    volatile uint16_t _lastFailLen = 0;
    volatile RxDiagError _lastErr = RX_ERR_NONE;
    volatile uint32_t _lastErrMs = 0;
    volatile uint32_t _lastRxOkMs = 0;
    uint32_t _flagsAtLastWindow = 0;

    uint16_t _txDelayFlags = 45; // Cantidad de bytes 0x7E de preámbulo
    uint16_t _txTailFlags = 6;   // Cantidad de bytes 0x7E de cola
    uint8_t _txVolume = 80;      // 10..100%
    uint8_t _txAudioPin = AFSK_DEFAULT_TX_PIN; // Pin en conector P3/CN1 (27, 21, 4, 17 o 26)

    // Estado del Demodulador AFSK (RX) — 100% punto fijo entero para máxima velocidad en Core 0
    int16_t _sampleBuf[AFSK_SAMPLES_PER_BIT];
    uint8_t _sampleIdx = 0;
    int32_t _dcAcc = 2048L << 8;
    int32_t _lpfCorrInt = 0;
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
