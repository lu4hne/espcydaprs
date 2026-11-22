#include "afsk_modem.h"
#include <driver/adc.h>
#include <driver/dac.h>
#include <esp32-hal-sigmadelta.h>
#include <esp_timer.h>
#include <math.h>

AfskModem Modem;

static constexpr uint8_t SIGMADELTA_TX_CH = 0;

// Tabla Senoidal de 256 entradas (0..255, centrada en 128)
static uint8_t s_sineLut[256];

// Coeficientes del correlador en cuadratura para 8 muestras a 9600 Hz
// Mark = 1200 Hz (1 ciclo completo en 8 muestras)
// Space = 2200 Hz (1.833 ciclos en 8 muestras)
static int16_t s_markCos[AFSK_SAMPLES_PER_BIT];
static int16_t s_markSin[AFSK_SAMPLES_PER_BIT];
static int16_t s_spaceCos[AFSK_SAMPLES_PER_BIT];
static int16_t s_spaceSin[AFSK_SAMPLES_PER_BIT];

struct TxJob {
    uint8_t data[MAX_AX25_FRAME_LEN];
    uint16_t len;
    bool loopback;
};

static void afskDspTaskTrampoline(void* arg) {
    AfskModem* self = static_cast<AfskModem*>(arg);
    self->dspTaskLoop();
}

static void IRAM_ATTR afskRxTimerCallback(void* arg) {
    AfskModem* self = static_cast<AfskModem*>(arg);
    self->onRxTimerSample();
}

AfskModem::AfskModem()
    : _rxQueue(nullptr), _txQueue(nullptr) {
    memset(_sampleBuf, 0, sizeof(_sampleBuf));
}

void AfskModem::begin() {
    // Inicializar tablas trigonométricas para DDS y Correlador
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

    // Configurar pin PTT y LEDs de estado
    pinMode(RADIO_PTT_PIN, OUTPUT);
    digitalWrite(RADIO_PTT_PIN, LOW);

    if (_txAudioPin != CYD_LED_R) {
        pinMode(CYD_LED_R, OUTPUT);
        digitalWrite(CYD_LED_R, HIGH); // Apagado (Activo en LOW)
    }

    // Configurar DAC2 (GPIO 26 -> amplificador de audio onboard / conector SPK P4)
    dac_output_enable(DAC_CHANNEL_2);
    dac_output_voltage(DAC_CHANNEL_2, 128);

    // Configurar ADC1_CH7 (GPIO 35) para entrada RX (12 bits, 0..3.3V)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_12);

    _rxQueue = xQueueCreate(10, sizeof(RawAx25Frame));
    _txQueue = xQueueCreate(6, sizeof(TxJob));

    // 1. Iniciar timer periódico de hardware (esp_timer) a 9600 Hz (104 us) para muestreo RX
    //    Consume < 6% de CPU y deja 94% libre para la pila WiFi/TCP-IP y el Watchdog de Core 0.
    esp_timer_create_args_t timerArgs = {};
    timerArgs.callback = &afskRxTimerCallback;
    timerArgs.arg = this;
    timerArgs.dispatch_method = ESP_TIMER_TASK;
    timerArgs.name = "afsk_rx_9600";
    if (esp_timer_create(&timerArgs, &_rxTimer) == ESP_OK) {
        esp_timer_start_periodic(_rxTimer, 1000000ULL / AFSK_SAMPLE_RATE); // 104 us
    }

    // 2. Lanzar tarea de transmisión AX.25 (duerme en xQueueReceive con 0% CPU cuando no transmite)
    xTaskCreatePinnedToCore(
        afskDspTaskTrampoline,
        "AFSK_TX",
        4096,
        this,
        4,
        nullptr,
        1
    );
}

void AfskModem::configure(uint16_t txDelayMs, uint16_t txTailMs, uint8_t txVolumePercent, uint8_t txAudioPin) {
    // A 1200 bps, 1 byte (8 bits) dura 6.67 ms
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

    if (txAudioPin == 27 || txAudioPin == 21 || txAudioPin == 4 || txAudioPin == 17 || txAudioPin == 26) {
        _txAudioPin = txAudioPin;
    }
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
    job.data[len]     = crc & 0xFF;        // FCS LSB primero
    job.data[len + 1] = (crc >> 8) & 0xFF; // FCS MSB segundo
    job.len = len + 2;
    job.loopback = loopbackAlso;

    return xQueueSend(_txQueue, &job, pdMS_TO_TICKS(50)) == pdTRUE;
}

bool AfskModem::receiveFrame(RawAx25Frame* outFrame) {
    if (!_rxQueue) return false;
    return xQueueReceive(_rxQueue, outFrame, 0) == pdTRUE;
}

void AfskModem::resetRxDiag() {
    _rxFlagCount = 0;
    _rxCrcFailCount = 0;
    _rxAbortCount = 0;
    _rxShortCount = 0;
    _lastFailLen = 0;
    _lastErr = RX_ERR_NONE;
    _lastErrMs = 0;
    _flagsAtLastWindow = 0;
    _carrierNoFlagTicks = 0;
}

void AfskModem::setPtt(bool active) {
    _transmitting = active;
    digitalWrite(RADIO_PTT_PIN, active ? HIGH : LOW);

    if (_txAudioPin != CYD_LED_R) {
        pinMode(CYD_LED_R, OUTPUT);
        digitalWrite(CYD_LED_R, active ? LOW : HIGH); // LED Rojo encendido durante TX
    }

    // Si se seleccionó un pin de los conectores blancos (27, 21, 4 o 17),
    // activar el modulador Sigma-Delta de hardware (312.5 kHz) solo durante TX
    // y desacoplarlo en RX para evitar ruido digital en el receptor.
    if (_txAudioPin != AFSK_DAC2_PIN) {
        if (active) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
            sigmaDeltaAttach(_txAudioPin, 312500);
            sigmaDeltaWrite(_txAudioPin, 128);
#else
            sigmaDeltaSetup(_txAudioPin, SIGMADELTA_TX_CH, 312500);
            sigmaDeltaWrite(SIGMADELTA_TX_CH, 128);
#endif
        } else {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
            sigmaDeltaDetach(_txAudioPin);
#else
            sigmaDeltaDetachPin(_txAudioPin);
#endif
            pinMode(_txAudioPin, OUTPUT);
            // Dejar en HIGH (3.3V DC constantes bloqueados por el capacitor de 1uF):
            // mantiene apagado el LED Rojo (IO4) y nunca apaga el Backlight si el usuario eligió IO21/IO27.
            digitalWrite(_txAudioPin, HIGH);
        }
    }
}

void AfskModem::onRxTimerSample() {
    if (_transmitting || _paused) {
        return;
    }

    // Muestreo ADC a 9600 Hz en GPIO 35 (ADC1_CH7)
    int rawAdc = adc1_get_raw(ADC1_CHANNEL_7);

    // Filtro IIR 100% entero (sin float) para remover componente DC (~1.65V -> ~2048)
    int32_t dc = _dcAcc >> 8;
    _dcAcc += (rawAdc - dc);
    int16_t centered = (int16_t)(rawAdc - dc);

    // Medición rápida de nivel de audio y recorte ADC (sin llamadas a millis() en el camino crítico)
    if (rawAdc <= 30 || rawAdc >= 4065) {
        _clipInWindow++;
    }
    int16_t absSample = (centered < 0) ? -centered : centered;
    if (absSample > _peakInWindow) {
        _peakInWindow = absSample;
    }
    _levelWindowSamples++;
    if (_levelWindowSamples >= 960) { // Actualizar cada 100 ms
        int pct = (_peakInWindow * 100) / 1400;
        if (pct > 100) pct = 100;
        _rxLevelPercent = (uint8_t)pct;
        _lastWindowPeak = (int16_t)_peakInWindow;
        _carrierDetected = (_peakInWindow > 70) || _rxInFrame;
        _rxMarkPct = (uint8_t)((_markInWindow * 100UL) / 960UL);

        if (_clipInWindow >= 10) {
            _lastErr = RX_ERR_CLIPPING;
        } else if (pct >= 20) {
            if (_rxFlagCount == _flagsAtLastWindow) {
                if (_carrierNoFlagTicks < 255) _carrierNoFlagTicks++;
                if (_carrierNoFlagTicks >= 4) {
                    _lastErr = RX_ERR_NO_FLAGS;
                }
            } else {
                _carrierNoFlagTicks = 0;
            }
        } else {
            _carrierNoFlagTicks = 0;
        }
        _flagsAtLastWindow = _rxFlagCount;
        _peakInWindow = 0;
        _clipInWindow = 0;
        _markInWindow = 0;
        _levelWindowSamples = 0;
    }

    // Procesar todas las muestras continuamente a 9600 Hz en el demodulador DSP
    if (absSample > _rxPeakTrack) {
        _rxPeakTrack = absSample;
    }
    processRxSample(centered);
}

void AfskModem::dspTaskLoop() {
    const int64_t samplePeriodUs = 1000000LL / AFSK_SAMPLE_RATE; // ~104 us

    // Incrementos de fase DDS (16 bits): f * 65536 / 9600
    const uint16_t phaseIncMark  = (uint16_t)((AFSK_MARK_FREQ * 65536UL) / AFSK_SAMPLE_RATE);
    const uint16_t phaseIncSpace = (uint16_t)((AFSK_SPACE_FREQ * 65536UL) / AFSK_SAMPLE_RATE);

    TxJob currentJob;

    while (true) {
        // Dormir esperando paquete a transmitir (0% CPU en reposo)
        if (xQueueReceive(_txQueue, &currentJob, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }

        uint8_t activePin = _txAudioPin;
        bool useSigmaDelta = (activePin != AFSK_DAC2_PIN);

        setPtt(true);

        // Si es loopback, reiniciar estado del demodulador para recibir limpio
        if (currentJob.loopback) {
            _rxOnesCount = 0;
            _rxInFrame = false;
            _rxBitIndex = 0;
            _rxFrameLen = 0;
            _lpfCorrInt = 0;
            _pllCounter = 0;
        }

        uint16_t ddsPhase = 0;
        bool txToneMark = true; // Tono actual (Mark = true, Space = false)
        uint8_t txOnesCount = 0;
        int64_t nextSampleTime = esp_timer_get_time();

        // Lambda para emitir 1 bit a 1200 bps (8 muestras a 9600 Hz)
        auto sendBit = [&](bool bitVal, bool stuffEnabled) {
            auto emitPhysicalBit = [&](bool logicalBit) {
                // Codificación NRZI: bit '0' invierte el tono, bit '1' mantiene el tono
                if (!logicalBit) {
                    txToneMark = !txToneMark;
                }
                uint16_t phaseInc = txToneMark ? phaseIncMark : phaseIncSpace;

                for (int s = 0; s < AFSK_SAMPLES_PER_BIT; s++) {
                    ddsPhase += phaseInc;
                    uint8_t rawSine = s_sineLut[ddsPhase >> 8];
                    int16_t centered = (int16_t)rawSine - 128;
                    int16_t scaled = (centered * (int16_t)_txVolume) / 100;
                    uint8_t dacVal = (uint8_t)(128 + scaled);

                    // 1. Salida por DAC2 interno (GPIO 26 -> conector SPK P4)
                    dac_output_voltage(DAC_CHANNEL_2, dacVal);

                    // 2. Salida simultánea por Sigma-Delta en el pin del conector blanco elegido (27, 21, 4 o 17)
                    if (useSigmaDelta) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
                        sigmaDeltaWrite(activePin, dacVal);
#else
                        sigmaDeltaWrite(SIGMADELTA_TX_CH, dacVal);
#endif
                    }

                    if (currentJob.loopback) {
                        processRxSample(centered * 8);
                    }

                    nextSampleTime += samplePeriodUs;
                    int64_t now = esp_timer_get_time();
                    if (nextSampleTime > now) {
                        ets_delay_us((uint32_t)(nextSampleTime - now));
                    } else {
                        nextSampleTime = now;
                    }
                }
            };

            emitPhysicalBit(bitVal);

            if (stuffEnabled) {
                if (bitVal) {
                    txOnesCount++;
                    if (txOnesCount == 5) {
                        // Bit-stuffing: insertar un '0' tras cinco '1's consecutivos
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
                bool bit = (byteVal >> b) & 0x01; // LSB primero
                sendBit(bit, !isFlag);
            }
        };

        // 1. Preámbulo de banderas 0x7E (TX Delay)
        for (uint16_t i = 0; i < _txDelayFlags; i++) {
            sendByte(0x7E, true);
        }

        // 2. Cuerpo de la trama AX.25 + FCS (con bit-stuffing activo)
        txOnesCount = 0;
        for (uint16_t i = 0; i < currentJob.len; i++) {
            sendByte(currentJob.data[i], false);
        }

        // 3. Cola de banderas 0x7E (TX Tail)
        for (uint16_t i = 0; i < _txTailFlags; i++) {
            sendByte(0x7E, true);
        }

        // Silenciar DAC al punto medio (1.65V), desacoplar Sigma-Delta y soltar PTT
        dac_output_voltage(DAC_CHANNEL_2, 128);
        setPtt(false);
        _txCount++;

        // Dar respiro al planificador de FreeRTOS tras transmitir
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void AfskModem::processRxSample(int16_t centeredSample) {
    static_assert(AFSK_SAMPLES_PER_BIT == 8, "AFSK_SAMPLES_PER_BIT debe ser 8 para mascara & 7");
    _sampleBuf[_sampleIdx] = centeredSample;
    _sampleIdx = (_sampleIdx + 1) & 7;

    // Correlación en cuadratura 100% entera sobre ventana deslizante de 8 muestras
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

    // Filtro pasa-bajos post-detección entero (alpha = 5/16 = 0.3125, sin float)
    _lpfCorrInt = (_lpfCorrInt * 11 + diff * 5) >> 4;
    bool currentToneMark = (_lpfCorrInt >= 0);
    if (currentToneMark) {
        _markInWindow++;
    }

    // Recuperación de reloj (Digital PLL) en transiciones de tono
    if (currentToneMark != _lastToneMark) {
        _lastToneMark = currentToneMark;
        // Ajustar fase hacia el centro del bit (evita deriva de reloj)
        if (_pllCounter < (AFSK_SAMPLES_PER_BIT / 2)) {
            _pllCounter += 1;
        } else {
            _pllCounter -= 1;
        }
    }

    _pllCounter++;
    if (_pllCounter >= AFSK_SAMPLES_PER_BIT) {
        _pllCounter -= AFSK_SAMPLES_PER_BIT;

        // Decodificación NRZI en el centro del bit:
        // Sin cambio de tono = '1', Cambio de tono = '0'
        bool nrziBit = (currentToneMark == _nrziLastTone);
        _nrziLastTone = currentToneMark;

        processRxBit(nrziBit);
    }
}

void AfskModem::processRxBit(bool nrziBit) {
    if (nrziBit) {
        // Bit '1' recibido
        _rxOnesCount++;
        if (_rxOnesCount >= 7) {
            // 7 unos seguidos = Abortar trama HDLC (solo contabilizar si hubo portadora de audio real)
            if (_rxInFrame && _rxFrameLen >= 4 && _rxPeakTrack > 100) {
                _rxAbortCount++;
                _lastFailLen = _rxFrameLen;
                _lastErr = RX_ERR_ABORT;
            }
            _rxInFrame = false;
            _rxFrameLen = 0;
            _rxBitIndex = 0;
            return;
        }
        if (_rxInFrame) {
            _rxCurrentByte = (_rxCurrentByte >> 1) | 0x80;
            _rxBitIndex++;
            if (_rxBitIndex == 8) {
                if (_rxFrameLen < MAX_AX25_FRAME_LEN) {
                    _rxFrameBuf[_rxFrameLen++] = _rxCurrentByte;
                } else {
                    _rxInFrame = false;
                }
                _rxCurrentByte = 0;
                _rxBitIndex = 0;
            }
        }
    } else {
        // Bit '0' recibido
        if (_rxOnesCount == 6) {
            // Se detectó bandera HDLC 0x7E (01111110) — contabilizar solo con audio real (>100 cuentas)
            bool hasAudio = (_rxLevelPercent >= 10) || (_rxPeakTrack > 100);
            if (hasAudio) {
                _rxFlagCount++;
            }
            if (_rxInFrame && _rxFrameLen >= 16) {
                // Verificar CRC-16 CCITT de la trama recibida
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
                    _lastErr = RX_ERR_NONE;
                } else if (hasAudio) {
                    _rxCrcFailCount++;
                    _lastFailLen = _rxFrameLen;
                    _lastErr = RX_ERR_CRC_FAIL;
                }
            } else if (_rxInFrame && _rxFrameLen >= 3 && hasAudio) {
                _rxShortCount++;
                _lastFailLen = _rxFrameLen;
                if (_lastErr != RX_ERR_CRC_FAIL) {
                    _lastErr = RX_ERR_SHORT;
                }
            }
            // Iniciar/reiniciar captura de nueva trama
            _rxInFrame = true;
            _rxFrameLen = 0;
            _rxBitIndex = 0;
            _rxCurrentByte = 0;
            _rxPeakTrack = 0;
            _rxOnesCount = 0;
            return;
        } else if (_rxOnesCount == 5) {
            // Bit '0' de relleno (bit-stuffing): descartarlo
            _rxOnesCount = 0;
            return;
        }

        // Bit '0' normal de datos
        _rxOnesCount = 0;
        if (_rxInFrame) {
            _rxCurrentByte = (_rxCurrentByte >> 1);
            _rxBitIndex++;
            if (_rxBitIndex == 8) {
                if (_rxFrameLen < MAX_AX25_FRAME_LEN) {
                    _rxFrameBuf[_rxFrameLen++] = _rxCurrentByte;
                } else {
                    _rxInFrame = false;
                }
                _rxCurrentByte = 0;
                _rxBitIndex = 0;
            }
        }
    }
}
