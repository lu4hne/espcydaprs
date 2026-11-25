#include "afsk_modem.h"
#include <driver/adc.h>
#include <esp32-hal-sigmadelta.h>
#include <esp_timer.h>
#include <math.h>

AfskModem Modem;

static constexpr uint8_t SIGMADELTA_TX_CH = 0;
static uint8_t s_sineLut[256];
static int16_t s_markCos[AFSK_SAMPLES_PER_BIT];
static int16_t s_markSin[AFSK_SAMPLES_PER_BIT];
static int16_t s_spaceCos[AFSK_SAMPLES_PER_BIT];
static int16_t s_spaceSin[AFSK_SAMPLES_PER_BIT];

static TaskHandle_t s_rxTaskHandle = nullptr;

struct TxJob {
    uint8_t  data[MAX_AX25_FRAME_LEN];
    uint16_t len;
    bool     loopback;
};

static void afskRxTaskTrampoline(void* arg) {
    static_cast<AfskModem*>(arg)->rxTaskLoop();
}

static void afskTxTaskTrampoline(void* arg) {
    static_cast<AfskModem*>(arg)->txTaskLoop();
}

static void IRAM_ATTR afskRxHwTimerIsr() {
    BaseType_t hpWoken = pdFALSE;
    if (s_rxTaskHandle) {
        vTaskNotifyGiveFromISR(s_rxTaskHandle, &hpWoken);
        if (hpWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

AfskModem::AfskModem() {
    memset(_sampleBuf, 0, sizeof(_sampleBuf));
}

void AfskModem::begin() {
    for (int i = 0; i < 256; i++) {
        float rad = (2.0f * M_PI * i) / 256.0f;
        int val = (int)roundf(127.5f + 120.0f * sinf(rad));
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        s_sineLut[i] = (uint8_t)val;
    }

    for (int k = 0; k < AFSK_SAMPLES_PER_BIT; k++) {
        float t = (float)k / (float)AFSK_SAMPLE_RATE;
        s_markCos[k]  = (int16_t)roundf(256.0f * cosf(2.0f * M_PI * AFSK_MARK_FREQ * t));
        s_markSin[k]  = (int16_t)roundf(256.0f * sinf(2.0f * M_PI * AFSK_MARK_FREQ * t));
        s_spaceCos[k] = (int16_t)roundf(256.0f * cosf(2.0f * M_PI * AFSK_SPACE_FREQ * t));
        s_spaceSin[k] = (int16_t)roundf(256.0f * sinf(2.0f * M_PI * AFSK_SPACE_FREQ * t));
    }

    pinMode(MT_PTT_PIN, OUTPUT);
    digitalWrite(MT_PTT_PIN, LOW);

    pinMode(MT_AFSK_TX_PIN, OUTPUT);
    digitalWrite(MT_AFSK_TX_PIN, HIGH); // Reposo DC bloqueado por capacitor 1uF

    // ESP32-C3: ADC1_CHANNEL_0 corresponde a GPIO 0 (12 bits, 0..3.3V)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_12);

    _rxQueue = xQueueCreate(12, sizeof(RawAx25Frame));
    _txQueue = xQueueCreate(6, sizeof(TxJob));

    // Tarea dedicada AFSK_RX con prioridad 10 (máxima prioridad de usuario)
    xTaskCreate(
        afskRxTaskTrampoline,
        "AFSK_RX",
        3072,
        this,
        10,
        &s_rxTaskHandle
    );

    // Timer de Hardware 0 a 1 MHz (alarma cada 104 us = 9600 Hz)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    _hwRxTimer = timerBegin(1000000);
    if (_hwRxTimer) {
        timerAttachInterrupt(_hwRxTimer, &afskRxHwTimerIsr);
        timerAlarm(_hwRxTimer, 1000000ULL / AFSK_SAMPLE_RATE, true, 0);
    }
#else
    _hwRxTimer = timerBegin(0, 80, true);
    if (_hwRxTimer) {
        timerAttachInterrupt(_hwRxTimer, &afskRxHwTimerIsr, true);
        timerAlarmWrite(_hwRxTimer, 1000000ULL / AFSK_SAMPLE_RATE, true);
        timerAlarmEnable(_hwRxTimer);
    }
#endif

    // Tarea de transmisión AX.25 (prioridad 5, duerme con 0% CPU en reposo)
    xTaskCreate(
        afskTxTaskTrampoline,
        "AFSK_TX",
        4096,
        this,
        5,
        nullptr
    );
}

void AfskModem::configure(uint16_t txDelayMs, uint16_t txTailMs, uint8_t txVolumePercent) {
    uint16_t delayFlags = (txDelayMs * 150) / 1000;
    if (delayFlags < 15) delayFlags = 15;
    if (delayFlags > 180) delayFlags = 180;
    _txDelayFlags = delayFlags;

    uint16_t tailFlags = (txTailMs * 150) / 1000;
    if (tailFlags < 4) tailFlags = 4;
    if (tailFlags > 40) tailFlags = 40;
    _txTailFlags = tailFlags;

    if (txVolumePercent < 10) txVolumePercent = 10;
    if (txVolumePercent > 100) txVolumePercent = 100;
    _txVolume = txVolumePercent;
}

uint16_t AfskModem::calcCrc16(const uint8_t* data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0x8408;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

bool AfskModem::transmitFrame(const uint8_t* payload, uint16_t len, bool loopbackAlso) {
    if (!_txQueue || len == 0 || len + 2 > MAX_AX25_FRAME_LEN) {
        return false;
    }

    TxJob job;
    memcpy(job.data, payload, len);
    uint16_t crc = calcCrc16(payload, len);
    job.data[len]     = crc & 0xFF;
    job.data[len + 1] = (crc >> 8) & 0xFF;
    job.len = len + 2;
    job.loopback = loopbackAlso;

    return xQueueSend(_txQueue, &job, pdMS_TO_TICKS(50)) == pdTRUE;
}

bool AfskModem::receiveFrame(RawAx25Frame* outFrame) {
    if (!_rxQueue) return false;
    return xQueueReceive(_rxQueue, outFrame, 0) == pdTRUE;
}

void AfskModem::setPtt(bool active) {
    _transmitting = active;
    digitalWrite(MT_PTT_PIN, active ? HIGH : LOW);

    if (active) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
        sigmaDeltaAttach(MT_AFSK_TX_PIN, 312500);
        sigmaDeltaWrite(MT_AFSK_TX_PIN, 128);
#else
        sigmaDeltaSetup(MT_AFSK_TX_PIN, SIGMADELTA_TX_CH, 312500);
        sigmaDeltaWrite(SIGMADELTA_TX_CH, 128);
#endif
    } else {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
        sigmaDeltaDetach(MT_AFSK_TX_PIN);
#else
        sigmaDeltaDetachPin(MT_AFSK_TX_PIN);
#endif
        pinMode(MT_AFSK_TX_PIN, OUTPUT);
        digitalWrite(MT_AFSK_TX_PIN, HIGH);
    }
}

void AfskModem::rxTaskLoop() {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        onRxSample();
    }
}

void AfskModem::onRxSample() {
    if (_transmitting) {
        return;
    }

    int rawAdc = adc1_get_raw(ADC1_CHANNEL_0);
    int32_t dc = _dcAcc >> 8;
    _dcAcc += (rawAdc - dc);
    int16_t centered = (int16_t)(rawAdc - dc);

    int16_t absSample = (centered < 0) ? -centered : centered;
    if (absSample > _peakInWindow) {
        _peakInWindow = absSample;
    }
    _levelWindowSamples++;
    if (_levelWindowSamples >= 960) {
        int pct = (_peakInWindow * 100) / 1400;
        if (pct > 100) pct = 100;
        _rxLevelPercent = (uint8_t)pct;
        _carrierDetected = (_peakInWindow > 70) || _rxInFrame;
        _rxMarkPct = (uint8_t)((_markInWindow * 100UL) / 960UL);
        _peakInWindow = 0;
        _markInWindow = 0;
        _levelWindowSamples = 0;
    }

    if (absSample > _rxPeakTrack) {
        _rxPeakTrack = absSample;
    }
    processRxSample(centered);
}

void AfskModem::txTaskLoop() {
    const int64_t samplePeriodUs = 1000000LL / AFSK_SAMPLE_RATE;
    const uint16_t phaseIncMark  = (uint16_t)((AFSK_MARK_FREQ * 65536UL) / AFSK_SAMPLE_RATE);
    const uint16_t phaseIncSpace = (uint16_t)((AFSK_SPACE_FREQ * 65536UL) / AFSK_SAMPLE_RATE);

    TxJob currentJob;

    while (true) {
        if (xQueueReceive(_txQueue, &currentJob, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }

        setPtt(true);

        if (currentJob.loopback) {
            _rxInFrame = false;
            _rxFrameLen = 0;
            _rxBitIndex = 0;
            _rxOnesCount = 0;
            _lpfCorrInt = 0;
        }

        uint16_t phaseAcc = 0;
        bool currentToneMark = true;
        uint8_t txOnesCount = 0;
        int64_t nextSampleTime = esp_timer_get_time();

        auto emitPhysicalBit = [&](bool bitVal) {
            if (!bitVal) {
                currentToneMark = !currentToneMark; // NRZI: '0' cambia de tono
            }
            uint16_t inc = currentToneMark ? phaseIncMark : phaseIncSpace;

            for (int s = 0; s < AFSK_SAMPLES_PER_BIT; s++) {
                phaseAcc += inc;
                uint8_t rawSine = s_sineLut[phaseAcc >> 8];
                int16_t centered = (int16_t)rawSine - 128;
                int16_t scaled = (centered * (int16_t)_txVolume) / 100;
                uint8_t outByte = (uint8_t)constrain(128 + scaled, 0, 255);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
                sigmaDeltaWrite(MT_AFSK_TX_PIN, outByte);
#else
                sigmaDeltaWrite(SIGMADELTA_TX_CH, outByte);
#endif

                if (currentJob.loopback) {
                    processRxSample(scaled * 8);
                }

                nextSampleTime += samplePeriodUs;
                while (true) {
                    int64_t now = esp_timer_get_time();
                    if (now >= nextSampleTime) break;
                }
            }
        };

        auto sendBit = [&](bool bitVal, bool stuffEnabled) {
            emitPhysicalBit(bitVal);
            if (stuffEnabled) {
                if (bitVal) {
                    txOnesCount++;
                    if (txOnesCount == 5) {
                        emitPhysicalBit(false);
                        txOnesCount = 0;
                    }
                } else {
                    txOnesCount = 0;
                }
            }
        };

        auto sendByte = [&](uint8_t byteVal, bool isFlag) {
            for (uint8_t b = 0; b < 8; b++) {
                sendBit((byteVal >> b) & 0x01, !isFlag);
            }
        };

        for (uint16_t i = 0; i < _txDelayFlags; i++) sendByte(0x7E, true);
        txOnesCount = 0;
        for (uint16_t i = 0; i < currentJob.len; i++) sendByte(currentJob.data[i], false);
        for (uint16_t i = 0; i < _txTailFlags; i++) sendByte(0x7E, true);

        setPtt(false);
        _txCount++;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void AfskModem::processRxSample(int16_t centeredSample) {
    static_assert(AFSK_SAMPLES_PER_BIT == 8, "AFSK_SAMPLES_PER_BIT debe ser 8");
    _sampleBuf[_sampleIdx] = centeredSample;
    _sampleIdx = (_sampleIdx + 1) & 7;

    int32_t iMark = 0, qMark = 0;
    int32_t iSpace = 0, qSpace = 0;

    for (uint8_t k = 0; k < 8; k++) {
        int32_t s = _sampleBuf[(_sampleIdx + k) & 7];
        iMark  += (s * s_markCos[k]) >> 8;
        qMark  += (s * s_markSin[k]) >> 8;
        iSpace += (s * s_spaceCos[k]) >> 8;
        qSpace += (s * s_spaceSin[k]) >> 8;
    }

    int32_t energyMark  = (iMark * iMark + qMark * qMark) >> 4;
    int32_t energySpace = (iSpace * iSpace + qSpace * qSpace) >> 4;
    int32_t diff = energyMark - energySpace;

    _lpfCorrInt = (_lpfCorrInt * 11 + diff * 5) >> 4;
    bool currentToneMark = (_lpfCorrInt >= 0);
    if (currentToneMark) {
        _markInWindow++;
    }

    if (currentToneMark != _lastToneMark) {
        _lastToneMark = currentToneMark;
        if (_pllCounter < 4) _pllCounter += 1;
        else                 _pllCounter -= 1;
    }

    _pllCounter++;
    if (_pllCounter >= 8) {
        _pllCounter -= 8;
        bool nrziBit = (currentToneMark == _nrziLastTone);
        _nrziLastTone = currentToneMark;
        processRxBit(nrziBit);
    }
}

void AfskModem::processRxBit(bool nrziBit) {
    if (nrziBit) {
        _rxOnesCount++;
        if (_rxOnesCount >= 7) {
            _rxInFrame = false;
            _rxFrameLen = 0;
            _rxBitIndex = 0;
            return;
        }
        if (_rxInFrame) {
            _rxCurrentByte = (_rxCurrentByte >> 1) | 0x80;
            _rxBitIndex++;
            if (_rxBitIndex == 8) {
                if (_rxFrameLen < MAX_AX25_FRAME_LEN) _rxFrameBuf[_rxFrameLen++] = _rxCurrentByte;
                else _rxInFrame = false;
                _rxCurrentByte = 0;
                _rxBitIndex = 0;
            }
        }
    } else {
        if (_rxOnesCount == 6) {
            if (_rxInFrame && _rxFrameLen >= 16) {
                uint16_t payloadLen = _rxFrameLen - 2;
                uint16_t expectedCrc = calcCrc16(_rxFrameBuf, payloadLen);
                uint16_t rxCrc = (uint16_t)_rxFrameBuf[payloadLen] |
                                 ((uint16_t)_rxFrameBuf[payloadLen + 1] << 8);
                if (expectedCrc == rxCrc) {
                    RawAx25Frame validFrame;
                    memcpy(validFrame.data, _rxFrameBuf, payloadLen);
                    validFrame.len = payloadLen;
                    validFrame.audioPeak = _rxPeakTrack;
                    xQueueSend(_rxQueue, &validFrame, 0);
                    _rxCount++;
                }
            }
            _rxInFrame = true;
            _rxFrameLen = 0;
            _rxBitIndex = 0;
            _rxCurrentByte = 0;
            _rxPeakTrack = 0;
            _rxOnesCount = 0;
            return;
        } else if (_rxOnesCount == 5) {
            _rxOnesCount = 0;
            return;
        }

        _rxOnesCount = 0;
        if (_rxInFrame) {
            _rxCurrentByte = (_rxCurrentByte >> 1);
            _rxBitIndex++;
            if (_rxBitIndex == 8) {
                if (_rxFrameLen < MAX_AX25_FRAME_LEN) _rxFrameBuf[_rxFrameLen++] = _rxCurrentByte;
                else _rxInFrame = false;
                _rxCurrentByte = 0;
                _rxBitIndex = 0;
            }
        }
    }
}
