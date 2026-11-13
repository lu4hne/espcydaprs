# ESP32-2432S024C (CYD) — Transceptor / TNC APRS 1200 Baudios (VHF / UHF)

Aplicación completa de **codificación y decodificación APRS a 1200 baudios (AFSK Bell 202 / AX.25)** diseñada específicamente para la placa **ESP32 Cheap Yellow Display `ESP32-2432S024C`** (pantalla TFT ILI9341 de 2.4" `320x240` con panel táctil capacitivo **CST820**).

Permite conectar directamente un handy o base **Yaesu** o **Baofeng** (en VHF `144.390 MHz` / `144.930 MHz` o UHF) para recibir y enviar paquetes APRS en campo sin depender de un teléfono celular ni computadora.

---

## Características Principales

* **Módem DSP AFSK 1200 bps (Bell 202) en Tiempo Real (`Core 0`):**
  * **Modulador TX:** Síntesis Digital Directa (DDS) de fase continua para tonos **Mark (`1200 Hz`)** y **Space (`2200 Hz`)** a través del DAC interno (`GPIO 26`), con codificación NRZI, *bit-stuffing* HDLC, cálculo de `CRC-16 CCITT` y control automático de PTT (`GPIO 22`).
  * **Demodulador RX:** Muestreo ADC (`GPIO 35`) a `9600 Hz`, filtro IIR eliminador de continua (DC blocker), correlador en cuadratura (*Matched Filter* de fase invariante), lazo de seguimiento de fase digital (**DPLL**) para recuperación de reloj, *bit-unstuffing* y validación de `CRC-16`.
  * **Modo Test Loopback Integrado:** Permite probar la codificación y decodificación AX.25/APRS directamente en la placa con un toque en pantalla antes de conectar el radio.
* **Funciones APRS Completas (`Core 1`):**
  * **Baliza de Posición (Beacon):** Manual o automática periódica (`60s`, `120s`, `300s`, `600s`, `900s`) con soporte para **Coordenadas Fijas** (editables en pantalla) o **GPS Externo NMEA en tiempo real** (`TinyGPS++` por UART2).
  * **Mensajería APRS Bidireccional:**
    * Bandeja de entrada y salida de mensajes dirigidos a tu indicativo (`CALLSIGN-SSID`).
    * Respuesta automática de confirmación **`ACK`** al recibir un mensaje.
    * Motor de reintentos automáticos en transmisión hasta recibir `ACK` de la estación remota.
    * **Teclado táctil QWERTY completo en pantalla** para redactar mensajes, cambiar indicativos, coordenadas y comentarios en el terreno.
  * **Monitor de Tráfico (Packet Sniffer) y Directorio de Estaciones:**
    * Cálculo automático de **distancia en kilómetros (fórmula de Haversine)** y **rumbo magnético/cardinal (`N`, `NE`, `E`, `SE`, `S`, `SO`, `O`, `NO`)** respecto a tu posición.
    * Toca cualquier estación recibida para responderle con un mensaje directo.
  * **Digipeater Fill-in `WIDE1-1` Opcional:** Activable desde el menú de configuración para repetir paquetes de estaciones portátiles cercanas.
  * **Perfiles preconfigurados para Baofeng y Yaesu:** Ajusta automáticamente el `TX Delay` (preámbulo de banderas `0x7E`), `TX Tail` y nivel de audio DAC, guardando todos los parámetros en memoria Flash no volátil (**NVS**).

---

## Arquitectura del Proyecto

```text
espcydaprs/
├── platformio.ini          # Configuración PlatformIO y flags de TFT_eSPI para 2432S024C
├── include/
│   ├── config.h            # Pinout de hardware, constantes DSP y estructura NVS
│   ├── cst820_touch.h      # Driver I2C para el panel táctil capacitivo CST820
│   ├── afsk_modem.h        # Módem DSP Bell 202 (1200/2200 Hz, NRZI, HDLC, CRC-16)
│   ├── ax25_aprs.h         # Codificador/Decodificador AX.25, parser APRS y GPS
│   └── ui_manager.h        # Interfaz gráfica táctil de 5 pestañas + teclado QWERTY
└── src/
    ├── main.cpp            # Inicialización dual-core y comandos de diagnóstico USB Serial
    ├── cst820_touch.cpp    # Implementación del controlador táctil CST820
    ├── afsk_modem.cpp      # Procesamiento digital de señales (Core 0 a 9600 Hz)
    ├── ax25_aprs.cpp       # Lógica APRS, mensajería con ACK, Haversine y NVS
    └── ui_manager.cpp      # Renderizado TFT_eSPI y eventos táctiles (Core 1)
```

---

## Pinout en la Placa `ESP32-2432S024C`

La placa **ESP32-2432S024C** ya tiene internamente cableados la pantalla SPI (`ILI9341`) y el touch capacitivo (`CST820`). Para la radio y el GPS utilizamos los pines libres expuestos en los conectores blancos laterales:

| Función | Pin ESP32 | Ubicación en la 2432S024C | Descripción |
| :--- | :---: | :--- | :--- |
| **Audio TX (hacia MIC Radio)** | **`GPIO 26`** | Conector P3 / Salida DAC2 | Señal senoidal AFSK generada por el DAC interno (8 bits). |
| **Audio RX (desde SPK Radio)** | **`GPIO 35`** | Conector P3 (`ADC1_CH7`) | Entrada de audio analógico polarizada a `1.65V DC`. |
| **Control PTT Radio** | **`GPIO 22`** | Conector CN1 / P3 | Salida digital (`3.3V` en TX, `0V` en RX) hacia transistor NPN. |
| **GPS RX (Desde TX del GPS)** | **`GPIO 17`** | Conector P3 / LED Verde | Entrada UART2 (`9600 bps`) para módulo GPS NMEA opcional. |
| **GPS TX (Hacia RX del GPS)** | **`GPIO 16`** | Conector P3 / LED Azul | Salida UART2 (`9600 bps`) hacia módulo GPS opcional. |
| **Indicador LED TX** | **`GPIO 4`** | LED RGB Trasero (Rojo) | Se enciende en rojo automáticamente mientras transmite (PTT activo). |
| **Tierra Común** | **`GND`** | Conectores P3 / CN1 | Referencia común entre la placa CYD y el chasis/masa del radio. |

> [!NOTE]
> Los pines de Pantalla (`MISO=12`, `MOSI=13`, `SCLK=14`, `CS=15`, `DC=2`, `BL=27`) y Touch Capacitivo (`SDA=33`, `SCL=32`, `RST=25`, `INT=21`) ya están preconfigurados en [`platformio.ini`](file:///e:/Github/espcydaprs/platformio.ini) y [`include/config.h`](file:///e:/Github/espcydaprs/include/config.h). No necesitas modificar librerías externas.

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

* NOTA PARA HANDIES YAESU (FT-60 / FT-65 / VX):
  En muchos handies Yaesu el PTT comparte la misma línea que el positivo de
  micrófono (MIC), activando el PTT cuando hay una resistencia de ~2.2 kΩ a masa.
  Ver diagrama específico para Yaesu más abajo.
```

---

### Conexión según el Modelo de Radio

#### Opción A: Baofeng (`UV-5R`, `UV-82`, `BF-888S`) y Kenwood (Conector K1 de 2 espigas)
El conector tipo Kenwood/Baofeng utiliza dos plugs: uno de **3.5 mm (arriba)** y uno de **2.5 mm (abajo)**:

* **Plug de 3.5 mm (Superior):**
  * **Punta (Tip):** Sin conexión (o salida de +5V en algunos modelos).
  * **Anillo (Ring):** **Entrada de Micrófono (`MIC`)** $\rightarrow$ Conectar a la salida de `C2` (TX Audio).
  * **Cuerpo (Sleeve):** **Tierra de Micrófono (`MIC GND`)** — ¡En los Baofeng **poner a tierra el cuerpo de 3.5 mm con el cuerpo de 2.5 mm activa el PTT**! Por lo tanto, el **Colector del transistor PTT** va al `Sleeve` de 3.5 mm.
* **Plug de 2.5 mm (Inferior):**
  * **Punta (Tip):** **Salida de Audio (`SPK`)** $\rightarrow$ Conectar a la entrada de `C1` (RX Audio).
  * **Cuerpo (Sleeve):** **Masa común (`GND`)** $\rightarrow$ Conectar a `GND` del ESP32 y al **Emisor** del transistor PTT.

> [!TIP]
> **Consejo para Baofeng UV-5R / UV-82:**
> 1. Desactiva el ahorro de batería (**Menu 3 `SAVE` = `OFF`**) para que el handy no pierda el inicio de los paquetes entrantes.
> 2. Pon el Squelch en nivel 1 (**Menu 0 `SQL` = `1`**) y desactiva el doble escucha (**Menu 7 `TDR` = `OFF`**).
> 3. En la pantalla `CONF` del ESP32 selecciona **`Radio Preset: BAOFENG`** (aplica `TX Delay = 450 ms`).

---

#### Opción B: Yaesu Portátiles (`FT-60R`, `FT-70D`, `VX-6`, `FT-4X` / `FT-65`)
Los handies Yaesu como el **FT-60R** usan un conector **TRRS de 3.5 mm de 4 contactos** (y los **FT-4X / FT-65** usan conector doble de 2.5 mm + 3.5 mm donde el PTT se activa por impedancia en la línea de MIC):

```text
Esquema PTT + MIC combinado para Handies Yaesu (línea MIC/PTT compartida):

                              Colector
                                 |
                                [R6] 2.2 kΩ
                                 |
Salida C2 (TX Audio) o-----------+--------------> Línea MIC/PTT del Handy Yaesu
                                 |
GPIO 22 o---[R5 2.2 kΩ]---| 2N2222 (NPN)
                                 |
                                GND (Sleeve)
```

* **En Yaesu FT-60R / VX (Plug 3.5 mm TRRS de 4 polos, desde la punta hacia el cable):**
  1. **Punta (Tip):** Audio de Salida (`CLONE / SPK`) $\rightarrow$ Entrada `C1` (`GPIO 35`).
  2. **Primer Anillo (Ring 1):** Audio de Altavoz (`SPK`) $\rightarrow$ (En el FT-60 el audio sale entre Tip/Ring1 según el adaptador CT-44).
  3. **Segundo Anillo (Ring 2):** **Entrada `MIC + PTT`** $\rightarrow$ Conectar al nodo combinado `C2 + R6 (2.2 kΩ al colector)`.
  4. **Cuerpo (Sleeve):** **Masa (`GND`)** $\rightarrow$ Conectar a `GND` del ESP32.
* **En Bases Móviles Yaesu (`FT-2980`, `FT-2900`, `FTM-3100` con conector RJ-45):**
  * El PTT es un pin dedicado separado del audio de micrófono (`PTT` directo al colector del transistor `2N2222` sin necesitar `R6`, y `MIC` directo a `C2`).
  * En la pestaña `CONF` del ESP32 selecciona **`Radio Preset: YAESU`** (`TX Delay = 260 ms`, `Nivel TX = 55%`).

---

## Conexión de Módulo GPS Externo (Opcional)

El sistema soporta tanto **Posición Fija** (ideal para estación base o digipeater) como **GPS en Tiempo Real** (para uso vehicular o trekking con un módulo `NEO-6M`, `NEO-8M` o `ATGM336H` a `9600 baudios`):

| Pin Módulo GPS | Pin `ESP32-2432S024C` |
| :--- | :--- |
| **`VCC`** | `3.3V` o `5V` (conector P3/CN1) |
| **`GND`** | `GND` |
| **`TX` (Salida NMEA del GPS)** | **`GPIO 17`** (`GPS_UART_RX_PIN`) |
| **`RX` (Entrada del GPS)** | **`GPIO 16`** (`GPS_UART_TX_PIN`) |

Para alternar entre coordenadas fijas y GPS:
1. Ve a la pestaña **`CONF`** (Página 1).
2. Toca el botón **`Modo: POSICION FIJA`** para cambiarlo a **`Modo: GPS EXTERNO`**.
3. En la barra superior verás **`GPS:BUSC`** (en color ámbar) mientras adquiere satélites y **`GPS:xSAT`** (en verde) en cuanto tenga posición válida.

---

## Guía de Uso en la Pantalla Táctil

La interfaz cuenta con **5 pestañas** en la barra inferior:

1. **`PANEL` (Dashboard Principal):**
   * Muestra tu indicativo (`LU4HNE-7`), perfil de radio activo (`BFG` o `YSU`), estado de posición (`POS:FIJA` o `GPS`), coordenadas actuales, ruta (`WIDE1-1,WIDE2-1`) y contadores de paquetes `TX`/`RX`.
   * **Barra `AUDIO RX` (Vúmetro en vivo):** Ajusta la perilla de volumen de tu handy Yaesu/Baofeng hasta que al recibir una señal el vúmetro se mantenga en la **zona verde (`30%` a `80%`)**.
   * **`[TX BALIZA]`:** Transmite inmediatamente tu posición APRS por radio VHF/UHF.
   * **`[LOOPBACK]`:** Genera la trama AX.25, la modula en AFSK 1200 baudios y la inyecta internamente al demodulador DSP. Verás cómo el contador `RX` sube y el paquete aparece decodificado en pantalla.
   * **`[AUTO: OFF / AUTO xxs]`:** Activa o desactiva el envío periódico automático de baliza.
2. **`MONIT` (Monitor de Tráfico AX.25):**
   * Lista en tiempo real de todos los paquetes decodificados con indicativo de origen, distancia en `km`, rumbo cardinal y comentario.
   * **Toca cualquier paquete** para abrir automáticamente la pantalla de mensajes con ese indicativo cargado como destinatario.
3. **`MSJS` (Mensajes APRS Bidireccionales):**
   * Toca el recuadro **`Para:`** o el **texto del mensaje** para abrir el **teclado táctil QWERTY** en pantalla.
   * Pulsa **`[ENVIAR RF]`** para transmitir el mensaje por VHF/UHF (mostrará estado `ESP ACK` y reintentará automáticamente hasta recibir el `ACK`, cambiando a `ACK OK` en verde).
   * Pulsa **`[LOOP]`** para probar el envío y recepción de mensajes en bucle local.
4. **`ESTAC` (Directorio de Estaciones Escuchadas):**
   * Lista de estaciones únicas escuchadas con su distancia (`km`), acimut en grados, coordenadas y cantidad de paquetes recibidos. Toca una estación para redactarle un mensaje.
5. **`CONF` (Configuración en 2 Páginas + Guardado en Flash NVS):**
   * **Página 1:** Indicativo, SSID (`-0` a `-15`), Modo Posición (`FIJA` / `GPS`), Símbolo APRS (`Handy`, `Auto`, `Casa`, `Camión`), Latitud fija, Longitud fija y Comentario de baliza.
   * **Página 2:** Selector de preset **`BAOFENG`** / **`YAESU`**, ajuste fino de `TX Delay (ms)`, `Nivel TX (%)`, intervalo de baliza (`60s` a `900s`), `Auto-ACK` de mensajes y modo repetidor **`Digipeater WIDE1-1`**.
   * Pulsa **`[GUARDAR]`** arriba a la derecha para almacenar los cambios permanentemente en la memoria Flash del ESP32.

---

## Compilación y Carga con PlatformIO

1. Abre esta carpeta en **VS Code** con la extensión **PlatformIO IDE** o desde la terminal.
2. Para compilar el proyecto:
   ```bash
   pio run
   ```
3. Conecta la placa **ESP32-2432S024C** por USB y sube el firmware:
   ```bash
   pio run -t upload
   ```
4. (Opcional) Abre el monitor serie a `115200 baudios` para ver las tramas APRS decodificadas en consola o enviar comandos de prueba (`b` = enviar baliza RF, `l` = prueba loopback, `s` = estado):
   ```bash
   pio device monitor
   ```

