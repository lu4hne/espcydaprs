# ESP32-2432S024C (CYD) — Transceptor APRS 1200 bps / Tracker / Estación Fija + IGate (`v1.2.0`)

Aplicación completa de **codificación y decodificación APRS a 1200 baudios (AFSK Bell 202 / AX.25)** con **IGate APRS-IS por WiFi** y **actualización de firmware por MicroSD y OTA**, diseñada específicamente para la placa **ESP32 Cheap Yellow Display `ESP32-2432S024C`** (pantalla TFT ILI9341 de 2.4" `320x240`, panel táctil capacitivo **CST820** y lector **MicroSD** incorporado).

Permite conectar directamente un handy o base **Yaesu** o **Baofeng** (en VHF `144.390 MHz` / `144.930 MHz` o UHF) para operar tanto en terreno como **Tracker autónomo con GPS** como en el shack como **Estación Fija + IGate (RF $\leftrightarrow$ APRS-IS)** y **Digipeater Fill-in `WIDE1-1`**.

---

## Características Principales

* **3 Modos de Operación Configurables en Pantalla o vía Web (`CONF 1/4` o toque rápido en `PANEL`):**
  1. **`MODO TRACKER (Móvil / Portátil RF)`:**
     * Diseñado para operación vehicular o portátil con **GPS Externo NMEA** (`UART2`) o posición fija, SSID sugerido `-9` / `-7`, símbolo de vehículo (`/>`) o handy (`/[`).
     * Transmite balizas periódicas por RF (`1200 bps AFSK`) manteniendo el WiFi apagado para ahorrar batería.
  2. **`MODO ESTACIÓN FIJA + IGATE (RF -> APRS-IS)`:**
     * Conecta por WiFi al servidor **APRS-IS** (por defecto `samerica.aprs2.net:14580` o `rotate.aprs2.net:14580`).
     * **Cálculo automático de Passcode APRS-IS** a partir de tu indicativo (`CALLSIGN`), con opción de ingreso manual.
     * **Inyección RF $\rightarrow$ APRS-IS (`qAR`):** Todo paquete AX.25/APRS válido decodificado desde el radio VHF/UHF se inyecta en tiempo real a la red mundial APRS-IS (visible en `aprs.fi` y `aprsdirect.com`).
     * **Mensajería APRS-IS $\leftrightarrow$ RF:** Recibe mensajes desde APRS-IS dirigidos a tu estación (con auto-ACK) y puede retransmitir por RF mensajes de texto dirigidos a estaciones locales escuchadas recientemente.
     * **Baliza de Estación Fija Configurable:** Permite emitir tu posición fija por **`RF + APRS-IS`**, **`SOLO APRS-IS (Internet)`** o **`SOLO RF`**.
  3. **`MODO ESTACIÓN FIJA SOLO RF / DIGIPEATER`:**
     * Ideal para estaciones base o repetidoras de campaña sin acceso a Internet, con soporte para **Digipeater Fill-in `WIDE1-1`**.
* **Actualización de Firmware por MicroSD y OTA (`CONF 4/4`):**
  * **Auto-Actualización por MicroSD en Arranque:** Si insertas una tarjeta MicroSD con `/update.bin` o `/firmware.bin`, la placa lo detecta al encender, muestra la barra de progreso en pantalla, actualiza la Flash y renombra el archivo a `/update.bak` para evitar bucles.
  * **Escáner e Instalador MicroSD en Pantalla:** En `CONF 4/4` puedes pulsar **`[ESCANEAR SD]`** para listar archivos `.bin` en la tarjeta e instalarlos con un toque.
  * **Portal Web Local + Subida OTA por Navegador (`WebServer` + `ArduinoOTA`):** En modo IGate (o pulsando **`[ACTIVAR PORTAL WEB / AP WIFI OTA]`** que levanta la red WiFi `CYD-APRS-<INDICATIVO>` en `http://192.168.4.1`), puedes configurar toda la estación o subir un archivo `firmware.bin` desde el celular o PC.
  * **Actualización Remota HTTP/HTTPS (`OTA URL`):** Descarga directa desde URL `.bin` o servidor con manifiesto JSON y validación de versión semántica (`SemVer`).
* **Módem DSP AFSK 1200 bps (Bell 202) en Tiempo Real (`Core 0`):**
  * **Modulador TX:** Síntesis Digital Directa (DDS) de fase continua para tonos **Mark (`1200 Hz`)** y **Space (`2200 Hz`)** en `GPIO 26` (DAC2), con codificación NRZI, *bit-stuffing* HDLC, `CRC-16 CCITT` y control automático de PTT (`GPIO 22`).
  * **Demodulador RX:** Muestreo ADC (`GPIO 35`) a `9600 Hz`, filtro eliminador de continua (DC blocker), correlador en cuadratura (*Matched Filter*), lazo **DPLL** y validación `CRC-16`.
  * **Modo Test Loopback Integrado:** Prueba la codificación y decodificación AX.25/APRS directamente en la placa con un toque en pantalla.

---

## Arquitectura del Proyecto

```text
espcydaprs/
├── platformio.ini          # Configuración PlatformIO (min_spiffs.csv, TFT_eSPI, TinyGPS++, ArduinoJson)
├── include/
│   ├── config.h            # Pinout de hardware (Radio, GPS, MicroSD), modos y estructura NVS
│   ├── cst820_touch.h      # Driver I2C para el panel táctil capacitivo CST820
│   ├── afsk_modem.h        # Módem DSP Bell 202 (1200/2200 Hz, NRZI, HDLC, CRC-16)
│   ├── ax25_aprs.h         # Codificador/Decodificador AX.25, parser APRS, GPS y modos
│   ├── igate_manager.h     # Cliente TCP APRS-IS (inyección qAR, Passcode hash, IS<->RF)
│   ├── update_manager.h    # Actualizador por MicroSD, HTTP/HTTPS OTA y Portal Web / AP
│   └── ui_manager.h        # Interfaz gráfica táctil de 5 pestañas + 4 páginas Config + QWERTY
└── src/
    ├── main.cpp            # Inicialización dual-core, auto-update SD y comandos USB Serial
    ├── cst820_touch.cpp    # Implementación del controlador táctil CST820
    ├── afsk_modem.cpp      # Procesamiento digital de señales (Core 0 a 9600 Hz)
    ├── ax25_aprs.cpp       # Lógica APRS, mensajería con ACK, Haversine y NVS
    ├── igate_manager.cpp   # Gestión WiFi, login APRS-IS e inyección de paquetes RF->IS
    ├── update_manager.cpp  # Flasheo desde MicroSD (VSPI) y OTA Web
    └── ui_manager.cpp      # Renderizado TFT_eSPI y eventos táctiles (Core 1)
```

---

## Pinout en la Placa `ESP32-2432S024C`

La placa **ESP32-2432S024C** ya tiene internamente cableados la pantalla SPI (`ILI9341`), el touch capacitivo (`CST820`) y el zócalo **MicroSD** (`VSPI`). Para la radio y el GPS utilizamos los pines libres expuestos en los conectores laterales:

| Función | Pin ESP32 | Ubicación en la 2432S024C | Descripción |
| :--- | :---: | :--- | :--- |
| **Audio TX (hacia MIC Radio)** | **`GPIO 26`** | Conector P3 / Salida DAC2 | Señal senoidal AFSK generada por el DAC interno (8 bits). |
| **Audio RX (desde SPK Radio)** | **`GPIO 35`** | Conector P3 (`ADC1_CH7`) | Entrada de audio analógico polarizada a `1.65V DC`. |
| **Control PTT Radio** | **`GPIO 22`** | Conector CN1 / P3 | Salida digital (`3.3V` en TX, `0V` en RX) hacia transistor NPN. |
| **GPS RX (Desde TX del GPS)** | **`GPIO 17`** | Conector P3 / LED Verde | Entrada UART2 (`9600 bps`) para módulo GPS NMEA opcional. |
| **GPS TX (Hacia RX del GPS)** | **`GPIO 16`** | Conector P3 / LED Azul | Salida UART2 (`9600 bps`) hacia módulo GPS opcional. |
| **MicroSD (`CS / SCK / MISO / MOSI`)** | **`5 / 18 / 19 / 23`** | Zócalo MicroSD Integrado | Bus VSPI dedicado para actualización de firmware `.bin`. |
| **Indicador LED TX** | **`GPIO 4`** | LED RGB Trasero (Rojo) | Se enciende en rojo automáticamente mientras transmite (PTT activo). |
| **Tierra Común** | **`GND`** | Conectores P3 / CN1 | Referencia común entre la placa CYD y el chasis/masa del radio. |

---

## Circuito de Interfaz entre el ESP32 y el Radio (Baofeng / Yaesu)

Para acoplar el audio sin saturar el micrófono del radio ni dañar el ADC del ESP32 con voltajes negativos, arma esta sencilla interfaz en una pequeña placa perforada:

### Esquema Eléctrico Completo

```text
================================================================================
1. ENTRADA DE AUDIO RX (Salida Auricular/SPK del Radio ---> GPIO 35 del ESP32)
================================================================================

                  +3.3V (ESP32)
                     |
                    [R1] 10 kΩ
                     |
SPK Radio o---||-----+----------------> GPIO 35 (ADC1_CH7)
             C1      |
            1 µF    [R2] 10 kΩ
                     |
                    GND (Común)

* R1 y R2 (10 kΩ) crean un punto medio de 1.65V DC para que la onda de audio
  oscile entre 0V y 3.3V sin recortarse.
* C1 (1 µF cerámico o electrolítico) aísla el nivel DC del amplificador del radio.

================================================================================
2. SALIDA DE AUDIO TX (GPIO 26 DAC del ESP32 ---> Entrada MIC del Radio)
================================================================================

                                    C2 (1 µF)
GPIO 26 (DAC) o---[R3 10 kΩ]---+------||--------> Entrada MIC del Radio
                               |
                              [R4] 1 kΩ (o potenciómetro de 2.2 kΩ)
                               |
                              GND (Común)

* R3 (10 kΩ) y R4 (1 kΩ) forman un atenuador ~10:1 que reduce los ~2.5 Vpp del
  DAC a ~200 mVpp (nivel ideal de micrófono electret para Yaesu y Baofeng).
* C2 (1 µF) bloquea los +5V/+8V de alimentación fantasma (bias) del micrófono.

================================================================================
3. CONTROL DE PTT (GPIO 22 del ESP32 ---> Línea PTT del Radio)
================================================================================

                               Colector
                                  |-------------> Pin PTT del Radio
GPIO 22 o----[R5 2.2 kΩ]----|  2N2222 / BC548
                            |     |
                             \    v Emisor
                                  |
                                 GND (Común)
```

---

## Guía de Uso en la Pantalla Táctil

La interfaz cuenta con **5 pestañas** en la barra inferior:

1. **`PANEL` (Dashboard Principal):**
   * Muestra tu indicativo (`LU4HNE-7` / `LU4HNE-10`), el **Modo Activo** (`MODO: TRACKER RF`, `MODO: FIJA + IGATE` o `MODO: FIJA SOLO RF`), estado de posición (`POS:FIJA` o `GPS:xSAT`), estado de conexión **APRS-IS** (`IG:ON` en verde cuando está verificado) y contadores `TX`, `RX` e `IG` (paquetes inyectados a APRS-IS).
   * **Cambio rápido de modo:** Toca la tarjeta superior izquierda en `PANEL` para alternar entre **Tracker**, **Estación Fija + IGate** y **Fija Solo RF**.
   * **Barra `AUDIO RX` (Vúmetro en vivo):** Ajusta el volumen de tu handy hasta que al recibir una señal el vúmetro se mantenga en la **zona verde (`30%` a `80%`)**.
   * **`[TX BALIZA]` / `[LOOPBACK]` / `[AUTO]`:** Transmite baliza inmediata (por RF y/o APRS-IS), ejecuta autodiagnóstico en bucle cerrado o activa el temporizador automático.
2. **`MONIT` (Monitor de Tráfico AX.25 / IGate):**
   * Lista en tiempo real de paquetes decodificados con distancia (`km`), rumbo cardinal y etiqueta verde **`[IGATE]`** cuando el paquete fue inyectado exitosamente en **APRS-IS**.
3. **`MSJS` (Mensajes APRS Bidireccionales por RF y APRS-IS):**
   * Redacta mensajes con el teclado táctil QWERTY, con confirmación automática **`ACK`** y reintentos.
4. **`ESTAC` (Directorio de Estaciones Escuchadas):**
   * Lista de estaciones únicas con distancia, acimut, coordenadas y cantidad de paquetes.
5. **`CONF` (Configuración en 4 Páginas + Guardado en Flash NVS):**
   * **Página 1 (`1/4: MODO Y ESTACION`):** Selector de Rol (`TRACKER MOVIL`, `ESTACION FIJA + IGATE`, `ESTACION FIJA SOLO RF`), Indicativo, SSID (`-0` a `-15`), Símbolo (`IGate I&`, `Handy`, `Auto`, `Casa`, `Camión`), fuente de posición (`FIJA` / `GPS`), Latitud, Longitud y Comentario.
   * **Página 2 (`2/4: RADIO Y BALIZA`):** Selector de preset **`BAOFENG`** / **`YAESU`**, `TX Delay (ms)`, `Nivel TX (%)`, intervalo de baliza (`60s` a `900s`), `Auto-ACK` y **`Digipeater WIDE1-1`**.
   * **Página 3 (`3/4: WIFI E IGATE`):** SSID y Clave WiFi (con teclado alfanumérico + símbolos `#?=`), servidor APRS-IS (`samerica.aprs2.net` / `rotate.aprs2.net`), `Passcode` (calculado automáticamente desde tu indicativo o manual), modo de baliza IGate (`RF + IS`, `SOLO IS`, `SOLO RF`) y botón **`[CONECTAR]`**.
   * **Página 4 (`4/4: SD Y WEB OTA`):**
     * **`[ESCANEAR SD]` / `[INSTALAR SD]`:** Busca e instala archivos `.bin` desde la tarjeta MicroSD con barra de progreso en pantalla.
     * **`[OTA URL]`:** Descarga e instala actualizaciones remotas por HTTP/HTTPS.
     * **`[ACTIVAR PORTAL WEB / AP WIFI OTA]`:** Inicia el servidor web de configuración y actualización OTA en `http://<IP>` (o crea el punto de acceso WiFi `CYD-APRS-<INDICATIVO>` en `http://192.168.4.1`).

---

## Compilación y Carga con PlatformIO

1. Para compilar el proyecto:
   ```bash
   pio run
   ```
2. Para subir por cable USB:
   ```bash
   pio run -t upload
   ```
3. Para actualizar por **MicroSD**:
   * Copia el archivo generado `.pio/build/esp32-2432s024c/firmware.bin` a la raíz de una tarjeta MicroSD FAT32 (como `/firmware.bin` o `/update.bin`).
   * Inserta la tarjeta en la placa CYD y reinicia (se instalará automáticamente al arrancar) o ve a **`CONF` -> `PAG 4/4` -> `[ESCANEAR SD]`**.
