#include "ui_manager.h"

UiManager Ui;

// Paleta de colores RGB565 estilo Radioaficionado / Oscuro de alto contraste
#define COL_BG          0x0842  // Azul noche muy oscuro
#define COL_HEADER      0x10E4  // Azul pizarra oscuro
#define COL_CARD        0x18E3  // Gris azulado para tarjetas
#define COL_BORDER      0x39E7  // Borde suave
#define COL_ACCENT      0x05FF  // Cian brillante
#define COL_GREEN       0x07E0  // Verde activo / RX
#define COL_RED         0xF800  // Rojo TX / Alerta
#define COL_ORANGE      0xFD20  // Ámbar / Advertencia
#define COL_YELLOW      0xFFE0  // Amarillo / Indicativos
#define COL_WHITE       0xFFFF
#define COL_MUTED       0x9CF3  // Gris claro para etiquetas

UiManager::UiManager() : _tft(TFT_eSPI()) {}

void UiManager::begin() {
    pinMode(CYD_TFT_BL, OUTPUT);
    digitalWrite(CYD_TFT_BL, HIGH);

    _tft.init();
    _tft.setRotation(1); // 320x240 Horizontal
    _tft.fillScreen(COL_BG);

    _touch.begin();
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

    // 2. Si el teclado está abierto, solo redibujar cuando sea necesario
    if (_kbTarget != KB_NONE) {
        if (_needsFullRedraw) {
            drawKeyboard();
            _needsFullRedraw = false;
        }
        return;
    }

    // 3. Redibujado completo al cambiar de pestaña o recibir nuevo paquete
    uint32_t rxNow = Modem.getRxPacketCount();
    uint32_t txNow = Modem.getTxPacketCount();
    if (rxNow != _lastDrawnRxCount && (_currentTab == TAB_MONITOR || _currentTab == TAB_STATIONS || _currentTab == TAB_MESSAGES)) {
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
        return;
    }

    // 4. Actualización periódica ligera (cada 200 ms) sin parpadeo
    if (millis() - _lastDynamicUpdateMs >= 200) {
        _lastDynamicUpdateMs = millis();
        drawTopBar();
        if (_currentTab == TAB_DASHBOARD) {
            drawDashboard(false);
        }
    }
}

void UiManager::drawTopBar() {
    AprsConfig& cfg = Aprs.config();
    _tft.fillRect(0, 0, 320, 22, COL_HEADER);
    _tft.drawFastHLine(0, 22, 320, COL_BORDER);

    // Indicativo propio
    char myCall[16];
    if (cfg.ssid > 0) snprintf(myCall, sizeof(myCall), "%s-%u", cfg.callsign, cfg.ssid);
    else snprintf(myCall, sizeof(myCall), "%s", cfg.callsign);

    _tft.setTextColor(COL_YELLOW, COL_HEADER);
    _tft.drawString(myCall, 6, 3, 2);

    // Perfil de Radio
    const char* radioBadge = (cfg.radioType == RADIO_BAOFENG) ? "BFG" :
                             (cfg.radioType == RADIO_YAESU)   ? "YSU" : "CUS";
    _tft.setTextColor(COL_ACCENT, COL_HEADER);
    _tft.drawString(radioBadge, 92, 3, 2);

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
    _tft.drawString(posBadge, 134, 3, 2);

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

        _tft.fillRect(i * tabW, y, tabW - 1, 28, bg);
        _tft.drawFastHLine(i * tabW, y, tabW - 1, sel ? COL_WHITE : COL_BORDER);
        _tft.setTextColor(fg, bg);
        _tft.drawCentreString(labels[i], i * tabW + (tabW / 2), y + 6, 2);

        // Insignia de mensajes no leídos en MSJS
        if (i == TAB_MESSAGES && Aprs.getUnreadMessageCount() > 0) {
            _tft.fillCircle(i * tabW + tabW - 10, y + 8, 5, COL_RED);
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

    if (fullRedraw) {
        // Tarjeta 1: Coordenadas y Estado de Estación
        _tft.fillRoundRect(6, 28, 198, 66, 4, COL_CARD);
        _tft.drawRoundRect(6, 28, 198, 66, 4, COL_BORDER);

        // Tarjeta 2: Contadores TX / RX
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
        // [BALIZA AHORA] [TEST LOOPBACK] [AUTO BCN]
        _tft.fillRoundRect(6, 176, 106, 31, 4, COL_GREEN);
        _tft.setTextColor(COL_BG, COL_GREEN);
        _tft.drawCentreString("TX BALIZA", 59, 184, 2);

        _tft.fillRoundRect(117, 176, 102, 31, 4, COL_ACCENT);
        _tft.setTextColor(COL_BG, COL_ACCENT);
        _tft.drawCentreString("LOOPBACK", 168, 184, 2);
    }

    // Botón AUTO BCN (puede cambiar de estado)
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

    // Datos de posición en Tarjeta 1
    char latLonBuf[40];
    snprintf(latLonBuf, sizeof(latLonBuf), "Lat: %.4f  Lon: %.4f", Aprs.getCurrentLat(), Aprs.getCurrentLon());
    _tft.setTextColor(COL_WHITE, COL_CARD);
    _tft.drawString(latLonBuf, 12, 33, 2);

    char pathBuf[44];
    snprintf(pathBuf, sizeof(pathBuf), "Ruta: %s-%u,%s-%u  Alt:%dm",
             cfg.path1, cfg.path1Ssid, cfg.path2, cfg.path2Ssid, Aprs.getCurrentAlt());
    _tft.setTextColor(COL_ACCENT, COL_CARD);
    _tft.drawString(pathBuf, 12, 52, 2);

    char commShort[32];
    strlcpy(commShort, cfg.comment, sizeof(commShort));
    _tft.setTextColor(COL_MUTED, COL_CARD);
    _tft.drawString(commShort, 12, 71, 2);

    // Contadores TX / RX en Tarjeta 2
    char countBuf[24];
    snprintf(countBuf, sizeof(countBuf), "TX: %lu", (unsigned long)Modem.getTxPacketCount());
    _tft.setTextColor(COL_ORANGE, COL_CARD);
    _tft.drawString(countBuf, 218, 35, 2);

    snprintf(countBuf, sizeof(countBuf), "RX: %lu", (unsigned long)Modem.getRxPacketCount());
    _tft.setTextColor(COL_GREEN, COL_CARD);
    _tft.drawString(countBuf, 218, 54, 2);

    snprintf(countBuf, sizeof(countBuf), "Est: %d", Aprs.getStationCount());
    _tft.setTextColor(COL_WHITE, COL_CARD);
    _tft.drawString(countBuf, 218, 72, 2);

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
        if (last.hasPosition) {
            snprintf(hdr, sizeof(hdr), "%s -> %.1fkm (%d%s)  ",
                     last.srcCall, last.distanceKm, last.bearingDeg,
                     AprsEngine::bearingToCardinal(last.bearingDeg));
        } else {
            snprintf(hdr, sizeof(hdr), "%s > %s (%s)     ",
                     last.srcCall, last.dstCall, last.pathStr);
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
        _tft.drawString("Pulsa LOOPBACK para probar codif/decodif", 12, 152, 2);
    }
}

void UiManager::drawMonitor(bool fullRedraw) {
    _tft.setTextColor(COL_ACCENT, COL_BG);
    _tft.drawString("MONITOR DE TRAFICO AX.25 / APRS (1200 bps)", 6, 26, 2);

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

        char line1[46];
        uint32_t ageSec = (millis() - p.timestampMs) / 1000;
        if (p.hasPosition) {
            snprintf(line1, sizeof(line1), "%s [%.1fkm %s] hace %lus",
                     p.srcCall, p.distanceKm, AprsEngine::bearingToCardinal(p.bearingDeg),
                     (unsigned long)ageSec);
        } else {
            snprintf(line1, sizeof(line1), "%s > %s (%lus)",
                     p.srcCall, p.dstCall, (unsigned long)ageSec);
        }
        _tft.setTextColor(COL_YELLOW, COL_CARD);
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

    // Botón [ENVIAR RF] y [TEST]
    _tft.fillRoundRect(150, 29, 96, 22, 3, COL_GREEN);
    _tft.setTextColor(COL_BG, COL_GREEN);
    _tft.drawCentreString("ENVIAR RF", 198, 32, 2);

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

        char hdr[44];
        uint16_t badgeCol = COL_ACCENT;
        const char* statusStr = "RX";
        if (!m.incoming) {
            if (m.state == MSG_STATE_TX_ACK) {
                statusStr = "ACK OK";
                badgeCol = COL_GREEN;
            } else if (m.state == MSG_STATE_TX_WAIT) {
                statusStr = "ESP ACK";
                badgeCol = COL_ORANGE;
            } else {
                statusStr = "SIN ACK";
                badgeCol = COL_RED;
            }
            snprintf(hdr, sizeof(hdr), "A %s [%s]", m.peerCall, statusStr);
        } else {
            badgeCol = COL_YELLOW;
            snprintf(hdr, sizeof(hdr), "DE %s [RECIBIDO]", m.peerCall);
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

    // Encabezado con selector de página y botón GUARDAR
    _tft.setTextColor(COL_ACCENT, COL_BG);
    _tft.drawString(_cfgPage == 0 ? "CONFIG 1/2: ESTACION Y POS" : "CONFIG 2/2: RADIO Y MODEM", 6, 27, 2);

    _tft.fillRoundRect(188, 25, 60, 22, 3, COL_HEADER);
    _tft.drawRoundRect(188, 25, 60, 22, 3, COL_BORDER);
    _tft.setTextColor(COL_WHITE, COL_HEADER);
    _tft.drawCentreString(_cfgPage == 0 ? "PAG 2 >" : "< PAG 1", 218, 28, 2);

    _tft.fillRoundRect(254, 25, 62, 22, 3, COL_GREEN);
    _tft.setTextColor(COL_BG, COL_GREEN);
    _tft.drawCentreString("GUARDAR", 285, 28, 2);

    if (_cfgPage == 0) {
        // Fila 1: Indicativo y SSID
        _tft.fillRoundRect(6, 52, 190, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 52, 190, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_MUTED, COL_CARD);
        _tft.drawString("Indicativo:", 12, 61, 2);
        _tft.setTextColor(COL_YELLOW, COL_CARD);
        _tft.drawString(cfg.callsign, 90, 61, 2);

        _tft.fillRoundRect(202, 52, 112, 34, 3, COL_CARD);
        _tft.drawRoundRect(202, 52, 112, 34, 3, COL_BORDER);
        char ssidStr[20];
        snprintf(ssidStr, sizeof(ssidStr), "SSID: -%u", cfg.ssid);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString(ssidStr, 258, 61, 2);

        // Fila 2: Modo Posición (FIJA / GPS UART) y Símbolo
        _tft.fillRoundRect(6, 91, 190, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 91, 190, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_ACCENT, COL_CARD);
        _tft.drawCentreString(cfg.posMode == POS_MODE_FIXED ? "Modo: POSICION FIJA" : "Modo: GPS EXTERNO", 101, 100, 2);

        _tft.fillRoundRect(202, 91, 112, 34, 3, COL_CARD);
        _tft.drawRoundRect(202, 91, 112, 34, 3, COL_BORDER);
        const char* symDesc = (cfg.symbolCode == '[') ? "Simb: Handy" :
                              (cfg.symbolCode == '>') ? "Simb: Auto" :
                              (cfg.symbolCode == '-') ? "Simb: Casa" : "Simb: Camion";
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString(symDesc, 258, 100, 2);

        // Fila 3: Latitud y Longitud Fija
        char latStr[24], lonStr[24];
        snprintf(latStr, sizeof(latStr), "Lat: %.4f", cfg.fixedLat);
        snprintf(lonStr, sizeof(lonStr), "Lon: %.4f", cfg.fixedLon);

        _tft.fillRoundRect(6, 130, 150, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 130, 150, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawString(latStr, 14, 139, 2);

        _tft.fillRoundRect(164, 130, 150, 34, 3, COL_CARD);
        _tft.drawRoundRect(164, 130, 150, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawString(lonStr, 172, 139, 2);

        // Fila 4: Comentario de Baliza
        _tft.fillRoundRect(6, 169, 308, 36, 3, COL_CARD);
        _tft.drawRoundRect(6, 169, 308, 36, 3, COL_BORDER);
        _tft.setTextColor(COL_MUTED, COL_CARD);
        _tft.drawString("Coment:", 12, 179, 2);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        char commShort[30];
        strlcpy(commShort, cfg.comment, sizeof(commShort));
        _tft.drawString(commShort, 72, 179, 2);
    } else {
        // Página 2: Radio (Baofeng vs Yaesu), TX Delay, Volumen DAC, Intervalo Baliza, Digipeater
        _tft.fillRoundRect(6, 52, 308, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 52, 308, 34, 3, COL_BORDER);
        const char* radStr = (cfg.radioType == RADIO_BAOFENG) ? "Radio Preset: BAOFENG UV-5R / UV-82" :
                             (cfg.radioType == RADIO_YAESU)   ? "Radio Preset: YAESU FT-60 / FT-65 / 2980" :
                                                                "Radio Preset: PERSONALIZADO";
        _tft.setTextColor(COL_YELLOW, COL_CARD);
        _tft.drawCentreString(radStr, 160, 61, 2);

        // Fila 2: TX Delay (ms) y Volumen TX (%)
        char dlyStr[28], volStr[28];
        snprintf(dlyStr, sizeof(dlyStr), "TX Delay: %u ms", cfg.txDelayMs);
        snprintf(volStr, sizeof(volStr), "Nivel TX: %u %%", cfg.txVolume);

        _tft.fillRoundRect(6, 91, 150, 34, 3, COL_CARD);
        _tft.drawRoundRect(6, 91, 150, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString(dlyStr, 81, 100, 2);

        _tft.fillRoundRect(164, 91, 150, 34, 3, COL_CARD);
        _tft.drawRoundRect(164, 91, 150, 34, 3, COL_BORDER);
        _tft.setTextColor(COL_WHITE, COL_CARD);
        _tft.drawCentreString(volStr, 239, 100, 2);

        // Fila 3: Intervalo de Baliza y Auto-ACK
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

        // Fila 4: Fill-in Digipeater WIDE1-1
        _tft.fillRoundRect(6, 169, 308, 36, 3, COL_CARD);
        _tft.drawRoundRect(6, 169, 308, 36, 3, COL_BORDER);
        _tft.setTextColor(cfg.digipeatWide1 ? COL_ORANGE : COL_MUTED, COL_CARD);
        _tft.drawCentreString(cfg.digipeatWide1 ? "Repetidor Digipeater WIDE1-1: ACTIVO" : "Repetidor Digipeater WIDE1-1: APAGADO", 160, 179, 2);
    }
}

void UiManager::openKeyboard(KeyboardTarget target, const char* title, const char* initialVal, uint8_t maxLen) {
    _kbTarget = target;
    strlcpy(_kbTitle, title, sizeof(_kbTitle));
    strlcpy(_kbBuffer, initialVal ? initialVal : "", sizeof(_kbBuffer));
    _kbMaxLen = (maxLen > 66) ? 66 : maxLen;
    _kbShift = true;
    _needsFullRedraw = true;
}

void UiManager::drawKeyboard() {
    _tft.fillScreen(COL_BG);

    // Barra superior de título y botones [ESC] / [OK]
    _tft.fillRect(0, 0, 320, 26, COL_HEADER);
    _tft.setTextColor(COL_ACCENT, COL_HEADER);
    _tft.drawString(_kbTitle, 8, 5, 2);

    _tft.fillRoundRect(208, 3, 50, 20, 3, COL_RED);
    _tft.setTextColor(COL_WHITE, COL_RED);
    _tft.drawCentreString("ESC", 233, 5, 2);

    _tft.fillRoundRect(264, 3, 50, 20, 3, COL_GREEN);
    _tft.setTextColor(COL_BG, COL_GREEN);
    _tft.drawCentreString("OK", 289, 5, 2);

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

    // Filas del teclado QWERTY + Números
    static const char* row0 = "1234567890";
    static const char* row1U = "QWERTYUIOP";
    static const char* row1L = "qwertyuiop";
    static const char* row2U = "ASDFGHJKL-";
    static const char* row2L = "asdfghjkl-";
    static const char* row3U = "ZXCVBNM./!";
    static const char* row3L = "zxcvbnm./!";

    const char* rows[4] = {
        row0,
        _kbShift ? row1U : row1L,
        _kbShift ? row2U : row2L,
        _kbShift ? row3U : row3L
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

    // Fila inferior: [MAY/min] [ESPACIO] [BORRAR]
    int yBot = 202;
    _tft.fillRoundRect(4, yBot, 72, 34, 3, _kbShift ? COL_ACCENT : COL_HEADER);
    _tft.setTextColor(_kbShift ? COL_BG : COL_WHITE, _kbShift ? COL_ACCENT : COL_HEADER);
    _tft.drawCentreString("MAY/min", 40, yBot + 9, 2);

    _tft.fillRoundRect(82, yBot, 154, 34, 3, COL_CARD);
    _tft.drawRoundRect(82, yBot, 154, 34, 3, COL_BORDER);
    _tft.setTextColor(COL_WHITE, COL_CARD);
    _tft.drawCentreString("ESPACIO", 159, yBot + 9, 2);

    _tft.fillRoundRect(242, yBot, 72, 34, 3, COL_ORANGE);
    _tft.setTextColor(COL_BG, COL_ORANGE);
    _tft.drawCentreString("<- BORR", 278, yBot + 9, 2);
}

void UiManager::handleKeyboardTouch(uint16_t tx, uint16_t ty) {
    // Botones superiores ESC y OK
    if (ty < 28) {
        if (tx >= 204 && tx <= 260) {
            _kbTarget = KB_NONE;
            _needsFullRedraw = true;
            return;
        }
        if (tx >= 262) {
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
            static const char* row3U = "ZXCVBNM./!";
            static const char* row3L = "zxcvbnm./!";
            const char* rows[4] = {
                row0,
                _kbShift ? row1U : row1L,
                _kbShift ? row2U : row2L,
                _kbShift ? row3U : row3L
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
        if (tx < 78) {
            _kbShift = !_kbShift;
            _needsFullRedraw = true;
        } else if (tx >= 82 && tx < 238) {
            size_t len = strlen(_kbBuffer);
            if (len < _kbMaxLen) {
                _kbBuffer[len] = ' ';
                _kbBuffer[len + 1] = '\0';
                _needsFullRedraw = true;
            }
        } else if (tx >= 240) {
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
            break;
        case KB_CFG_COMMENT:
            strlcpy(cfg.comment, _kbBuffer, sizeof(cfg.comment));
            break;
        case KB_CFG_LAT:
            cfg.fixedLat = atof(_kbBuffer);
            break;
        case KB_CFG_LON:
            cfg.fixedLon = atof(_kbBuffer);
            break;
        default:
            break;
    }
}

void UiManager::handleTouch(uint16_t tx, uint16_t ty) {
    if (_kbTarget != KB_NONE) {
        handleKeyboardTouch(tx, ty);
        return;
    }

    // Barra de pestañas inferior (y >= 210)
    if (ty >= 210) {
        int tabIdx = tx / (320 / 5);
        if (tabIdx >= 0 && tabIdx <= 4) {
            _currentTab = (UiTab)tabIdx;
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
    if (ty >= 174 && ty <= 208) {
        if (tx >= 6 && tx <= 112) {
            // [TX BALIZA] por RF
            showToast("Transmitiendo Baliza APRS (1200 bps)...", COL_RED);
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
            Aprs.config().autoBeacon = !Aprs.config().autoBeacon;
            _needsFullRedraw = true;
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
            showToast("Enviando Mensaje APRS por RF...", COL_RED);
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

    // Botones de barra superior de Config: [PAG 1/2] y [GUARDAR]
    if (ty >= 24 && ty <= 48) {
        if (tx >= 186 && tx <= 250) {
            _cfgPage = (_cfgPage == 0) ? 1 : 0;
            _needsFullRedraw = true;
            return;
        }
        if (tx >= 252) {
            Aprs.saveConfig();
            showToast("Configuracion guardada en Flash NVS!", COL_GREEN);
            _needsFullRedraw = true;
            return;
        }
    }

    if (_cfgPage == 0) {
        // Fila 1 (y=52..86): Indicativo / SSID
        if (ty >= 52 && ty <= 86) {
            if (tx < 198) {
                openKeyboard(KB_CFG_CALLSIGN, "TU INDICATIVO (SIN SSID):", cfg.callsign, 6);
            } else {
                cfg.ssid = (cfg.ssid + 1) % 16;
                _needsFullRedraw = true;
            }
            return;
        }
        // Fila 2 (y=91..125): Modo Posición / Símbolo
        if (ty >= 91 && ty <= 125) {
            if (tx < 198) {
                cfg.posMode = (cfg.posMode == POS_MODE_FIXED) ? POS_MODE_GPS : POS_MODE_FIXED;
            } else {
                if (cfg.symbolCode == '[') cfg.symbolCode = '>';
                else if (cfg.symbolCode == '>') cfg.symbolCode = '-';
                else if (cfg.symbolCode == '-') cfg.symbolCode = 'k';
                else cfg.symbolCode = '[';
            }
            _needsFullRedraw = true;
            return;
        }
        // Fila 3 (y=130..164): Latitud / Longitud
        if (ty >= 130 && ty <= 164) {
            char numBuf[20];
            if (tx < 160) {
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
    } else {
        // Página 2: Radio Preset / TX Delay / TX Volume / Intervalo / Auto-ACK / Digi
        if (ty >= 52 && ty <= 86) {
            RadioType nextType = (cfg.radioType == RADIO_BAOFENG) ? RADIO_YAESU : RADIO_BAOFENG;
            Aprs.applyRadioPreset(nextType);
            _needsFullRedraw = true;
            return;
        }
        if (ty >= 91 && ty <= 125) {
            if (tx < 160) {
                // Ciclar TX Delay: 200 -> 300 -> 450 -> 600 -> 750 ms
                cfg.txDelayMs += 100;
                if (cfg.txDelayMs > 750) cfg.txDelayMs = 200;
                cfg.radioType = RADIO_CUSTOM;
            } else {
                // Ciclar Volumen TX: 25% -> 50% -> 75% -> 100%
                cfg.txVolume += 15;
                if (cfg.txVolume > 100) cfg.txVolume = 25;
                cfg.radioType = RADIO_CUSTOM;
            }
            Modem.configure(cfg.txDelayMs, cfg.txTailMs, cfg.txVolume);
            _needsFullRedraw = true;
            return;
        }
        if (ty >= 130 && ty <= 164) {
            if (tx < 160) {
                // Ciclar intervalo de baliza: 60s, 120s, 300s, 600s, 900s
                if (cfg.beaconIntervalSec < 120) cfg.beaconIntervalSec = 120;
                else if (cfg.beaconIntervalSec < 300) cfg.beaconIntervalSec = 300;
                else if (cfg.beaconIntervalSec < 600) cfg.beaconIntervalSec = 600;
                else if (cfg.beaconIntervalSec < 900) cfg.beaconIntervalSec = 900;
                else cfg.beaconIntervalSec = 60;
            } else {
                cfg.autoAckMessages = !cfg.autoAckMessages;
            }
            _needsFullRedraw = true;
            return;
        }
        if (ty >= 169 && ty <= 205) {
            cfg.digipeatWide1 = !cfg.digipeatWide1;
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
}

