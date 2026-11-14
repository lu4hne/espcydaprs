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

## Pinout en las Placas CYD (Modelos `ILI9341` y `ST7789`)

### ¿Por qué no aparece `IO26` en los conectores laterales de la CYD?
En todas las variantes de la familia **CYD (`ESP32-2432S028R`, `ESP32-2432S024C`, etc.)**, el pin **`GPIO 26` (`DAC2`)** **no sale a los conectores blancos de expansión (`P3` / `CN1`)** porque está cableado internamente en el PCB a la entrada (`pin 4 IN-`) del amplificador de audio integrado **`SC8002B` / `FM8002A`**, cuya salida va al conector blanco de 2 pines **`SPK` (`P4`)**.

Para resolver esto sin necesidad de soldar sobre el integrado, el firmware genera la onda senoidal AFSK (`1200 / 2200 Hz`) **simultáneamente por dos vías**:
1. **Por un pin libre de tus conectores blancos de expansión** (`IO27`, `IO21`, `IO4` o `IO17`, seleccionable en pantalla) usando el modulador hardware **Sigma-Delta DAC del ESP32 a `312.5 kHz`** (que además se desconecta automáticamente al pasar a `RX` para cero ruido en recepción).
2. **Por `GPIO 26` (`DAC2`)** hacia el conector `SPK` (por si en algún momento deseas tomar audio analógico de `IO26`).

---

### Tabla de Conexiones según tu Modelo de Placa CYD

Puedes cambiar entre ambos modelos en cualquier momento desde **`CONF` -> `PAG 2/4`**, manteniendo presionado el botón físico **`BOOT` durante 2 segundos**, enviando la letra **`d`** por el monitor serie USB o desde el **Portal Web**:

#### Modelo A: Placa CYD con Driver `ILI9341`
*(Conectores expuestos: `IO35`, `IO22`, `IO21`, `IO27`, `TX`, `RX`)*

| Función | Pin a Usar | Comentario / Ubicación |
| :--- | :---: | :--- |
| **Audio RX (desde SPK Radio)** | **`IO35`** | Conector `P3` (`ADC1_CH7` - Solo entrada analógica). |
| **Control PTT Radio** | **`IO22`** | Conector `CN1` / `P3` (Salida `3.3V` en TX, `0V` en RX). |
| **Audio TX (hacia MIC Radio)** | **`IO27`** *(o `IO21`)* | Conector `CN1` / `P3` (DAC Sigma-Delta `312.5 kHz`). Si tu placa usa `IO21` para el Backlight, usa **`IO27`** para TX (por defecto); si usa `IO27` para Backlight, selecciona **`IO21`** en `CONF 2/4`. |
| **Backlight Pantalla TFT** | **`IO21` / `IO27`** | El firmware enciende automáticamente el pin que no esté asignado a `Audio TX`. |
| **Tierra Común** | **`GND`** | Conector `P3` / `CN1`. |

#### Modelo B: Placa CYD Antigua con Driver `ST7789` (1 solo USB)
*(Conectores expuestos: `IO35`, `IO22`, `IO21`, `IO4`, `IO16`, `IO17`, `TX`, `RX`)*

| Función | Pin a Usar | Comentario / Ubicación |
| :--- | :---: | :--- |
| **Audio RX (desde SPK Radio)** | **`IO35`** | Conector `P3` (`ADC1_CH7` - Solo entrada analógica). |
| **Control PTT Radio** | **`IO22`** | Conector `CN1` / `P3` (Salida `3.3V` en TX, `0V` en RX). |
| **Audio TX (hacia MIC Radio)** | **`IO4`** *(o `IO17`)* | Conector `P3` (DAC Sigma-Delta `312.5 kHz`). Al elegir `Placa: ST7789` se asigna **`IO4`** automáticamente, dejando libres `IO16`/`IO17` para el GPS. |
| **GPS RX (Desde TX del GPS)** | **`IO17`** | Conector `P3` (Entrada UART2 `9600 bps` para módulo GPS opcional). |
| **GPS TX (Hacia RX del GPS)** | **`IO16`** | Conector `P3` (Salida UART2 `9600 bps` hacia módulo GPS opcional). |
| **Backlight Pantalla TFT** | **`IO21`** | Iluminación de pantalla TFT. |
| **MicroSD (`CS / SCK / MISO / MOSI`)** | **`5 / 18 / 19 / 23`** | Zócalo MicroSD integrado (Bus VSPI). |
| **Tierra Común** | **`GND`** | Conector `P3` / `CN1`. |

> **Nota sobre el Touch:** El firmware auto-detecta al arrancar si la placa tiene panel táctil **capacitivo (`CST820` / `FT6336` por I2C)** o **resistivo (`XPT2046` por SPI en pines `CLK=25, MOSI=32, MISO=39, CS=33, IRQ=36`)**, funcionando en ambos modelos sin recompilar.

---

## Circuito de Interfaz entre el ESP32 y el Radio (Baofeng / Yaesu)

Para acoplar el audio sin saturar el micrófono del radio ni dañar el ADC del ESP32 con voltajes negativos, arma esta sencilla interfaz en una pequeña placa perforada:

### Esquema Eléctrico Completo

```text
================================================================================
1. ENTRADA DE AUDIO RX (Salida Auricular/SPK del Radio ---> IO35 del ESP32)
================================================================================

                  +3.3V (ESP32)
                     |
                    [R1] 10 kΩ
                     |
SPK Radio o---||-----+----------------> IO35 (ADC1_CH7)
             C1      |
            1 µF    [R2] 10 kΩ
                     |
                    GND (Común)

* R1 y R2 (10 kΩ) crean un punto medio de 1.65V DC para que la onda de audio
  oscile entre 0V y 3.3V sin recortarse.
* C1 (1 µF cerámico o electrolítico) aísla el nivel DC del amplificador del radio.

================================================================================
2. SALIDA DE AUDIO TX (IO27 / IO4 / IO21 / IO17 ---> Entrada MIC del Radio)
================================================================================

                                             C2 (1 µF)
Pin TX (IO27 o IO4) o---[R3 10 kΩ]---+---+------||--------> Entrada MIC del Radio
                                     |   |
                            [R4] 1 kΩ   === C3 (10 nF a 100 nF cerámico "103"/"104")
                                     |   |
                                    GND GND (Común)

* R3 (10 kΩ) y R4 (1 kΩ) forman un atenuador ~10:1 que reduce los ~2.5 Vpp a
  ~200 mVpp (nivel ideal de micrófono electret para Yaesu y Baofeng).
* R3 (10 kΩ) + C3 (10 nF a 100 nF en paralelo con R4) forman además el filtro
  pasa-bajos RC que elimina por completo la portadora Sigma-Delta de 312.5 kHz,
  entregando una onda senoidal pura de 1200 / 2200 Hz.
* C2 (1 µF) bloquea los +5V/+8V de alimentación fantasma (bias) del micrófono.

================================================================================
3. CONTROL DE PTT (IO22 del ESP32 ---> Línea PTT del Radio)
================================================================================

                               Colector
                                  |-------------> Pin PTT del Radio
IO22 o-------[R5 2.2 kΩ]----|  2N2222 / BC548
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
