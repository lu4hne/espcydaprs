#include "ui_manager.h"
#include "igate_manager.h"
#include "update_manager.h"
#include "sensor_manager.h"

UiManager Ui;

// Paleta de colores RGB565 estilo Radioaficionado / Oscuro de alto contraste
#define COL_BG          0x0842  // Azul noche muy oscuro
#define COL_HEADER      0x10E4  // Azul pizarra oscuro
#define COL_CARD        0x18E3  // Gris azulado para tarjetas
#define COL_BORDER      0x39E7  // Borde suave
#define COL_ACCENT      0x05FF  // Cian brillante
#define COL_GREEN       0x07E0  // Verde activo / RX / Verificado
#define COL_RED         0xF800  // Rojo TX / Alerta
#define COL_ORANGE      0xFD20  // Ámbar / Advertencia
#define COL_YELLOW      0xFFE0  // Amarillo / Indicativos
#define COL_WHITE       0xFFFF
#define COL_MUTED       0x9CF3  // Gris claro para etiquetas

UiManager::UiManager() : _tft(TFT_eSPI()) {}

void UiManager::begin() {
    _tft.init();
    applyDisplayHardware();
    _tft.fillScreen(COL_BG);

    _touch.begin(&_tft, Aprs.config().boardModel == BOARD_MODEL_ILI9341);
    _needsFullRedraw = true;
}

void UiManager::applyDisplayHardware() {
    AprsConfig& cfg = Aprs.config();

    // Configurar brillo por PWM (LEDC canal 0, Timer 0) para el pin de Backlight que NO esté asignado a Audio TX
    // En modelo ST7789 (1 USB) el Backlight usa GPIO 27/21; en 2432S024C usa GPIO 27/21.
    if (cfg.tftBrightness < 15 || cfg.tftBrightness > 100) cfg.tftBrightness = 100;
    uint8_t blDuty = (uint8_t)(((uint32_t)cfg.tftBrightness * 255UL) / 100UL);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    if (cfg.txAudioPin != CYD_BL_PIN_21) {
        pinMode(CYD_BL_PIN_21, OUTPUT);
        ledcAttach(CYD_BL_PIN_21, 5000, 8);
        ledcWrite(CYD_BL_PIN_21, blDuty);
    } else {
        ledcDetach(CYD_BL_PIN_21);
    }
    if (cfg.txAudioPin != CYD_BL_PIN_27) {
        pinMode(CYD_BL_PIN_27, OUTPUT);
        ledcAttach(CYD_BL_PIN_27, 5000, 8);
        ledcWrite(CYD_BL_PIN_27, blDuty);
    } else {
        ledcDetach(CYD_BL_PIN_27);
    }
#else
    ledcSetup(0, 5000, 8);
    if (cfg.txAudioPin != CYD_BL_PIN_21) {
        pinMode(CYD_BL_PIN_21, OUTPUT);
        ledcAttachPin(CYD_BL_PIN_21, 0);
    } else {
        ledcDetachPin(CYD_BL_PIN_21);
    }
    if (cfg.txAudioPin != CYD_BL_PIN_27) {
        pinMode(CYD_BL_PIN_27, OUTPUT);
        ledcAttachPin(CYD_BL_PIN_27, 0);
    } else {
        ledcDetachPin(CYD_BL_PIN_27);
    }
    ledcWrite(0, blDuty);
#endif

    // Configurar pines GPIO 16 / GPIO 17 (LEDC canal 2, Timer 1 independiente):
    // En la placa CYD (ST7789 / ILI9341), el fabricante cableó el LED RGB trasero (R=IO4, G=IO16, B=IO17)
    // en paralelo con los pines del conector blanco P3 (IO4, IO16, IO17).
    // - Si Posición = GPS (Modo Tracker con GPS externo), IO17 (RX2) e IO16 (TX2) se dedican al UART2 del GPS.
    // - Si Posición = FIJA (sin GPS externo), IO16 e IO17 quedan libres y se controlan por PWM para no encandilar.
    if (cfg.posMode == POS_MODE_GPS && cfg.txAudioPin != GPS_UART_RX_PIN) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
        ledcDetach(CYD_LED_B);
#else
        ledcDetachPin(CYD_LED_B);
#endif
        Serial2.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_UART_RX_PIN, GPS_UART_TX_PIN);
    } else if (cfg.txAudioPin != CYD_LED_B) {
        Serial2.end();
        pinMode(CYD_LED_G, OUTPUT);
        digitalWrite(CYD_LED_G, HIGH); // Verde apagado en reposo
        uint8_t blueDuty = 255; // 255 = Apagado (activo en LOW)
        if (Igate.isAprsIsVerified() && cfg.ledBrightness > 0) {
            blueDuty = (cfg.ledBrightness == 1) ? 252 : 232; // 252 = ~1.2% muy tenue, 232 = ~9% medio
        }
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
        ledcAttach(CYD_LED_B, 5000, 8);
        ledcWrite(CYD_LED_B, blueDuty);
#else
        ledcSetup(2, 5000, 8);
        ledcAttachPin(CYD_LED_B, 2);
        ledcWrite(2, blueDuty);
#endif
    }

    if (cfg.boardModel == BOARD_MODEL_ST7789) {
        // Secuencia completa de inicialización ST7789 idéntica a Panel_ST7789 de LovyanGFX (WeatherClock)
        // Primero SWRESET (0x01) para limpiar los registros 0xC0..0xC7 que _tft.init() envió para ILI9341
        _tft.writecommand(0x01); // SWRESET
        delay(130);
        _tft.writecommand(0x11); // SLPOUT
        delay(120);

        _tft.writecommand(0xB7); _tft.writedata(0x35); // GCTRL
        _tft.writecommand(0xBB); _tft.writedata(0x28); // VCOMS (JLX240 datasheet)
        _tft.writecommand(0xC0); _tft.writedata(0x0C); // LCMCTRL
        _tft.writecommand(0xC2); _tft.writedata(0x01); _tft.writedata(0xFF); // VDVVRHEN
        _tft.writecommand(0xC3); _tft.writedata(0x10); // VRHS
        _tft.writecommand(0xC4); _tft.writedata(0x20); // VDVSET
        _tft.writecommand(0xD0); _tft.writedata(0xA4); _tft.writedata(0xA1); // PWCTRL1
        _tft.writecommand(0xB0); _tft.writedata(0x00); _tft.writedata(0xC0); // RAMCTRL

        static const uint8_t st_pvgam[14] = {
            0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x32, 0x44, 0x42, 0x06, 0x0E, 0x12, 0x14, 0x17
        };
        _tft.writecommand(0xE0); // PVGAMCTRL
        for (uint8_t b : st_pvgam) _tft.writedata(b);

        static const uint8_t st_nvgam[14] = {
            0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x31, 0x54, 0x47, 0x0E, 0x1C, 0x17, 0x1B, 0x1E
        };
        _tft.writecommand(0xE1); // NVGAMCTRL
        for (uint8_t b : st_nvgam) _tft.writedata(b);

        _tft.writecommand(0x11); // SLPOUT
        delay(130);
        _tft.writecommand(0x38); // IDMOFF
        _tft.writecommand(0x29); // DISPON
        delay(20);

        // Actualizar dimensiones internas 320x240 en TFT_eSPI y luego fijar COLMOD + MADCTL de ST7789
        _tft.setRotation(1);
        _tft.writecommand(0x3A); _tft.writedata(0x55); // COLMOD: 16-bit RGB565
        _tft.writecommand(0x36); _tft.writedata(0x6C); // MADCTL: MV | MX | MH | BGR (igual a WeatherClock rotation=1)
        _tft.writecommand(cfg.displayInvert ? 0x21 : 0x20);
    } else {
        // Re-inicializar registros nativos de ILI9341 por si se acaba de conmutar desde ST7789
        _tft.writecommand(0x01); // SWRESET
        delay(120);
        _tft.init();
        _tft.setRotation(1); // 320x240 Horizontal
        _tft.writecommand(cfg.displayInvert ? 0x21 : 0x20);
    }
    _touch.begin(&_tft, cfg.boardModel == BOARD_MODEL_ILI9341);
    _needsFullRedraw = true;
}

void UiManager::update() {
    // 1. Leer entrada táctil con anti-rebote
    uint16_t tx = 0, ty = 0;
    if (millis() - _lastTouchMs > 180) {
        if (_touch.getTouch(&tx, &ty, 1)) {
            _lastTouchMs = millis();
            handleTouch(tx, ty);
        }
    }

    // 2. Modales de emergencia / cancelación / teclado
    if (_showEmergModal) {
        if (millis() - _emergModalTimerMs > 5000) {
            // Timeout de 5 segundos sin tocar: se cancela y cierra solo, refrescando el panel completo
            _showEmergModal = false;
            _emergConfirmStep = 0;
            _needsFullRedraw = true;
        } else {
            if (_needsFullRedraw) {
                drawEmergConfirmModal();
                _needsFullRedraw = false;
            } else {
                // Actualizar barra regresiva de 5 segundos
                uint32_t elapsed = millis() - _emergModalTimerMs;
                int barW = 276;
                int remW = (int)(((5000 - elapsed) * barW) / 5000);
                if (remW < 0) remW = 0;
                _tft.fillRect(22, 154, remW, 8, (elapsed < 3000) ? COL_ORANGE : COL_RED);
                _tft.fillRect(22 + remW, 154, barW - remW, 8, COL_BG);
            }
            return;
        }
    }

    if (_showCancelModal) {
        if (_needsFullRedraw) {
            drawEmergCancelModal();
            _needsFullRedraw = false;
        }
        return;
    }

    if (_kbTarget != KB_NONE) {
        if (_needsFullRedraw) {
            drawKeyboard();
            _needsFullRedraw = false;
        }
        return;
    }

    // 3. Redibujado completo al cambiar de pestaña o recibir nuevo paquete / ACK
    uint32_t rxNow  = Modem.getRxPacketCount();
    uint32_t txNow  = Modem.getTxPacketCount();
    uint32_t igNow  = Igate.getRfToIsCount();
    uint32_t revNow = Aprs.getUiRevision();
    if ((rxNow != _lastDrawnRxCount || igNow != _lastDrawnIgCount || revNow != _lastDrawnUiRev) &&
        (_currentTab == TAB_MONITOR || _currentTab == TAB_STATIONS || _currentTab == TAB_MESSAGES)) {
        _needsFullRedraw = true;
    }

    if (_needsFullRedraw) {
        _tft.fillScreen(COL_BG);
        drawTopBar();
        drawBottomTabs();
        drawCurrentTab(true);
        _needsFullRedraw = false;
        _lastDynamicUpdateMs = millis();
        _lastDrawnRxCount = rxNow;
        _lastDrawnTxCount = txNow;
        _lastDrawnIgCount = igNow;
        _lastDrawnUiRev   = revNow;
        return;
    }

    // 4. Actualización periódica ligera (cada 200 ms) sin parpadeo
    if (millis() - _lastDynamicUpdateMs >= 200) {
        _lastDynamicUpdateMs = millis();
        drawTopBar();
        if (_currentTab == TAB_DASHBOARD) {
            drawDashboard(false);
        }

        // Actualizar LED RGB de estado (Azul tenue = IGate Online, Verde = RX DCD)
        // solo si IO16/17 no están en uso por GPS ni por un sensor I2C/DHT conectado
        AprsConfig& cfg = Aprs.config();
        bool pins1617UsedBySensor = Sensors.isConnected() && (cfg.sensorPinProfile == SENSOR_PINS_16_17);
        if (cfg.posMode == POS_MODE_FIXED && cfg.txAudioPin != CYD_LED_B && !pins1617UsedBySensor) {
            digitalWrite(CYD_LED_G, Modem.isCarrierDetected() ? LOW : HIGH);
            uint8_t blueDuty = 255; // Apagado (activo en LOW)
            if (Igate.isAprsIsVerified() && cfg.ledBrightness > 0) {
                blueDuty = (cfg.ledBrightness == 1) ? 252 : 232;
            }
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
            ledcWrite(CYD_LED_B, blueDuty);
#else
            ledcWrite(2, blueDuty);
#endif
        }
    }
}

void UiManager::drawTopBar() {
    AprsConfig& cfg = Aprs.config();
    if (Aprs.isEmergencyActive()) {
        _tft.fillRect(0, 0, 320, 22, COL_RED);
        _tft.drawFastHLine(0, 22, 320, COL_WHITE);
        _tft.setTextColor(COL_WHITE, COL_RED);
        _tft.drawCentreString("*** SOS EMERGENCIA ACTIVA ***", 160, 3, 2);
        return;
    }
    _tft.fillRect(0, 0, 320, 22, COL_HEADER);
    _tft.drawFastHLine(0, 22, 320, COL_BORDER);

    // Indicativo propio
    char myCall[16];
    if (cfg.ssid > 0) snprintf(myCall, sizeof(myCall), "%s-%u", cfg.callsign, cfg.ssid);
    else snprintf(myCall, sizeof(myCall), "%s", cfg.callsign);

    _tft.setTextColor(COL_YELLOW, COL_HEADER);
    _tft.drawString(myCall, 6, 3, 2);

    // Rol activo: TRK (Tracker) / IGATE (Estación Fija + IGate) / FIJA (Base RF)
    if (cfg.stationMode == STATION_MODE_FIXED_IGATE || cfg.igateEnabled) {
        if (Igate.isAprsIsVerified()) {
            _tft.setTextColor(COL_GREEN, COL_HEADER);
            _tft.drawString("IG:ON", 88, 3, 2);
        } else if (Igate.isWifiConnected()) {
            _tft.setTextColor(COL_ACCENT, COL_HEADER);
            _tft.drawString("IG:TCP", 88, 3, 2);
        } else {
            _tft.setTextColor(COL_ORANGE, COL_HEADER);
            _tft.drawString("IG:WIFI", 88, 3, 2);
        }
    } else if (cfg.stationMode == STATION_MODE_TRACKER) {
        _tft.setTextColor(COL_ACCENT, COL_HEADER);
        _tft.drawString("TRACK", 88, 3, 2);
    } else {
        _tft.setTextColor(COL_ACCENT, COL_HEADER);
        _tft.drawString("FIJA", 88, 3, 2);
    }

    // Estado de Posición (FIJA o GPS)
    char posBadge[20];
    if (cfg.posMode == POS_MODE_FIXED) {
        snprintf(posBadge, sizeof(posBadge), "POS:FIJA");
        _tft.setTextColor(COL_MUTED, COL_HEADER);
    } else {
        const GpsState& gps = Aprs.getGpsState();
        if (gps.validFix) {
            snprintf(posBadge, sizeof(posBadge), "GPS:%uSAT", gps.satellites);
            _tft.setTextColor(COL_GREEN, COL_HEADER);
        } else {
            snprintf(posBadge, sizeof(posBadge), "GPS:BUSC");
            _tft.setTextColor(COL_ORANGE, COL_HEADER);
        }
    }
    _tft.drawString(posBadge, 146, 3, 2);

    // Estado TX / RX / IDLE a la derecha
    if (Modem.isTransmitting()) {
        _tft.fillRoundRect(236, 2, 78, 18, 3, COL_RED);
        _tft.setTextColor(COL_WHITE, COL_RED);
        _tft.drawCentreString("TX 1200", 275, 3, 2);
    } else if (Modem.isCarrierDetected()) {
        _tft.fillRoundRect(236, 2, 78, 18, 3, COL_GREEN);
        _tft.setTextColor(COL_BG, COL_GREEN);
        _tft.drawCentreString("RX DCD", 275, 3, 2);
    } else {
        _tft.fillRoundRect(236, 2, 78, 18, 3, COL_CARD);
        _tft.setTextColor(COL_MUTED, COL_CARD);
        _tft.drawCentreString("RX 1200", 275, 3, 2);
    }
}

void UiManager::drawBottomTabs() {
    const char* labels[5] = { "PANEL", "MONIT", "MSJS", "ESTAC", "CONF" };
    int tabW = 320 / 5; // 64px por pestaña
    int y = 212;

    for (int i = 0; i < 5; i++) {
        bool sel = ((int)_currentTab == i);
        uint16_t bg = sel ? COL_ACCENT : COL_HEADER;
        uint16_t fg = sel ? COL_BG : COL_WHITE;

        int x0 = i * tabW;
        int w  = (i == 4) ? (320 - x0) : (tabW - 1);
        _tft.fillRect(x0, y, w, 28, bg);
        _tft.drawFastHLine(x0, y, w, sel ? COL_WHITE : COL_BORDER);
        _tft.setTextColor(fg, bg);
        _tft.drawCentreString(labels[i], x0 + (w / 2), y + 6, 2);

        // Insignia de mensajes no leídos en MSJS
        if (i == TAB_MESSAGES && Aprs.getUnreadMessageCount() > 0) {
            _tft.fillCircle(x0 + w - 8, y + 7, 4, COL_RED);
        }
    }
}

void UiManager::drawCurrentTab(bool fullRedraw) {
    switch (_currentTab) {
        case TAB_DASHBOARD: drawDashboard(fullRedraw); break;
        case TAB_MONITOR:   drawMonitor(fullRedraw);   break;
        case TAB_MESSAGES:  drawMessages(fullRedraw);  break;
        case TAB_STATIONS:  drawStations(fullRedraw);  break;
        case TAB_CONFIG:    drawConfig(fullRedraw);    break;
    }
}

void UiManager::drawDashboard(bool fullRedraw) {
    AprsConfig& cfg = Aprs.config();

    if (Aprs.isEmergencyActive()) {
        const EmergencyState& emg = Aprs.getEmergencyState();
        uint32_t elapsedSec = (millis() - emg.startTimeMs) / 1000UL;
        uint32_t mm = elapsedSec / 60;
        uint32_t ss = elapsedSec % 60;

        if (fullRedraw) {
            // Tarjeta Superior (y=28..94): Alerta SOS y Estado
            _tft.fillRoundRect(6, 28, 308, 66, 4, 0x4800);
            _tft.drawRoundRect(6, 28, 308, 66, 4, COL_RED);

            // Tarjeta Central (y=98..164): Detalles de la emergencia
            _tft.fillRoundRect(6, 98, 308, 66, 4, COL_CARD);
            _tft.drawRoundRect(6, 98, 308, 66, 4, COL_BORDER);

            // Botón inferior grande (y=170..208): CANCELAR EMERGENCIA
            _tft.fillRoundRect(6, 170, 308, 38, 4, COL_RED);
            _tft.drawRoundRect(6, 170, 308, 38, 4, COL_WHITE);
            _tft.setTextColor(COL_WHITE, COL_RED);
            _tft.drawCentreString(">>> CANCELAR EMERGENCIA <<<", 160, 180, 2);
        }

        // Datos dinámicos en Tarjeta Superior
        _tft.setTextColor(COL_WHITE, 0x4800);
        char titleBuf[48];
        snprintf(titleBuf, sizeof(titleBuf), "ALERTA: %s", Aprs.getEmergencyTypeStr(emg.type));
        _tft.drawString(titleBuf, 12, 32, 2);

        char posBuf[48];
        snprintf(posBuf, sizeof(posBuf), "Pos: %.4f, %.4f (%dm)",
                 Aprs.getCurrentLat(), Aprs.getCurrentLon(), Aprs.getCurrentAlt());
        _tft.setTextColor(COL_YELLOW, 0x4800);
        _tft.drawString(posBuf, 12, 51, 2);

        char timeBuf[48];
        snprintf(timeBuf, sizeof(timeBuf), "Activo: %02lu:%02lu | Balizas: %u", mm, ss, emg.beaconCount);
        _tft.setTextColor(COL_WHITE, 0x4800);
        _tft.drawString(timeBuf, 12, 70, 2);

        // Datos en Tarjeta Central
        char info1[48], info2[48];
        snprintf(info1, sizeof(info1), "Intervalo: cada %u s (Sig: %lu s)",
                 cfg.emergIntervalSec, (unsigned long)Aprs.getSecondsUntilNextBeacon());
        _tft.setTextColor(COL_ACCENT, COL_CARD);
        _tft.drawString(info1, 12, 104, 2);

        if (cfg.emergSendDirectMsg && strlen(cfg.emergMsgDest) > 0) {
            snprintf(info2, sizeof(info2), "Msj Directo: %s | Sirena: %s",
                     cfg.emergMsgDest, cfg.emergSoundAlert ? "ON" : "OFF");
        } else {
            snprintf(info2, sizeof(info2), "Msj Directo: NO | Sirena: %s",
                     cfg.emergSoundAlert ? "ON" : "OFF");
        }
        _tft.setTextColor(COL_MUTED, COL_CARD);
        _tft.drawString(info2, 12, 123, 2);

        _tft.setTextColor(COL_GREEN, COL_CARD);
        _tft.drawString("Transmision AX.25 RF activa (\\!)", 12, 142, 2);

        return;
    }

    if (fullRedraw) {
        // Tarjeta 1: Modo, Coordenadas y Estado de Estación / IGate
        _tft.fillRoundRect(6, 28, 198, 66, 4, COL_CARD);
        _tft.drawRoundRect(6, 28, 198, 66, 4, COL_BORDER);

        // Tarjeta 2: Contadores TX / RX / IGate
        _tft.fillRoundRect(210, 28, 104, 66, 4, COL_CARD);
        _tft.drawRoundRect(210, 28, 104, 66, 4, COL_BORDER);

        // Tarjeta 3: Vúmetro de Audio RX (GPIO 35)
        _tft.fillRoundRect(6, 98, 308, 28, 4, COL_CARD);
        _tft.drawRoundRect(6, 98, 308, 28, 4, COL_BORDER);
        _tft.setTextColor(COL_MUTED, COL_CARD);
        _tft.drawString("AUDIO RX:", 12, 104, 2);

        // Tarjeta 4: Último paquete decodificado
        _tft.fillRoundRect(6, 130, 308, 42, 4, COL_CARD);
        _tft.drawRoundRect(6, 130, 308, 42, 4, COL_BORDER);

        // Botones de acción rápida abajo (y = 176..207)
        if (cfg.stationMode == STATION_MODE_TRACKER) {
            // Modo Tracker Móvil: 4 botones incluyendo [SOS]
            _tft.fillRoundRect(4, 176, 74, 31, 4, COL_GREEN);
            _tft.setTextColor(COL_BG, COL_GREEN);
            _tft.drawCentreString("BALIZA", 41, 184, 2);

            _tft.fillRoundRect(82, 176, 74, 31, 4, COL_ACCENT);
            _tft.setTextColor(COL_BG, COL_ACCENT);
            _tft.drawCentreString("LOOP", 119, 184, 2);

            _tft.fillRoundRect(238, 176, 78, 31, 4, 0x8000);
            _tft.drawRoundRect(238, 176, 78, 31, 4, COL_RED);
            _tft.setTextColor(COL_WHITE, 0x8000);
            _tft.drawCentreString("SOS", 277, 184, 2);
        } else {
            // Modos Fijos / IGate: 3 botones tradicionales sin botón SOS
            _tft.fillRoundRect(6, 176, 106, 31, 4, COL_GREEN);
            _tft.setTextColor(COL_BG, COL_GREEN);
            _tft.drawCentreString("TX BALIZA", 59, 184, 2);

            _tft.fillRoundRect(117, 176, 102, 31, 4, COL_ACCENT);
            _tft.setTextColor(COL_BG, COL_ACCENT);
            _tft.drawCentreString("LOOPBACK", 168, 184, 2);
        }
    }

    // Botón AUTO BCN (puede cambiar de estado dinámicamente)
    if (cfg.stationMode == STATION_MODE_TRACKER) {
        uint16_t autoBg = cfg.autoBeacon ? COL_ORANGE : COL_HEADER;
        _tft.fillRoundRect(160, 176, 74, 31, 4, autoBg);
        _tft.drawRoundRect(160, 176, 74, 31, 4, COL_BORDER);
        _tft.setTextColor(cfg.autoBeacon ? COL_BG : COL_WHITE, autoBg);
        if (cfg.autoBeacon) {
            char bcnBuf[12];
            snprintf(bcnBuf, sizeof(bcnBuf), "A:%lus", (unsigned long)Aprs.getSecondsUntilNextBeacon());
            _tft.drawCentreString(bcnBuf, 197, 184, 2);
        } else {
            _tft.drawCentreString("AUTO:OFF", 197, 184, 2);
        }
    } else {
        uint16_t autoBg = cfg.autoBeacon ? COL_ORANGE : COL_HEADER;
        _tft.fillRoundRect(224, 176, 90, 31, 4, autoBg);
        _tft.drawRoundRect(224, 176, 90, 31, 4, COL_BORDER);
        _tft.setTextColor(cfg.autoBeacon ? COL_BG : COL_WHITE, autoBg);
        if (cfg.autoBeacon) {
            char bcnBuf[16];
            snprintf(bcnBuf, sizeof(bcnBuf), "AUTO %lus", (unsigned long)Aprs.getSecondsUntilNextBeacon());
            _tft.drawCentreString(bcnBuf, 269, 184, 2);
        } else {
            _tft.drawCentreString("AUTO: OFF", 269, 184, 2);
        }
    }

    // Datos de posición y modo en Tarjeta 1 (Tocar Tarjeta 1 cambia rápido Tracker <-> Fija+IGate)
    const char* modeTitle = (cfg.stationMode == STATION_MODE_FIXED_IGATE) ? "MODO: FIJA + IGATE" :
                            (cfg.stationMode == STATION_MODE_TRACKER)     ? "MODO: TRACKER RF" :
                                                                            "MODO: FIJA SOLO RF";
    _tft.setTextColor(COL_YELLOW, COL_CARD);
    _tft.drawString(modeTitle, 12, 32, 2);

    char latLonBuf[36];
    if (Sensors.isConnected()) {
        const SensorReading& sr = Sensors.reading();
        if (sr.hasHumidity && sr.hasPressure) {
            snprintf(latLonBuf, sizeof(latLonBuf), "WX: %.1fC %.0f%% %.0fhPa    ", sr.temperatureC, sr.humidityPct, sr.pressureHpa);
        } else if (sr.hasPressure) {
            snprintf(latLonBuf, sizeof(latLonBuf), "WX: %.1fC | %.1f hPa    ", sr.temperatureC, sr.pressureHpa);
        } else {
            snprintf(latLonBuf, sizeof(latLonBuf), "WX: %.1fC | Hum: %.0f%%    ", sr.temperatureC, sr.humidityPct);
        }
        _tft.setTextColor(COL_ACCENT, COL_CARD);
    } else {
        snprintf(latLonBuf, sizeof(latLonBuf), "%.4f, %.4f (%dm)",
                 Aprs.getCurrentLat(), Aprs.getCurrentLon(), Aprs.getCurrentAlt());
        _tft.setTextColor(COL_WHITE, COL_CARD);
    }
    _tft.drawString(latLonBuf, 12, 51, 2);

    char line3Buf[34];
    if (cfg.stationMode == STATION_MODE_FIXED_IGATE || cfg.igateEnabled) {
        if (Igate.isAprsIsVerified()) {
            snprintf(line3Buf, sizeof(line3Buf), "APRS-IS OK (%s)", Igate.getLocalIpString().c_str());
            _tft.setTextColor(COL_GREEN, COL_CARD);
        } else {
            snprintf(line3Buf, sizeof(line3Buf), "IS: %s", Igate.getStateText());
            _tft.setTextColor(COL_ORANGE, COL_CARD);
        }
    } else {
        snprintf(line3Buf, sizeof(line3Buf), "TX:IO%u RX:35 PTT:22 [%s]",
                 cfg.txAudioPin,
                 cfg.radioType == RADIO_BAOFENG ? "BFG" : "YSU");
        _tft.setTextColor(COL_ACCENT, COL_CARD);
    }
    _tft.fillRect(10, 70, 188, 18, COL_CARD);
    _tft.drawString(line3Buf, 12, 71, 2);

    // Contadores TX / RX / IGate en Tarjeta 2
    char countBuf[24];
    snprintf(countBuf, sizeof(countBuf), "TX: %lu", (unsigned long)Modem.getTxPacketCount());
    _tft.setTextColor(COL_ORANGE, COL_CARD);
    _tft.drawString(countBuf, 218, 33, 2);

    snprintf(countBuf, sizeof(countBuf), "RX: %lu", (unsigned long)Modem.getRxPacketCount());
    _tft.setTextColor(COL_GREEN, COL_CARD);
    _tft.drawString(countBuf, 218, 52, 2);

    if (cfg.stationMode == STATION_MODE_FIXED_IGATE || cfg.igateEnabled) {
        snprintf(countBuf, sizeof(countBuf), "IG: %lu", (unsigned long)Igate.getRfToIsCount());
        _tft.setTextColor(COL_ACCENT, COL_CARD);
    } else {
        snprintf(countBuf, sizeof(countBuf), "Est: %d", Aprs.getStationCount());
        _tft.setTextColor(COL_WHITE, COL_CARD);
    }
    _tft.fillRect(216, 70, 92, 18, COL_CARD);
    _tft.drawString(countBuf, 218, 71, 2);

    // Barra de nivel de Audio RX (Vúmetro para ajustar volumen del Baofeng/Yaesu)
    uint8_t audioPct = Modem.getRxAudioLevel();
    int barMaxW = 186;
    int barFill = (audioPct * barMaxW) / 100;
    uint16_t barCol = (audioPct < 15) ? COL_MUTED :
                      (audioPct <= 80) ? COL_GREEN : COL_RED;
    _tft.fillRect(82, 105, barFill, 14, barCol);
    _tft.fillRect(82 + barFill, 105, barMaxW - barFill, 14, COL_BG);
    _tft.drawRect(81, 104, barMaxW + 2, 16, COL_BORDER);

    char pctStr[10];
    snprintf(pctStr, sizeof(pctStr), "%3u%%", audioPct);
    _tft.setTextColor(COL_WHITE, COL_CARD);
    _tft.drawString(pctStr, 274, 104, 2);

    // Último paquete recibido en Tarjeta 4
    if (Aprs.getPacketLogCount() > 0) {
        const DecodedPacket& last = Aprs.getPacketLog(0);
        char hdr[48];
        const char* igTag = last.gatedToIs ? " [->IS]" : "";
        if (last.hasPosition) {
            snprintf(hdr, sizeof(hdr), "%s -> %.1fkm (%d%s)%s  ",
                     last.srcCall, last.distanceKm, last.bearingDeg,
                     AprsEngine::bearingToCardinal(last.bearingDeg), igTag);
        } else {
            snprintf(hdr, sizeof(hdr), "%s > %s%s     ",
                     last.srcCall, last.dstCall, igTag);
        }
        _tft.setTextColor(COL_YELLOW, COL_CARD);
        _tft.drawString(hdr, 12, 134, 2);

        char bodyShort[44];
        strlcpy(bodyShort, last.comment, sizeof(bodyShort));
        _tft.fillRect(12, 152, 296, 16, COL_CARD);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawString(bodyShort, 12, 152, 2);
    } else {
        _tft.setTextColor(COL_MUTED, COL_CARD);
        _tft.drawString("Esperando paquetes AFSK 1200 bps...", 12, 136, 2);
        _tft.drawString("Toca MODO arriba para Tracker o Fija+IGate", 12, 152, 2);
    }
}

void UiManager::drawMonitor(bool fullRedraw) {
    _tft.setTextColor(COL_ACCENT, COL_BG);
    _tft.drawString("MONITOR DE TRAFICO AX.25 / IGATE (1200 bps)", 6, 26, 2);

    int count = Aprs.getPacketLogCount();
    if (count == 0) {
        _tft.setTextColor(COL_MUTED, COL_BG);
        _tft.drawCentreString("Sin paquetes recibidos aun.", 160, 95, 2);
        _tft.drawCentreString("Usa [LOOPBACK] en PANEL o conecta el handy.", 160, 118, 2);
        return;
    }

    int maxShow = (count > 4) ? 4 : count;
    for (int i = 0; i < maxShow; i++) {
        const DecodedPacket& p = Aprs.getPacketLog(i);
        int y = 46 + i * 40;

        _tft.fillRoundRect(4, y, 312, 37, 3, COL_CARD);
        _tft.drawRoundRect(4, y, 312, 37, 3, COL_BORDER);

        char line1[48];
        uint32_t ageSec = (millis() - p.timestampMs) / 1000;
        const char* igBadge = p.gatedToIs ? " [IGATE]" : "";
        if (p.hasPosition) {
            snprintf(line1, sizeof(line1), "%s [%.1fkm %s]%s %lus",
                     p.srcCall, p.distanceKm, AprsEngine::bearingToCardinal(p.bearingDeg),
                     igBadge, (unsigned long)ageSec);
        } else {
            snprintf(line1, sizeof(line1), "%s > %s%s (%lus)",
                     p.srcCall, p.dstCall, igBadge, (unsigned long)ageSec);
        }
        _tft.setTextColor(p.gatedToIs ? COL_GREEN : COL_YELLOW, COL_CARD);
        _tft.drawString(line1, 9, y + 3, 2);

        char line2[46];
        strlcpy(line2, strlen(p.comment) > 0 ? p.comment : p.infoText, sizeof(line2));
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawString(line2, 9, y + 19, 2);
    }
}

void UiManager::drawMessages(bool fullRedraw) {
    Aprs.clearUnreadMessages();

    // Caja superior de redacción rápida (y = 26..86)
    _tft.fillRoundRect(4, 26, 312, 60, 4, COL_CARD);
    _tft.drawRoundRect(4, 26, 312, 60, 4, COL_BORDER);

    // Campo PARA: [Indicativo]
    _tft.setTextColor(COL_MUTED, COL_CARD);
    _tft.drawString("Para:", 10, 31, 2);
    _tft.fillRoundRect(48, 29, 96, 20, 3, COL_BG);
    _tft.drawRoundRect(48, 29, 96, 20, 3, COL_ACCENT);
    _tft.setTextColor(COL_YELLOW, COL_BG);
    _tft.drawString(_composeTo, 54, 31, 2);

    // Botón [ENVIAR IS+RF / ENVIAR RF] y [LOOP]
    _tft.fillRoundRect(150, 29, 96, 22, 3, COL_GREEN);
    _tft.setTextColor(COL_BG, COL_GREEN);
    const char* sendLbl = Igate.isAprsIsConnected()
        ? (Aprs.config().igateBeaconMode == IGATE_BCN_IS_ONLY ? "ENVIAR IS" : "ENVIAR IS+RF")
        : "ENVIAR RF";
    _tft.drawCentreString(sendLbl, 198, 32, 2);

    _tft.fillRoundRect(252, 29, 58, 22, 3, COL_ACCENT);
    _tft.setTextColor(COL_BG, COL_ACCENT);
    _tft.drawCentreString("LOOP", 281, 32, 2);

    // Campo Texto del mensaje (tocar para editar con teclado)
    _tft.fillRoundRect(10, 55, 300, 26, 3, COL_BG);
    _tft.drawRoundRect(10, 55, 300, 26, 3, COL_BORDER);
    _tft.setTextColor(COL_WHITE, COL_BG);
    char msgPreview[40];
    strlcpy(msgPreview, _composeText, sizeof(msgPreview));
    _tft.drawString(msgPreview, 16, 60, 2);

    // Historial de mensajes recibidos/enviados (y = 90..208)
    int count = Aprs.getMessageCount();
    if (count == 0) {
        _tft.setTextColor(COL_MUTED, COL_BG);
        _tft.drawCentreString("Bandeja de mensajes APRS vacia.", 160, 138, 2);
        _tft.drawCentreString("Toca 'Para' o el texto arriba para escribir.", 160, 158, 2);
        return;
    }

    int maxShow = (count > 3) ? 3 : count;
    for (int i = 0; i < maxShow; i++) {
        const AprsMessageEntry& m = Aprs.getMessage(i);
        int y = 90 + i * 39;

        _tft.fillRoundRect(4, y, 312, 36, 3, COL_CARD);
        _tft.drawRoundRect(4, y, 312, 36, 3, COL_BORDER);

        char hdr[48];
        uint16_t badgeCol = COL_ACCENT;
        const char* statusStr = "RX";
        if (!m.incoming) {
            if (m.state == MSG_STATE_TX_ACK) {
                statusStr = m.sentViaIs ? "ACK OK (IS)" : "ACK OK";
                badgeCol = COL_GREEN;
            } else if (m.state == MSG_STATE_TX_WAIT) {
                statusStr = m.sentViaIs ? "IS->ESP ACK" : "ESP ACK";
                badgeCol = COL_ORANGE;
            } else {
                if (m.sentViaIs) {
                    statusStr = "ENVIADO IS (S/ACK)";
                    badgeCol = COL_ACCENT;
                } else {
                    statusStr = "SIN ACK";
                    badgeCol = COL_RED;
                }
            }
            snprintf(hdr, sizeof(hdr), "A %s [%s]", m.peerCall, statusStr);
        } else {
            badgeCol = COL_YELLOW;
            snprintf(hdr, sizeof(hdr), "DE %s [%s]", m.peerCall, m.sentViaIs ? "RX IGATE" : "RX RF");
        }

        _tft.setTextColor(badgeCol, COL_CARD);
        _tft.drawString(hdr, 10, y + 2, 2);

        char txtLine[44];
        strlcpy(txtLine, m.text, sizeof(txtLine));
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawString(txtLine, 10, y + 18, 2);
    }
}

void UiManager::drawStations(bool fullRedraw) {
    _tft.setTextColor(COL_ACCENT, COL_BG);
    _tft.drawString("ESTACIONES ESCUCHADAS (Toca para enviar MSJ)", 6, 26, 2);

    const StationEntry* list = Aprs.getStations();
    int drawn = 0;

    for (int i = 0; i < MAX_STATIONS_LIST && drawn < 4; i++) {
        if (!list[i].active) continue;
        int y = 46 + drawn * 40;

        _tft.fillRoundRect(4, y, 312, 37, 3, COL_CARD);
        _tft.drawRoundRect(4, y, 312, 37, 3, COL_BORDER);

        uint32_t ageSec = (millis() - list[i].lastHeardMs) / 1000;
        char line1[48];
        if (list[i].hasPos) {
            snprintf(line1, sizeof(line1), "%s  %.1f km (%d deg %s)  [%upkts]",
                     list[i].callsign, list[i].distanceKm, list[i].bearingDeg,
                     AprsEngine::bearingToCardinal(list[i].bearingDeg), list[i].pktCount);
        } else {
            snprintf(line1, sizeof(line1), "%s  (Sin pos) hace %lus [%upkts]",
                     list[i].callsign, (unsigned long)ageSec, list[i].pktCount);
        }

        _tft.setTextColor(COL_YELLOW, COL_CARD);
        _tft.drawString(line1, 10, y + 3, 2);

        char line2[46];
        if (list[i].hasPos) {
            snprintf(line2, sizeof(line2), "%.4f, %.4f | %s",
                     list[i].lat, list[i].lon, list[i].lastComment);
        } else {
            strlcpy(line2, list[i].lastComment, sizeof(line2));
        }
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawString(line2, 10, y + 19, 2);

        drawn++;
    }

    if (drawn == 0) {
        _tft.setTextColor(COL_MUTED, COL_BG);
        _tft.drawCentreString("Ninguna estacion registrada aun.", 160, 105, 2);
    }
}

void UiManager::drawConfig(bool fullRedraw) {
    AprsConfig& cfg = Aprs.config();

    static const char* const PAGE_TITLES[7] = {
        "CONF 1/7: MODO Y ESTACION",
        "CONF 2/7: RADIO Y BALIZA",
        "CONF 3/7: WIFI E IGATE",
        "CONF 4/7: SD Y WEB OTA",
        "CONF 5/7: SENSORES CLIMA",
        "CONF 6/7: SOS / EMERGENCIA",
        "CONF 7/7: CREDITOS Y VERSION"
    };

    // Encabezado con selector de página y botón GUARDAR
    _tft.setTextColor(COL_ACCENT, COL_BG);
    _tft.drawString(PAGE_TITLES[_cfgPage % 7], 6, 27, 2);

    char nextLbl[16];
    snprintf(nextLbl, sizeof(nextLbl), "PAG %u/7 >", (_cfgPage % 7) + 1);
    _tft.fillRoundRect(168, 23, 72, 26, 4, COL_HEADER);
    _tft.drawRoundRect(168, 23, 72, 26, 4, COL_BORDER);
    _tft.setTextColor(COL_WHITE, COL_HEADER);
    _tft.drawCentreString(nextLbl, 204, 28, 2);

    _tft.fillRoundRect(244, 23, 72, 26, 4, COL_GREEN);
    _tft.drawRoundRect(244, 23, 72, 26, 4, COL_WHITE);
    _tft.setTextColor(COL_BG, COL_GREEN);
    _tft.drawCentreString("GUARDAR", 280, 28, 2);

    if (_cfgPage == 0) {
        // Página 1: Rol del Equipo (Tracker vs Estación Fija + IGate vs Fija RF), Indicativo, Posición, Coordenadas
        // Fila 1 (y=52..86): Selector de Modo Operativo Principal
        uint16_t modeBg = (cfg.stationMode == STATION_MODE_FIXED_IGATE) ? 0x0320 : COL_CARD;
        _tft.fillRoundRect(6, 52, 308, 34, 3, modeBg);
        _tft.drawRoundRect(6, 52, 308, 34, 3, COL_ACCENT);
        const char* modeDesc = (cfg.stationMode == STATION_MODE_TRACKER)     ? "Modo: TRACKER MOVIL / PORTATIL (RF)" :
                               (cfg.stationMode == STATION_MODE_FIXED_IGATE) ? "Modo: ESTACION FIJA + IGATE (APRS-IS)" :
                                                                               "Modo: ESTACION FIJA SOLO RF / DIGI";
        _tft.setTextColor(COL_YELLOW, modeBg);
        _tft.drawCentreString(modeDesc, 160, 61, 2);

        // Fila 2 (y=91..125): Indicativo, SSID y Símbolo
        _tft.fillRoundRect(6, 91, 132, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 91, 132, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_MUTED, COL_CARD);
        _tft.drawString("Call:", 10, 100, 2);
        _tft.setTextColor(COL_YELLOW, COL_CARD);
        _tft.drawString(cfg.callsign, 46, 100, 2);

        _tft.fillRoundRect(142, 91, 74, 34, 3, COL_CARD);
        _tft.drawRoundRect(142, 91, 74, 34, 3, COL_BORDER);
        char ssidStr[16];
        snprintf(ssidStr, sizeof(ssidStr), "SSID:-%u", cfg.ssid);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString(ssidStr, 179, 100, 2);

        _tft.fillRoundRect(220, 91, 94, 34, 3, COL_CARD);
        _tft.drawRoundRect(220, 91, 94, 34, 3, COL_BORDER);
        const char* symDesc = (cfg.symbolCode == '&') ? "Simb:IGate" :
                              (cfg.symbolCode == '[') ? "Simb:Handy" :
                              (cfg.symbolCode == '>') ? "Simb:Auto" :
                              (cfg.symbolCode == '-') ? "Simb:Casa" : "Simb:Camion";
        _tft.setTextColor(COL_ACCENT, COL_CARD);
        _tft.drawCentreString(symDesc, 267, 100, 2);

        // Fila 3 (y=130..164): Fuente Posición (FIJA/GPS), Latitud y Longitud
        _tft.fillRoundRect(6, 130, 82, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 130, 82, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_ACCENT, COL_CARD);
        _tft.drawCentreString(cfg.posMode == POS_MODE_FIXED ? "Pos:FIJA" : "Pos:GPS", 47, 139, 2);

        char latStr[20], lonStr[20];
        snprintf(latStr, sizeof(latStr), "Lat:%.3f", cfg.fixedLat);
        snprintf(lonStr, sizeof(lonStr), "Lon:%.3f", cfg.fixedLon);

        _tft.fillRoundRect(92, 130, 108, 34, 3, COL_CARD);
        _tft.drawRoundRect(92, 130, 108, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString(latStr, 146, 139, 2);

        _tft.fillRoundRect(204, 130, 110, 34, 3, COL_CARD);
        _tft.drawRoundRect(204, 130, 110, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString(lonStr, 259, 139, 2);

        // Fila 4 (y=169..205): Comentario de Baliza
        _tft.fillRoundRect(6, 169, 308, 36, 3, COL_CARD);
        _tft.drawRoundRect(6, 169, 308, 36, 3, COL_BORDER);
        _tft.setTextColor(COL_MUTED, COL_CARD);
        _tft.drawString("Coment:", 12, 179, 2);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        char commShort[30];
        strlcpy(commShort, cfg.comment, sizeof(commShort));
        _tft.drawString(commShort, 72, 179, 2);

    } else if (_cfgPage == 1) {
        // Página 2: Modelo de Placa (ILI9341 vs ST7789), Pin de Audio TX, Preset Radio, Delay, Vol, Baliza, Digi
        // Fila 1 (y=52..86): Modelo de Placa (informativo) y Pin de Salida Audio TX (conector + IO26)
        _tft.fillRoundRect(6, 52, 146, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 52, 146, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_MUTED, COL_CARD);
        _tft.drawCentreString(cfg.boardModel == BOARD_MODEL_ST7789 ? "Placa: ST7789" : "Placa: ILI9341", 79, 61, 2);

        _tft.fillRoundRect(158, 52, 156, 34, 3, COL_CARD);
        _tft.drawRoundRect(158, 52, 156, 34, 3, COL_ACCENT);
        char txPinStr[28];
        if (cfg.txAudioPin == 26) {
            snprintf(txPinStr, sizeof(txPinStr), "TX Pin: IO26 (SPK)");
        } else {
            snprintf(txPinStr, sizeof(txPinStr), "TX Pin: IO%u (+IO26)", cfg.txAudioPin);
        }
        _tft.setTextColor(COL_GREEN, COL_CARD);
        _tft.drawCentreString(txPinStr, 236, 61, 2);

        // Fila 2 (y=91..125): Preset Radio (Baofeng/Yaesu), TX Delay y Volumen TX
        _tft.fillRoundRect(6, 91, 102, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 91, 102, 34, 3, COL_BORDER);
        const char* radShort = (cfg.radioType == RADIO_BAOFENG) ? "Rad:BAOFENG" :
                               (cfg.radioType == RADIO_YAESU)   ? "Rad:YAESU" : "Rad:CUSTOM";
        _tft.setTextColor(COL_YELLOW, COL_CARD);
        _tft.drawCentreString(radShort, 57, 100, 2);

        char dlyStr[20], volStr[20];
        snprintf(dlyStr, sizeof(dlyStr), "Dly:%ums", cfg.txDelayMs);
        snprintf(volStr, sizeof(volStr), "Vol:%u%%", cfg.txVolume);

        _tft.fillRoundRect(114, 91, 102, 34, 3, COL_CARD);
        _tft.drawRoundRect(114, 91, 102, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString(dlyStr, 165, 100, 2);

        _tft.fillRoundRect(222, 91, 92, 34, 3, COL_CARD);
        _tft.drawRoundRect(222, 91, 92, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString(volStr, 268, 100, 2);

        // Fila 3 (y=130..164): Intervalo de Baliza y Auto-ACK
        char intStr[28];
        snprintf(intStr, sizeof(intStr), "Baliza: cada %u s", cfg.beaconIntervalSec);
        _tft.fillRoundRect(6, 130, 150, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 130, 150, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_ACCENT, COL_CARD);
        _tft.drawCentreString(intStr, 81, 139, 2);

        _tft.fillRoundRect(164, 130, 150, 34, 3, COL_CARD);
        _tft.drawRoundRect(164, 130, 150, 34, 3, COL_BORDER);
        _tft.setTextColor(cfg.autoAckMessages ? COL_GREEN : COL_MUTED, COL_CARD);
        _tft.drawCentreString(cfg.autoAckMessages ? "Auto-ACK MSJ: SI" : "Auto-ACK MSJ: NO", 239, 139, 2);

        // Fila 4 (y=169..205): Fill-in Digipeater WIDE1-1
        _tft.fillRoundRect(6, 169, 308, 36, 3, COL_CARD);
        _tft.drawRoundRect(6, 169, 308, 36, 3, COL_BORDER);
        _tft.setTextColor(cfg.digipeatWide1 ? COL_ORANGE : COL_MUTED, COL_CARD);
        _tft.drawCentreString(cfg.digipeatWide1 ? "Repetidor Digipeater WIDE1-1: ACTIVO" : "Repetidor Digipeater WIDE1-1: APAGADO", 160, 179, 2);

    } else if (_cfgPage == 2) {
        // Página 3: Configuración de WiFi e IGate APRS-IS
        // Fila 1 (y=52..86): WiFi SSID y Contraseña WiFi
        _tft.fillRoundRect(6, 52, 168, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 52, 168, 34, 3, COL_BORDER);
        char ssidDisp[24];
        snprintf(ssidDisp, sizeof(ssidDisp), "WiFi:%s", strlen(cfg.wifiSsid) > 0 ? cfg.wifiSsid : "(Tocar)");
        _tft.setTextColor(COL_YELLOW, COL_CARD);
        _tft.drawString(ssidDisp, 12, 61, 2);

        _tft.fillRoundRect(180, 52, 134, 34, 3, COL_CARD);
        _tft.drawRoundRect(180, 52, 134, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString(strlen(cfg.wifiPass) > 0 ? "Clave: [****]" : "Clave: (Vacia)", 247, 61, 2);

        // Fila 2 (y=91..125): Servidor APRS-IS y Passcode
        _tft.fillRoundRect(6, 91, 188, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 91, 188, 34, 3, COL_BORDER);
        char srvDisp[28];
        snprintf(srvDisp, sizeof(srvDisp), "IS:%s", cfg.aprsIsServer);
        _tft.setTextColor(COL_ACCENT, COL_CARD);
        _tft.drawString(srvDisp, 12, 100, 2);

        _tft.fillRoundRect(200, 91, 114, 34, 3, COL_CARD);
        _tft.drawRoundRect(200, 91, 114, 34, 3, COL_BORDER);
        char passDisp[22];
        snprintf(passDisp, sizeof(passDisp), "Pass:%u%s",
                 Igate.getEffectivePasscode(), cfg.aprsIsPasscode < 0 ? "(A)" : "");
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString(passDisp, 257, 100, 2);

        // Fila 3 (y=130..164): Habilitar IGate + Modo de Baliza en IGate
        _tft.fillRoundRect(6, 130, 146, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 130, 146, 34, 3, COL_BORDER);
        bool igOn = (cfg.stationMode == STATION_MODE_FIXED_IGATE) || cfg.igateEnabled;
        _tft.setTextColor(igOn ? COL_GREEN : COL_MUTED, COL_CARD);
        _tft.drawCentreString(igOn ? "IGate RF->IS: SI" : "IGate RF->IS: NO", 79, 139, 2);

        _tft.fillRoundRect(158, 130, 156, 34, 3, COL_CARD);
        _tft.drawRoundRect(158, 130, 156, 34, 3, COL_BORDER);
        const char* bcnModeStr = (cfg.igateBeaconMode == IGATE_BCN_BOTH)    ? "Baliza: RF + IS" :
                                 (cfg.igateBeaconMode == IGATE_BCN_IS_ONLY) ? "Baliza: SOLO IS" :
                                                                              "Baliza: SOLO RF";
        _tft.setTextColor(COL_YELLOW, COL_CARD);
        _tft.drawCentreString(bcnModeStr, 236, 139, 2);

        // Fila 4 (y=169..205): Estado en vivo APRS-IS + Botón Reconectar
        _tft.fillRoundRect(6, 169, 222, 36, 3, COL_CARD);
        _tft.drawRoundRect(6, 169, 222, 36, 3, COL_BORDER);
        char stLine[36];
        snprintf(stLine, sizeof(stLine), "%s (%lu pkts)", Igate.getStateText(), (unsigned long)Igate.getRfToIsCount());
        _tft.setTextColor(Igate.isAprsIsVerified() ? COL_GREEN : COL_ORANGE, COL_CARD);
        _tft.drawString(stLine, 12, 179, 2);

        _tft.fillRoundRect(234, 169, 80, 36, 3, COL_ACCENT);
        _tft.setTextColor(COL_BG, COL_ACCENT);
        _tft.drawCentreString("CONECTAR", 274, 179, 2);

    } else if (_cfgPage == 3) {
        // Página 4: Actualización de Firmware por MicroSD y OTA (Web / HTTP)
        // Fila 1 (y=52..86): Estado MicroSD + Botón [ESCANEAR SD]
        _tft.fillRoundRect(6, 52, 212, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 52, 212, 34, 3, COL_BORDER);
        char sdShort[32];
        strlcpy(sdShort, Updater.getStatusMsg(), sizeof(sdShort));
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawString(sdShort, 12, 61, 2);

        _tft.fillRoundRect(224, 52, 90, 34, 3, COL_ACCENT);
        _tft.setTextColor(COL_BG, COL_ACCENT);
        _tft.drawCentreString("ESCANEAR SD", 269, 61, 2);

        // Fila 2 (y=91..125): Botón para instalar archivo .bin detectado en MicroSD
        if (Updater.getSdBinCount() > 0) {
            const SdBinEntry& b = Updater.getSdBin(0);
            char installLbl[46];
            snprintf(installLbl, sizeof(installLbl), "INSTALAR SD: %s (%luKB)",
                     b.path, (unsigned long)(b.sizeBytes / 1024UL));
            _tft.fillRoundRect(6, 91, 308, 34, 3, COL_GREEN);
            _tft.setTextColor(COL_BG, COL_GREEN);
            _tft.drawCentreString(installLbl, 160, 100, 2);
        } else {
            _tft.fillRoundRect(6, 91, 308, 34, 3, COL_CARD);
            _tft.drawRoundRect(6, 91, 308, 34, 3, COL_BORDER);
            _tft.setTextColor(COL_MUTED, COL_CARD);
            _tft.drawCentreString("SD: Copia /firmware.bin o /update.bin y escanea", 160, 100, 2);
        }

        // Fila 3 (y=130..164): URL OTA Remota + Botón [OTA NUBE]
        _tft.fillRoundRect(6, 130, 212, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 130, 212, 34, 3, COL_BORDER);
        char urlShort[28];
        if (strlen(cfg.otaServerUrl) > 0) {
            snprintf(urlShort, sizeof(urlShort), "URL:%s", cfg.otaServerUrl);
            _tft.setTextColor(COL_YELLOW, COL_CARD);
        } else {
            strlcpy(urlShort, "URL OTA: (Toca para editar)", sizeof(urlShort));
            _tft.setTextColor(COL_MUTED, COL_CARD);
        }
        _tft.drawString(urlShort, 12, 139, 2);

        _tft.fillRoundRect(224, 130, 90, 34, 3, COL_ORANGE);
        _tft.setTextColor(COL_BG, COL_ORANGE);
        _tft.drawCentreString("OTA URL", 269, 139, 2);

        // Fila 4 (y=169..205): Portal Web OTA Local (WiFi / Punto de Acceso CYD-APRS)
        uint16_t portalBg = Updater.isWebPortalRunning() ? COL_GREEN : COL_HEADER;
        _tft.fillRoundRect(6, 169, 308, 36, 3, portalBg);
        _tft.drawRoundRect(6, 169, 308, 36, 3, COL_ACCENT);
        _tft.setTextColor(Updater.isWebPortalRunning() ? COL_BG : COL_WHITE, portalBg);
        if (Updater.isWebPortalRunning()) {
            char portalTxt[46];
            snprintf(portalTxt, sizeof(portalTxt), "WEB OTA ACTIVO -> http://%s",
                     Updater.getPortalAddress().c_str());
            _tft.drawCentreString(portalTxt, 160, 179, 2);
        } else {
            _tft.drawCentreString("ACTIVAR PORTAL WEB / AP WIFI OTA (192.168.4.1)", 160, 179, 2);
        }

    } else if (_cfgPage == 4) {
        // Página 5: Configuración y Estado de Sensores Meteorológicos (BMP280 / BME280 / AHT20 / DHT22 / DHT11)
        const SensorReading& r = Sensors.reading();

        // Fila 1 (y=52..92): Tarjeta de Estado en Vivo del Sensor
        _tft.fillRoundRect(6, 52, 308, 40, 3, COL_CARD);
        _tft.drawRoundRect(6, 52, 308, 40, 3, r.connected ? COL_GREEN : COL_BORDER);
        _tft.setTextColor(r.connected ? COL_GREEN : COL_ORANGE, COL_CARD);
        _tft.drawString(r.statusText, 12, 55, 2);

        if (r.connected) {
            char valLine[48];
            if (r.hasHumidity && r.hasPressure) {
                snprintf(valLine, sizeof(valLine), "Temp: %.1f C | Hum: %.0f%% | %.1f hPa",
                         r.temperatureC, r.humidityPct, r.pressureHpa);
            } else if (r.hasPressure) {
                snprintf(valLine, sizeof(valLine), "Temp: %.1f C  |  Presion: %.1f hPa",
                         r.temperatureC, r.pressureHpa);
            } else {
                snprintf(valLine, sizeof(valLine), "Temp: %.1f C  |  Humedad: %.0f %%",
                         r.temperatureC, r.humidityPct);
            }
            _tft.setTextColor(COL_YELLOW, COL_CARD);
            _tft.drawString(valLine, 12, 73, 2);
        } else {
            _tft.setTextColor(COL_MUTED, COL_CARD);
            _tft.drawString("Sin sensor conectado (No informa en balizas)", 12, 73, 2);
        }

        // Fila 2 (y=97..131): Modelo de Sensor y Pines de Conexión (P3 IO16/17 o CN1)
        _tft.fillRoundRect(6, 97, 148, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 97, 148, 34, 3, COL_ACCENT);
        _tft.setTextColor(COL_YELLOW, COL_CARD);
        _tft.drawCentreString(Sensors.getModelConfigText(), 80, 106, 2);

        _tft.fillRoundRect(160, 97, 154, 34, 3, COL_CARD);
        _tft.drawRoundRect(160, 97, 154, 34, 3, COL_ACCENT);
        _tft.setTextColor(COL_ACCENT, COL_CARD);
        _tft.drawCentreString(Sensors.getPinProfileText(), 237, 106, 2);

        // Fila 3 (y=136..170): Modo de Inclusión en Baliza APRS + Calibración Offset Temp (- / +)
        _tft.fillRoundRect(6, 136, 174, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 136, 174, 34, 3, COL_BORDER);
        _tft.setTextColor(cfg.sensorBeaconMode == SENSOR_BCN_OFF ? COL_MUTED : COL_GREEN, COL_CARD);
        _tft.drawCentreString(Sensors.getBeaconModeText(), 93, 145, 2);

        char calStr[24];
        snprintf(calStr, sizeof(calStr), "Ofs:%+.1fC (-/+)", cfg.tempOffsetC);
        _tft.fillRoundRect(186, 136, 128, 34, 3, COL_CARD);
        _tft.drawRoundRect(186, 136, 128, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString(calStr, 250, 145, 2);

        // Fila 4 (y=175..205): Vista previa de Trama WX + Botón [DETECTAR]
        _tft.fillRoundRect(6, 175, 212, 30, 3, COL_CARD);
        _tft.drawRoundRect(6, 175, 212, 30, 3, COL_BORDER);
        char prevBuf[40];
        char bcnPrev[44];
        if (!r.connected || cfg.sensorBeaconMode == SENSOR_BCN_OFF) {
            strlcpy(bcnPrev, "Baliza: Normal (sin datos WX)", sizeof(bcnPrev));
            _tft.setTextColor(COL_MUTED, COL_CARD);
        } else if (cfg.sensorBeaconMode == SENSOR_BCN_WX_PACK && Sensors.formatAprsWxPayload(prevBuf, sizeof(prevBuf))) {
            snprintf(bcnPrev, sizeof(bcnPrev), "WX: /_%s", prevBuf);
            _tft.setTextColor(COL_ACCENT, COL_CARD);
        } else if (Sensors.formatCommentTelemetry(prevBuf, sizeof(prevBuf))) {
            snprintf(bcnPrev, sizeof(bcnPrev), "TX: [%s]", prevBuf);
            _tft.setTextColor(COL_WHITE, COL_CARD);
        }
        _tft.drawString(bcnPrev, 12, 182, 2);

        _tft.fillRoundRect(224, 175, 90, 30, 3, COL_ACCENT);
        _tft.setTextColor(COL_BG, COL_ACCENT);
        _tft.drawCentreString("DETECTAR", 269, 182, 2);

    } else if (_cfgPage == 5) {
        // Página 6: Parámetros del Aviso de Emergencia (SOS) Exclusivo Tracker
        // Fila 1 (y=52..86): Intervalo de Baliza SOS (60s / 120s / 180s)
        _tft.fillRoundRect(6, 52, 308, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 52, 308, 34, 3, COL_BORDER);
        char intBuf[36];
        snprintf(intBuf, sizeof(intBuf), "Intervalo Baliza SOS: %u s", cfg.emergIntervalSec);
        _tft.setTextColor(COL_YELLOW, COL_CARD);
        _tft.drawCentreString(intBuf, 160, 61, 2);

        // Fila 2 (y=91..125): Mensaje Directo APRS SOS (SI / NO)
        _tft.fillRoundRect(6, 91, 308, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 91, 308, 34, 3, COL_BORDER);
        _tft.setTextColor(cfg.emergSendDirectMsg ? COL_GREEN : COL_MUTED, COL_CARD);
        _tft.drawCentreString(cfg.emergSendDirectMsg ? "Mensaje APRS Directo: SI" : "Mensaje APRS Directo: NO", 160, 100, 2);

        // Fila 3 (y=130..164): Indicativo Destino del Mensaje Directo
        _tft.fillRoundRect(6, 130, 308, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 130, 308, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_MUTED, COL_CARD);
        _tft.drawString("Destinatario:", 12, 139, 2);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawString(strlen(cfg.emergMsgDest) > 0 ? cfg.emergMsgDest : "(Toca para escribir)", 116, 139, 2);

        // Fila 4 (y=169..205): Alerta Sonora Parlante Onboard (IO26)
        _tft.fillRoundRect(6, 169, 308, 36, 3, COL_CARD);
        _tft.drawRoundRect(6, 169, 308, 36, 3, COL_BORDER);
        _tft.setTextColor(cfg.emergSoundAlert ? COL_ORANGE : COL_MUTED, COL_CARD);
        _tft.drawCentreString(cfg.emergSoundAlert ? "Alerta Sonora Parlante (IO26): SI" : "Alerta Sonora Parlante (IO26): NO", 160, 179, 2);

    } else {
        // Página 7 (Siempre la última): Créditos del Proyecto y Versión Instalada (About)
        // Tarjeta 1 (y=52..92): Autor y Créditos
        _tft.fillRoundRect(6, 52, 308, 40, 4, COL_CARD);
        _tft.drawRoundRect(6, 52, 308, 40, 4, COL_ACCENT);
        _tft.setTextColor(COL_YELLOW, COL_CARD);
        _tft.drawCentreString("ESP32 CYD APRS — Proyecto de LU4HNE", 160, 56, 2);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString("Transceptor AFSK 1200 / Tracker / IGate APRS-IS", 160, 73, 2);

        // Tarjeta 2 (y=96..136): Versión de Firmware y Compilación
        _tft.fillRoundRect(6, 96, 308, 40, 4, COL_CARD);
        _tft.drawRoundRect(6, 96, 308, 40, 4, COL_BORDER);
        char verLine[48];
        snprintf(verLine, sizeof(verLine), "Version Instalada: v%s (%s)", FIRMWARE_VERSION, __DATE__);
        _tft.setTextColor(COL_GREEN, COL_CARD);
        _tft.drawCentreString(verLine, 160, 100, 2);

        char hwLine[48];
        snprintf(hwLine, sizeof(hwLine), "Driver: %s | Touch: %s | TX: IO%u+26",
                 cfg.boardModel == BOARD_MODEL_ST7789 ? "ST7789" : "ILI9341",
                 _touch.isCapacitive() ? "CST820" : "XPT2046",
                 cfg.txAudioPin);
        _tft.setTextColor(COL_ACCENT, COL_CARD);
        _tft.drawCentreString(hwLine, 160, 117, 2);

        // Tarjeta 3 (y=140..174): Red, IP y Memoria Libre
        _tft.fillRoundRect(6, 140, 308, 34, 4, COL_CARD);
        _tft.drawRoundRect(6, 140, 308, 34, 4, COL_BORDER);
        char netLine[48];
        snprintf(netLine, sizeof(netLine), "IP: %s | Pass:%u | RAM:%luKB",
                 Igate.getLocalIpString().c_str(),
                 Igate.getEffectivePasscode(),
                 (unsigned long)(ESP.getFreeHeap() / 1024UL));
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString(netLine, 160, 149, 2);

        // Fila 4 (y=178..206): Control de Brillo LED Azul, Brillo Pantalla TFT y Reinicio
        const char* ledLbl = (cfg.ledBrightness == 0) ? "LED: OFF" :
                             (cfg.ledBrightness == 1) ? "LED: TENUE" : "LED: MEDIO";
        _tft.fillRoundRect(6, 178, 98, 28, 4, COL_CARD);
        _tft.drawRoundRect(6, 178, 98, 28, 4, COL_ACCENT);
        _tft.setTextColor(COL_ACCENT, COL_CARD);
        _tft.drawCentreString(ledLbl, 55, 184, 2);

        char tftLbl[20];
        snprintf(tftLbl, sizeof(tftLbl), "TFT: %u%%", cfg.tftBrightness);
        _tft.fillRoundRect(110, 178, 98, 28, 4, COL_CARD);
        _tft.drawRoundRect(110, 178, 98, 28, 4, COL_YELLOW);
        _tft.setTextColor(COL_YELLOW, COL_CARD);
        _tft.drawCentreString(tftLbl, 159, 184, 2);

        _tft.fillRoundRect(214, 178, 100, 28, 4, COL_HEADER);
        _tft.drawRoundRect(214, 178, 100, 28, 4, COL_ORANGE);
        _tft.setTextColor(COL_ORANGE, COL_HEADER);
        _tft.drawCentreString("REINICIAR", 264, 184, 2);
    }
}

void UiManager::openKeyboard(KeyboardTarget target, const char* title, const char* initialVal, uint8_t maxLen) {
    _kbTarget = target;
    strlcpy(_kbTitle, title, sizeof(_kbTitle));
    strlcpy(_kbBuffer, initialVal ? initialVal : "", sizeof(_kbBuffer));
    _kbMaxLen = (maxLen > 94) ? 94 : maxLen;
    _kbShift = (target != KB_CFG_WIFI_PASS && target != KB_CFG_APRS_SERVER && target != KB_CFG_OTA_URL);
    _kbSymbols = false;
    _needsFullRedraw = true;
}

void UiManager::drawKeyboard() {
    _tft.fillScreen(COL_BG);

    // Barra superior de título y botones [ESC] / [OK]
    _tft.fillRect(0, 0, 320, 28, COL_HEADER);
    _tft.setTextColor(COL_ACCENT, COL_HEADER);
    _tft.drawString(_kbTitle, 8, 5, 2);

    _tft.fillRoundRect(204, 2, 52, 24, 3, COL_RED);
    _tft.setTextColor(COL_WHITE, COL_RED);
    _tft.drawCentreString("ESC", 230, 6, 2);

    _tft.fillRoundRect(260, 2, 56, 24, 3, COL_GREEN);
    _tft.drawRoundRect(260, 2, 56, 24, 3, COL_WHITE);
    _tft.setTextColor(COL_BG, COL_GREEN);
    _tft.drawCentreString("OK", 288, 6, 2);

    // Caja de texto actual
    _tft.fillRoundRect(6, 30, 308, 28, 3, COL_CARD);
    _tft.drawRoundRect(6, 30, 308, 28, 3, COL_ACCENT);
    _tft.setTextColor(COL_YELLOW, COL_CARD);

    char dispBuf[42];
    size_t len = strlen(_kbBuffer);
    if (len > 36) {
        strlcpy(dispBuf, &_kbBuffer[len - 36], sizeof(dispBuf));
    } else {
        strlcpy(dispBuf, _kbBuffer, sizeof(dispBuf));
    }
    strlcat(dispBuf, "_", sizeof(dispBuf));
    _tft.drawString(dispBuf, 12, 36, 2);

    // Filas del teclado QWERTY + Números + Símbolos (para claves WiFi y URLs)
    static const char* row0 = "1234567890";
    static const char* row1U = "QWERTYUIOP";
    static const char* row1L = "qwertyuiop";
    static const char* row2U = "ASDFGHJKL-";
    static const char* row2L = "asdfghjkl-";
    static const char* row3U = "ZXCVBNM./:";
    static const char* row3L = "zxcvbnm./:";

    static const char* row1S = "!@#$%^&*()";
    static const char* row2S = "_+=[]{};'\"";
    static const char* row3S = "?<>\\|,~`./";

    const char* rows[4] = {
        row0,
        _kbSymbols ? row1S : (_kbShift ? row1U : row1L),
        _kbSymbols ? row2S : (_kbShift ? row2U : row2L),
        _kbSymbols ? row3S : (_kbShift ? row3U : row3L)
    };

    for (int r = 0; r < 4; r++) {
        int y = 64 + r * 34;
        for (int c = 0; c < 10; c++) {
            int x = 4 + c * 31;
            _tft.fillRoundRect(x, y, 29, 31, 3, COL_CARD);
            _tft.drawRoundRect(x, y, 29, 31, 3, COL_BORDER);
            char keyStr[2] = { rows[r][c], '\0' };
            _tft.setTextColor(COL_WHITE, COL_CARD);
            _tft.drawCentreString(keyStr, x + 14, y + 7, 2);
        }
    }

    // Fila inferior: [MAY/min] [SIMB] [ESPACIO] [BORRAR]
    int yBot = 202;
    _tft.fillRoundRect(4, yBot, 58, 34, 3, _kbShift ? COL_ACCENT : COL_HEADER);
    _tft.setTextColor(_kbShift ? COL_BG : COL_WHITE, _kbShift ? COL_ACCENT : COL_HEADER);
    _tft.drawCentreString("MAY/m", 33, yBot + 9, 2);

    _tft.fillRoundRect(66, yBot, 52, 34, 3, _kbSymbols ? COL_YELLOW : COL_HEADER);
    _tft.setTextColor(_kbSymbols ? COL_BG : COL_WHITE, _kbSymbols ? COL_YELLOW : COL_HEADER);
    _tft.drawCentreString("#?=", 92, yBot + 9, 2);

    _tft.fillRoundRect(122, yBot, 114, 34, 3, COL_CARD);
    _tft.drawRoundRect(122, yBot, 114, 34, 3, COL_BORDER);
    _tft.setTextColor(COL_WHITE, COL_CARD);
    _tft.drawCentreString("ESPACIO", 179, yBot + 9, 2);

    _tft.fillRoundRect(240, yBot, 74, 34, 3, COL_ORANGE);
    _tft.setTextColor(COL_BG, COL_ORANGE);
    _tft.drawCentreString("<- BORR", 277, yBot + 9, 2);
}

void UiManager::handleKeyboardTouch(uint16_t tx, uint16_t ty) {
    // Botones superiores ESC y OK (zona superior hasta y <= 35)
    if (ty <= 35) {
        if (tx >= 195 && tx <= 255) {
            _kbTarget = KB_NONE;
            _needsFullRedraw = true;
            return;
        }
        if (tx >= 256) {
            commitKeyboardValue();
            _kbTarget = KB_NONE;
            _needsFullRedraw = true;
            return;
        }
        return;
    }

    // Teclas de caracteres (4 filas x 10 columnas)
    if (ty >= 64 && ty < 200) {
        int r = (ty - 64) / 34;
        int c = (tx - 4) / 31;
        if (r >= 0 && r < 4 && c >= 0 && c < 10) {
            static const char* row0 = "1234567890";
            static const char* row1U = "QWERTYUIOP";
            static const char* row1L = "qwertyuiop";
            static const char* row2U = "ASDFGHJKL-";
            static const char* row2L = "asdfghjkl-";
            static const char* row3U = "ZXCVBNM./:";
            static const char* row3L = "zxcvbnm./:";
            static const char* row1S = "!@#$%^&*()";
            static const char* row2S = "_+=[]{};'\"";
            static const char* row3S = "?<>\\|,~`./";

            const char* rows[4] = {
                row0,
                _kbSymbols ? row1S : (_kbShift ? row1U : row1L),
                _kbSymbols ? row2S : (_kbShift ? row2U : row2L),
                _kbSymbols ? row3S : (_kbShift ? row3U : row3L)
            };
            size_t len = strlen(_kbBuffer);
            if (len < _kbMaxLen) {
                _kbBuffer[len] = rows[r][c];
                _kbBuffer[len + 1] = '\0';
                _needsFullRedraw = true;
            }
        }
        return;
    }

    // Fila inferior (y >= 200)
    if (ty >= 200) {
        if (tx < 64) {
            _kbShift = !_kbShift;
            _kbSymbols = false;
            _needsFullRedraw = true;
        } else if (tx >= 64 && tx < 120) {
            _kbSymbols = !_kbSymbols;
            _needsFullRedraw = true;
        } else if (tx >= 120 && tx < 238) {
            size_t len = strlen(_kbBuffer);
            if (len < _kbMaxLen) {
                _kbBuffer[len] = ' ';
                _kbBuffer[len + 1] = '\0';
                _needsFullRedraw = true;
            }
        } else if (tx >= 238) {
            size_t len = strlen(_kbBuffer);
            if (len > 0) {
                _kbBuffer[len - 1] = '\0';
                _needsFullRedraw = true;
            }
        }
    }
}

void UiManager::commitKeyboardValue() {
    AprsConfig& cfg = Aprs.config();
    bool isCfg = false;
    switch (_kbTarget) {
        case KB_MSG_TO:
            strlcpy(_composeTo, _kbBuffer, sizeof(_composeTo));
            for (int i = 0; _composeTo[i]; i++) _composeTo[i] = (char)toupper((unsigned char)_composeTo[i]);
            break;
        case KB_MSG_TEXT:
            strlcpy(_composeText, _kbBuffer, sizeof(_composeText));
            break;
        case KB_CFG_CALLSIGN:
            strlcpy(cfg.callsign, _kbBuffer, sizeof(cfg.callsign));
            for (int i = 0; cfg.callsign[i]; i++) cfg.callsign[i] = (char)toupper((unsigned char)cfg.callsign[i]);
            isCfg = true;
            break;
        case KB_CFG_COMMENT:
            strlcpy(cfg.comment, _kbBuffer, sizeof(cfg.comment));
            isCfg = true;
            break;
        case KB_CFG_LAT:
            cfg.fixedLat = atof(_kbBuffer);
            isCfg = true;
            break;
        case KB_CFG_LON:
            cfg.fixedLon = atof(_kbBuffer);
            isCfg = true;
            break;
        case KB_CFG_WIFI_SSID:
            strlcpy(cfg.wifiSsid, _kbBuffer, sizeof(cfg.wifiSsid));
            isCfg = true;
            break;
        case KB_CFG_WIFI_PASS:
            strlcpy(cfg.wifiPass, _kbBuffer, sizeof(cfg.wifiPass));
            isCfg = true;
            break;
        case KB_CFG_APRS_SERVER:
            strlcpy(cfg.aprsIsServer, _kbBuffer, sizeof(cfg.aprsIsServer));
            isCfg = true;
            break;
        case KB_CFG_APRS_PASSCODE:
            cfg.aprsIsPasscode = atoi(_kbBuffer);
            isCfg = true;
            break;
        case KB_CFG_OTA_URL:
            strlcpy(cfg.otaServerUrl, _kbBuffer, sizeof(cfg.otaServerUrl));
            isCfg = true;
            break;
        case KB_CFG_EMERG_DEST:
            strlcpy(cfg.emergMsgDest, _kbBuffer, sizeof(cfg.emergMsgDest));
            for (int i = 0; cfg.emergMsgDest[i]; i++) cfg.emergMsgDest[i] = (char)toupper((unsigned char)cfg.emergMsgDest[i]);
            isCfg = true;
            break;
        default:
            break;
    }
    if (isCfg) {
        Aprs.saveConfig();
        showToast("Config guardada!", COL_GREEN);
    }
}

void UiManager::handleTouch(uint16_t tx, uint16_t ty) {
    if (_showEmergModal) {
        handleEmergConfirmTouch(tx, ty);
        return;
    }
    if (_showCancelModal) {
        handleEmergCancelTouch(tx, ty);
        return;
    }
    if (_kbTarget != KB_NONE) {
        handleKeyboardTouch(tx, ty);
        return;
    }

    // Barra de pestañas inferior (y >= 210)
    if (ty >= 210) {
        int tabIdx = tx / (320 / 5);
        if (tabIdx > 4) tabIdx = 4;
        if (tabIdx >= 0 && tabIdx <= 4) {
            if (_currentTab == TAB_CONFIG && (UiTab)tabIdx != TAB_CONFIG) {
                Aprs.saveConfig();
            }
            _currentTab = (UiTab)tabIdx;
            if (_currentTab == TAB_MESSAGES) {
                Aprs.clearUnreadMessages();
            }
            _needsFullRedraw = true;
        }
        return;
    }

    switch (_currentTab) {
        case TAB_DASHBOARD: handleDashboardTouch(tx, ty); break;
        case TAB_MONITOR:   handleMonitorTouch(tx, ty);   break;
        case TAB_MESSAGES:  handleMessagesTouch(tx, ty);  break;
        case TAB_STATIONS:  handleStationsTouch(tx, ty);  break;
        case TAB_CONFIG:    handleConfigTouch(tx, ty);    break;
    }
}

void UiManager::handleDashboardTouch(uint16_t tx, uint16_t ty) {
    if (Aprs.isEmergencyActive()) {
        if (ty >= 166 && ty <= 212) {
            _showCancelModal = true;
            _needsFullRedraw = true;
        }
        return;
    }

    // Tocar Tarjeta 1 (arriba a la izquierda) permite alternar rápido entre Tracker y Estación Fija + IGate
    if (ty >= 28 && ty <= 94 && tx >= 6 && tx <= 204) {
        AprsConfig& cfg = Aprs.config();
        StationMode nextMode = (StationMode)((cfg.stationMode + 1) % 3);
        Aprs.applyStationMode(nextMode);
        Aprs.saveConfig();
        Igate.reconnectNow();
        _needsFullRedraw = true;
        return;
    }

    AprsConfig& cfg = Aprs.config();
    if (ty >= 174 && ty <= 208) {
        if (cfg.stationMode == STATION_MODE_TRACKER) {
            if (tx >= 4 && tx <= 78) {
                // [BALIZA]
                showToast("Enviando Baliza APRS...", COL_RED);
                Aprs.sendPositionBeacon(false);
                _needsFullRedraw = true;
            } else if (tx >= 82 && tx <= 156) {
                // [LOOP]
                showToast("Ejecutando Test Loopback AX.25...", COL_ACCENT);
                Aprs.sendPositionBeacon(true);
                delay(80);
                Aprs.update();
                _needsFullRedraw = true;
            } else if (tx >= 160 && tx <= 234) {
                // [AUTO]
                cfg.autoBeacon = !cfg.autoBeacon;
                Aprs.saveConfig();
                _needsFullRedraw = true;
            } else if (tx >= 238) {
                // [SOS]
                _showEmergModal = true;
                _emergConfirmStep = 0;
                _emergModalTimerMs = millis();
                _selectedEmergType = EMERG_TYPE_SOS;
                _needsFullRedraw = true;
            }
        } else {
            if (tx >= 6 && tx <= 112) {
                // [TX BALIZA] por RF y/o APRS-IS
                showToast("Enviando Baliza APRS...", COL_RED);
                Aprs.sendPositionBeacon(false);
                _needsFullRedraw = true;
            } else if (tx >= 117 && tx <= 219) {
                // [LOOPBACK] codifica y decodifica internamente + DAC
                showToast("Ejecutando Test Loopback AX.25...", COL_ACCENT);
                Aprs.sendPositionBeacon(true);
                delay(80);
                Aprs.update();
                _needsFullRedraw = true;
            } else if (tx >= 224) {
                // [AUTO BCN ON/OFF]
                cfg.autoBeacon = !cfg.autoBeacon;
                Aprs.saveConfig();
                _needsFullRedraw = true;
            }
        }
    }
}

void UiManager::handleMonitorTouch(uint16_t tx, uint16_t ty) {
    // Tocar un paquete en el monitor copia el indicativo de origen para enviarle mensaje
    if (ty >= 46 && ty < 206) {
        int idx = (ty - 46) / 40;
        if (idx >= 0 && idx < Aprs.getPacketLogCount() && idx < 4) {
            const DecodedPacket& p = Aprs.getPacketLog(idx);
            strlcpy(_composeTo, p.srcCall, sizeof(_composeTo));
            _currentTab = TAB_MESSAGES;
            _needsFullRedraw = true;
        }
    }
}

void UiManager::handleMessagesTouch(uint16_t tx, uint16_t ty) {
    // Tocar campo "Para:" (x=4..146, y=26..52)
    if (ty >= 26 && ty <= 52) {
        if (tx < 146) {
            openKeyboard(KB_MSG_TO, "INDICATIVO DESTINO:", _composeTo, 9);
            return;
        }
        if (tx >= 150 && tx <= 246) {
            // [ENVIAR RF]
            showToast("Enviando Mensaje APRS...", COL_RED);
            Aprs.sendTextMessage(_composeTo, _composeText, false);
            _needsFullRedraw = true;
            return;
        }
        if (tx >= 250) {
            // [LOOP] prueba de mensaje en loopback
            showToast("Probando Mensaje APRS en Loopback...", COL_ACCENT);
            Aprs.sendTextMessage(_composeTo, _composeText, true);
            delay(80);
            Aprs.update();
            _needsFullRedraw = true;
            return;
        }
    }

    // Tocar campo de texto del mensaje (y=54..86)
    if (ty >= 54 && ty <= 86) {
        openKeyboard(KB_MSG_TEXT, "TEXTO DEL MENSAJE APRS:", _composeText, 64);
        return;
    }

    // Tocar un mensaje del historial copia el indicativo para responder rápido
    if (ty >= 90 && ty < 208) {
        int idx = (ty - 90) / 39;
        if (idx >= 0 && idx < Aprs.getMessageCount() && idx < 3) {
            strlcpy(_composeTo, Aprs.getMessage(idx).peerCall, sizeof(_composeTo));
            _needsFullRedraw = true;
        }
    }
}

void UiManager::handleStationsTouch(uint16_t tx, uint16_t ty) {
    if (ty >= 46 && ty < 206) {
        int clickedRow = (ty - 46) / 40;
        const StationEntry* list = Aprs.getStations();
        int drawn = 0;
        for (int i = 0; i < MAX_STATIONS_LIST; i++) {
            if (!list[i].active) continue;
            if (drawn == clickedRow) {
                strlcpy(_composeTo, list[i].callsign, sizeof(_composeTo));
                _currentTab = TAB_MESSAGES;
                _needsFullRedraw = true;
                return;
            }
            drawn++;
        }
    }
}

void UiManager::handleConfigTouch(uint16_t tx, uint16_t ty) {
    AprsConfig& cfg = Aprs.config();

    // Barra superior de Config: Título, [PAG x/7] y [GUARDAR] (y <= 52)
    if (ty <= 52) {
        if (tx >= 240) {
            // [GUARDAR] - Persistir absolutamente toda la configuración global en Flash NVS
            Aprs.saveConfig();
            Igate.reconnectNow();
            showToast("Config guardada en Flash NVS!", COL_GREEN);
            _needsFullRedraw = true;
            return;
        }
        if (tx >= 165 && tx < 240) {
            // [PAG x/7 >] - Auto-guardar lo modificado y pasar de pantalla
            Aprs.saveConfig();
            _cfgPage = (_cfgPage + 1) % 7;
            _needsFullRedraw = true;
            return;
        }
        return; // Toques en el título (tx < 165) no hacen nada (previene falsos toques a la fila 1)
    }

    if (_cfgPage == 0) {
        // Fila 1 (y=55..88): Selector de Modo Operativo (Tracker / Estación Fija + IGate / Fija RF)
        if (ty >= 55 && ty <= 88) {
            StationMode nextMode = (StationMode)((cfg.stationMode + 1) % 3);
            Aprs.applyStationMode(nextMode);
            Aprs.saveConfig();
            applyDisplayHardware();
            _needsFullRedraw = true;
            return;
        }
        // Fila 2 (y=91..125): Indicativo / SSID / Símbolo
        if (ty >= 91 && ty <= 125) {
            if (tx < 140) {
                openKeyboard(KB_CFG_CALLSIGN, "TU INDICATIVO (SIN SSID):", cfg.callsign, 6);
            } else if (tx < 218) {
                cfg.ssid = (cfg.ssid + 1) % 16;
                Aprs.saveConfig();
                _needsFullRedraw = true;
            } else {
                if (cfg.symbolCode == '[')      { cfg.symbolTable = '/'; cfg.symbolCode = '>'; }
                else if (cfg.symbolCode == '>') { cfg.symbolTable = 'I'; cfg.symbolCode = '&'; }
                else if (cfg.symbolCode == '&') { cfg.symbolTable = '/'; cfg.symbolCode = '-'; }
                else if (cfg.symbolCode == '-') { cfg.symbolTable = '/'; cfg.symbolCode = 'k'; }
                else                            { cfg.symbolTable = '/'; cfg.symbolCode = '['; }
                Aprs.saveConfig();
                _needsFullRedraw = true;
            }
            return;
        }
        // Fila 3 (y=130..164): Posición (FIJA/GPS) / Latitud / Longitud
        if (ty >= 130 && ty <= 164) {
            if (tx < 90) {
                cfg.posMode = (cfg.posMode == POS_MODE_FIXED) ? POS_MODE_GPS : POS_MODE_FIXED;
                Aprs.saveConfig();
                applyDisplayHardware();
                _needsFullRedraw = true;
                return;
            }
            char numBuf[20];
            if (tx < 202) {
                snprintf(numBuf, sizeof(numBuf), "%.4f", cfg.fixedLat);
                openKeyboard(KB_CFG_LAT, "LATITUD DECIMAL (Sur -):", numBuf, 12);
            } else {
                snprintf(numBuf, sizeof(numBuf), "%.4f", cfg.fixedLon);
                openKeyboard(KB_CFG_LON, "LONGITUD DECIMAL (Oeste -):", numBuf, 12);
            }
            return;
        }
        // Fila 4 (y=169..205): Comentario
        if (ty >= 169 && ty <= 205) {
            openKeyboard(KB_CFG_COMMENT, "COMENTARIO DE BALIZA:", cfg.comment, 40);
            return;
        }

    } else if (_cfgPage == 1) {
        // Página 2: Modelo de Placa / Pin TX / Radio Preset / TX Delay / TX Volume / Intervalo / Auto-ACK / Digi
        if (ty >= 55 && ty <= 88) {
            // "Placa: ILI9341 / ST7789" es informativo para evitar cambiar de driver por error táctil.
            // Para alternar perfil de placa se usa el botón físico BOOT (2 seg) o comando 'd' por USB.
            if (tx >= 150) {
                // Ciclar Pin Audio TX según los pines físicos disponibles en cada placa:
                if (cfg.boardModel == BOARD_MODEL_ILI9341) {
                    // ILI9341: IO27 (CN1) <-> IO26 (SPK) <-> IO21 (P3)
                    if (cfg.txAudioPin == 27) cfg.txAudioPin = 26;
                    else if (cfg.txAudioPin == 26) cfg.txAudioPin = 21;
                    else cfg.txAudioPin = 27;
                } else {
                    // ST7789: IO21 (CN1) <-> IO4 (P3) <-> IO17 (P3) <-> IO26 (SPK)
                    if (cfg.txAudioPin == 21) cfg.txAudioPin = 4;
                    else if (cfg.txAudioPin == 4) cfg.txAudioPin = 17;
                    else if (cfg.txAudioPin == 17) cfg.txAudioPin = 26;
                    else cfg.txAudioPin = 21;
                }

                Modem.configure(cfg.txDelayMs, cfg.txTailMs, cfg.txVolume, cfg.txAudioPin);
                Aprs.saveConfig();
                char pinToast[32];
                snprintf(pinToast, sizeof(pinToast), "Audio TX en IO%u", cfg.txAudioPin);
                showToast(pinToast, COL_GREEN);
            }
            _needsFullRedraw = true;
            return;
        }
        if (ty >= 91 && ty <= 125) {
            if (tx < 110) {
                RadioType nextType = (cfg.radioType == RADIO_BAOFENG) ? RADIO_YAESU : RADIO_BAOFENG;
                Aprs.applyRadioPreset(nextType);
                Aprs.saveConfig();
            } else if (tx < 218) {
                cfg.txDelayMs += 100;
                if (cfg.txDelayMs > 750) cfg.txDelayMs = 200;
                cfg.radioType = RADIO_CUSTOM;
                Modem.configure(cfg.txDelayMs, cfg.txTailMs, cfg.txVolume, cfg.txAudioPin);
                Aprs.saveConfig();
            } else {
                cfg.txVolume += 15;
                if (cfg.txVolume > 100) cfg.txVolume = 25;
                cfg.radioType = RADIO_CUSTOM;
                Modem.configure(cfg.txDelayMs, cfg.txTailMs, cfg.txVolume, cfg.txAudioPin);
                Aprs.saveConfig();
            }
            _needsFullRedraw = true;
            return;
        }
        if (ty >= 130 && ty <= 164) {
            if (tx < 160) {
                if (cfg.beaconIntervalSec < 120) cfg.beaconIntervalSec = 120;
                else if (cfg.beaconIntervalSec < 300) cfg.beaconIntervalSec = 300;
                else if (cfg.beaconIntervalSec < 600) cfg.beaconIntervalSec = 600;
                else if (cfg.beaconIntervalSec < 900) cfg.beaconIntervalSec = 900;
                else cfg.beaconIntervalSec = 60;
            } else {
                cfg.autoAckMessages = !cfg.autoAckMessages;
            }
            Aprs.saveConfig();
            _needsFullRedraw = true;
            return;
        }
        if (ty >= 169 && ty <= 205) {
            cfg.digipeatWide1 = !cfg.digipeatWide1;
            Aprs.saveConfig();
            _needsFullRedraw = true;
            return;
        }

    } else if (_cfgPage == 2) {
        // Página 3: WiFi e IGate APRS-IS
        if (ty >= 55 && ty <= 88) {
            if (tx < 176) {
                openKeyboard(KB_CFG_WIFI_SSID, "NOMBRE RED WIFI (SSID):", cfg.wifiSsid, 32);
            } else {
                openKeyboard(KB_CFG_WIFI_PASS, "CONTRASENA WIFI:", cfg.wifiPass, 63);
            }
            return;
        }
        if (ty >= 91 && ty <= 125) {
            if (tx < 196) {
                openKeyboard(KB_CFG_APRS_SERVER, "SERVIDOR APRS-IS:", cfg.aprsIsServer, 44);
            } else {
                char passBuf[16];
                snprintf(passBuf, sizeof(passBuf), "%ld", (long)cfg.aprsIsPasscode);
                openKeyboard(KB_CFG_APRS_PASSCODE, "PASSCODE (-1 = AUTO):", passBuf, 8);
            }
            return;
        }
        if (ty >= 130 && ty <= 164) {
            if (tx < 154) {
                cfg.igateEnabled = !cfg.igateEnabled;
                if (cfg.igateEnabled) cfg.stationMode = STATION_MODE_FIXED_IGATE;
            } else {
                cfg.igateBeaconMode = (cfg.igateBeaconMode + 1) % 3;
            }
            Aprs.saveConfig();
            _needsFullRedraw = true;
            return;
        }
        if (ty >= 169 && ty <= 205) {
            Aprs.saveConfig();
            showToast("Conectando WiFi e IGate APRS-IS...", COL_ACCENT);
            Igate.reconnectNow();
            _needsFullRedraw = true;
            return;
        }

    } else if (_cfgPage == 3) {
        // Página 4: Actualización por MicroSD y Web OTA
        if (ty >= 55 && ty <= 88) {
            showToast("Escaneando tarjeta MicroSD...", COL_ACCENT);
            Updater.scanSdBinaries();
            _needsFullRedraw = true;
            return;
        }
        if (ty >= 91 && ty <= 125) {
            if (Updater.getSdBinCount() > 0) {
                Updater.installSdBinary(Updater.getSdBin(0).path, _tft);
                _needsFullRedraw = true;
            } else {
                Updater.scanSdBinaries();
                _needsFullRedraw = true;
            }
            return;
        }
        if (ty >= 130 && ty <= 164) {
            if (tx < 220) {
                openKeyboard(KB_CFG_OTA_URL, "URL SERVIDOR O ARCHIVO .BIN:", cfg.otaServerUrl, 94);
            } else {
                Aprs.saveConfig();
                showToast("Verificando actualizacion OTA...", COL_ORANGE);
                Updater.checkAndApplyWebOta(cfg.otaServerUrl, _tft);
                _needsFullRedraw = true;
            }
            return;
        }
        if (ty >= 169 && ty <= 205) {
            if (Updater.isWebPortalRunning()) {
                Updater.stopWebPortal();
            } else {
                showToast("Iniciando Portal Web y AP WiFi...", COL_GREEN);
                Updater.startWebPortal(true);
            }
            _needsFullRedraw = true;
            return;
        }

    } else if (_cfgPage == 4) {
        // Página 5: Configuración de Sensores Meteorológicos (BMP280 / BME280 / AHT20 / DHT22 / DHT11)
        // Fila 2 (y=97..131): Modelo de Sensor y Pines
        if (ty >= 97 && ty <= 131) {
            if (tx < 156) {
                cfg.sensorModel = (cfg.sensorModel + 1) % 6;
            } else {
                cfg.sensorPinProfile = (cfg.sensorPinProfile + 1) % 3;
            }
            Sensors.forceDetectAndRead();
            Aprs.saveConfig();
            _needsFullRedraw = true;
            return;
        }
        // Fila 3 (y=136..170): Modo de Baliza y Calibración de Offset Temp (- / +)
        if (ty >= 136 && ty <= 170) {
            if (tx < 182) {
                cfg.sensorBeaconMode = (cfg.sensorBeaconMode + 1) % 3;
            } else if (tx < 250) {
                cfg.tempOffsetC -= 0.5f;
                if (cfg.tempOffsetC < -15.0f) cfg.tempOffsetC = -15.0f;
                Sensors.forceDetectAndRead();
            } else {
                cfg.tempOffsetC += 0.5f;
                if (cfg.tempOffsetC > 15.0f) cfg.tempOffsetC = 15.0f;
                Sensors.forceDetectAndRead();
            }
            Aprs.saveConfig();
            _needsFullRedraw = true;
            return;
        }
        // Fila 4 (y=175..205): Botón [DETECTAR]
        if (ty >= 175 && ty <= 205 && tx >= 220) {
            showToast("Escaneando sensor...", COL_ACCENT);
            Sensors.forceDetectAndRead();
            _needsFullRedraw = true;
            return;
        }

    } else if (_cfgPage == 5) {
        // Página 6: Parámetros del Aviso de Emergencia (SOS)
        // Fila 1 (y=55..88): Intervalo Baliza SOS (ciclo 60s -> 120s -> 180s)
        if (ty >= 55 && ty <= 88) {
            if (cfg.emergIntervalSec == 60) cfg.emergIntervalSec = 120;
            else if (cfg.emergIntervalSec == 120) cfg.emergIntervalSec = 180;
            else cfg.emergIntervalSec = 60;
            Aprs.saveConfig();
            _needsFullRedraw = true;
            return;
        }
        // Fila 2 (y=91..125): Mensaje Directo SI / NO
        if (ty >= 91 && ty <= 125) {
            cfg.emergSendDirectMsg = !cfg.emergSendDirectMsg;
            Aprs.saveConfig();
            _needsFullRedraw = true;
            return;
        }
        // Fila 3 (y=130..164): Destinatario Mensaje Directo
        if (ty >= 130 && ty <= 164) {
            openKeyboard(KB_CFG_EMERG_DEST, "DESTINATARIO SOS (CALLSIGN):", cfg.emergMsgDest, 9);
            return;
        }
        // Fila 4 (y=169..205): Alerta Sonora Parlante
        if (ty >= 169 && ty <= 205) {
            cfg.emergSoundAlert = !cfg.emergSoundAlert;
            Aprs.saveConfig();
            _needsFullRedraw = true;
            return;
        }

    } else {
        // Página 7 (Siempre la última): Créditos, Versión, Brillo LED/TFT y Reinicio
        if (ty >= 176 && ty <= 208) {
            if (tx < 106) {
                cfg.ledBrightness = (cfg.ledBrightness + 1) % 3; // 1 (Tenue) -> 2 (Medio) -> 0 (Off)
                applyDisplayHardware();
                Aprs.saveConfig();
                _needsFullRedraw = true;
            } else if (tx < 210) {
                if (cfg.tftBrightness >= 100) cfg.tftBrightness = 65;
                else if (cfg.tftBrightness >= 65) cfg.tftBrightness = 35;
                else if (cfg.tftBrightness >= 35) cfg.tftBrightness = 15;
                else cfg.tftBrightness = 100;
                applyDisplayHardware();
                Aprs.saveConfig();
                _needsFullRedraw = true;
            } else {
                showToast("Reiniciando ESP32 CYD APRS...", COL_ORANGE);
                delay(600);
                ESP.restart();
            }
            return;
        }
    }
}

void UiManager::drawEmergConfirmModal() {
    // Fondo oscurecido con marco de advertencia
    _tft.fillRoundRect(8, 16, 304, 206, 8, COL_BG);
    _tft.drawRoundRect(8, 16, 304, 206, 8, COL_RED);
    _tft.drawRoundRect(9, 17, 302, 204, 7, COL_RED);

    // Título y advertencia
    _tft.setTextColor(COL_YELLOW, COL_BG);
    _tft.drawCentreString("! ADVERTENCIA: AVISO DE SOS !", 160, 22, 2);
    _tft.setTextColor(COL_WHITE, COL_BG);
    _tft.drawCentreString("Emitira baliza APRS (\\!) de auxilio.", 160, 40, 2);

    // 4 Botones de selección de motivo (y=58..88)
    const char* types[4] = { "SOS Gen", "Medico", "Vehiculo", "Extravio" };
    for (int i = 0; i < 4; i++) {
        int bx = 14 + (i * 73);
        bool sel = (_selectedEmergType == i);
        uint16_t bg = sel ? COL_RED : COL_CARD;
        uint16_t border = sel ? COL_WHITE : COL_BORDER;
        _tft.fillRoundRect(bx, 58, 69, 28, 4, bg);
        _tft.drawRoundRect(bx, 58, 69, 28, 4, border);
        _tft.setTextColor(sel ? COL_WHITE : COL_MUTED, bg);
        _tft.drawCentreString(types[i], bx + 34, 64, 2);
    }

    // Botón de seguridad progresivo en 3 toques (y=94..146)
    uint16_t btnBg = (_emergConfirmStep == 0) ? COL_ORANGE :
                     (_emergConfirmStep == 1) ? 0xFD60 : COL_RED;
    const char* btnTxt = (_emergConfirmStep == 0) ? "[ 1/3 CONFIRMAR EMERGENCIA ]" :
                         (_emergConfirmStep == 1) ? "[ 2/3 PRESIONA OTRA VEZ ]" :
                                                   ">>> 3/3 ACTIVAR SOS AHORA <<<";

    _tft.fillRoundRect(18, 94, 284, 50, 6, btnBg);
    _tft.drawRoundRect(18, 94, 284, 50, 6, COL_WHITE);
    _tft.setTextColor(COL_WHITE, btnBg);
    _tft.drawCentreString(btnTxt, 160, 110, 2);

    // Barra de tiempo regresivo (y=154..162, 5 segundos)
    uint32_t elapsed = millis() - _emergModalTimerMs;
    int barW = 276;
    int remW = (int)(((5000 - elapsed) * barW) / 5000);
    if (remW < 0) remW = 0;
    _tft.fillRect(22, 154, remW, 8, (elapsed < 3000) ? COL_ORANGE : COL_RED);
    _tft.fillRect(22 + remW, 154, barW - remW, 8, COL_BG);
    _tft.drawRect(21, 153, barW + 2, 10, COL_BORDER);

    // Botón Salir / Cancelar (y=172..206)
    _tft.fillRoundRect(70, 172, 180, 32, 4, COL_HEADER);
    _tft.drawRoundRect(70, 172, 180, 32, 4, COL_BORDER);
    _tft.setTextColor(COL_WHITE, COL_HEADER);
    _tft.drawCentreString("SALIR / CANCELAR", 160, 180, 2);
}

void UiManager::handleEmergConfirmTouch(uint16_t tx, uint16_t ty) {
    // 1. Selector de motivo (y=58..88)
    if (ty >= 58 && ty <= 88) {
        for (int i = 0; i < 4; i++) {
            int bx = 14 + (i * 73);
            if (tx >= bx && tx <= bx + 69) {
                _selectedEmergType = i;
                _emergModalTimerMs = millis(); // Renueva timeout
                _needsFullRedraw = true;
                return;
            }
        }
    }

    // 2. Botón de seguridad progresivo (y=94..146)
    if (ty >= 94 && ty <= 146 && tx >= 18 && tx <= 302) {
        _emergConfirmStep++;
        _emergModalTimerMs = millis(); // Renueva 5 segundos para el siguiente toque
        if (_emergConfirmStep >= 3) {
            _showEmergModal = false;
            _emergConfirmStep = 0;
            Aprs.triggerEmergency(_selectedEmergType);
            _needsFullRedraw = true;
            showToast("! EMERGENCIA SOS TRANSMITIDA !", COL_RED);
        } else {
            _needsFullRedraw = true;
        }
        return;
    }

    // 3. Botón Salir / Cancelar (y=172..206)
    if (ty >= 172 && ty <= 206 && tx >= 70 && tx <= 250) {
        _showEmergModal = false;
        _emergConfirmStep = 0;
        _needsFullRedraw = true;
        return;
    }
}

void UiManager::drawEmergCancelModal() {
    _tft.fillRoundRect(16, 36, 288, 168, 8, COL_BG);
    _tft.drawRoundRect(16, 36, 288, 168, 8, COL_BORDER);
    _tft.drawRoundRect(17, 37, 286, 166, 7, COL_BORDER);

    _tft.setTextColor(COL_YELLOW, COL_BG);
    _tft.drawCentreString("¿CANCELAR AVISO DE SOS?", 160, 48, 2);

    _tft.setTextColor(COL_WHITE, COL_BG);
    _tft.drawCentreString("Se transmitira baliza APRS de", 160, 72, 2);
    _tft.drawCentreString("resolucion y situacion normalizada.", 160, 92, 2);

    // Botón Confirmar Cancelación (y=130..174, x=26..146)
    _tft.fillRoundRect(26, 130, 120, 44, 5, COL_GREEN);
    _tft.drawRoundRect(26, 130, 120, 44, 5, COL_WHITE);
    _tft.setTextColor(COL_BG, COL_GREEN);
    _tft.drawCentreString("CONFIRMAR", 86, 142, 2);

    // Botón Volver / Seguir en Emergencia (y=130..174, x=174..294)
    _tft.fillRoundRect(174, 130, 120, 44, 5, COL_RED);
    _tft.drawRoundRect(174, 130, 120, 44, 5, COL_WHITE);
    _tft.setTextColor(COL_WHITE, COL_RED);
    _tft.drawCentreString("VOLVER", 234, 142, 2);
}

void UiManager::handleEmergCancelTouch(uint16_t tx, uint16_t ty) {
    if (ty >= 130 && ty <= 174) {
        if (tx >= 26 && tx <= 146) {
            // Confirmar Cancelación
            _showCancelModal = false;
            Aprs.cancelEmergency();
            _needsFullRedraw = true;
            showToast("Aviso SOS Cancelado y Normalizado", COL_GREEN);
            return;
        } else if (tx >= 174 && tx <= 294) {
            // Volver
            _showCancelModal = false;
            _needsFullRedraw = true;
            return;
        }
    }
}

void UiManager::showToast(const char* msg, uint16_t bgColor) {
    _tft.fillRoundRect(16, 88, 288, 44, 6, bgColor);
    _tft.drawRoundRect(16, 88, 288, 44, 6, COL_WHITE);
    uint16_t fg = (bgColor == COL_RED || bgColor == COL_HEADER) ? COL_WHITE : COL_BG;
    _tft.setTextColor(fg, bgColor);
    _tft.drawCentreString(msg, 160, 102, 2);
    delay(450);
}
