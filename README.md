# ESP32 CYD (`ESP32-2432S024C` / `ESP32-2432S028R`) — Transceptor APRS 1200 bps / Tracker / Estación Fija + IGate + Estación Meteorológica WX (`v1.6.3`)

Aplicación completa de **codificación y decodificación APRS a 1200 baudios (AFSK Bell 202 / AX.25)** con **IGate APRS-IS bidireccional por WiFi**, **telemetría meteorológica (`BMP280 / BME280 / AHT20 / DHT22 / DHT11`)** y **actualización de firmware por MicroSD y Web OTA**, diseñada para la familia de placas **ESP32 Cheap Yellow Display (CYD)** tanto en sus variantes con controlador **`ILI9341`** como **`ST7789` (1-USB)** y paneles táctiles **`CST820` (I2C)** o **`XPT2046` (SPI dedicado o compartido)**.

Permite conectar directamente un handy o base **Yaesu** o **Baofeng** (en VHF `144.390 MHz` / `144.930 MHz` o UHF) para operar tanto en terreno como **Tracker autónomo con GPS** como en el shack como **Estación Fija + IGate (RF $\leftrightarrow$ APRS-IS)**, **Estación Meteorológica APRS WX** y **Digipeater Fill-in `WIDE1-1`**.

> ### 🌐 Instalador Web Online (Sin Instalar Programas ni Compilar)
> Puedes grabar tu placa CYD o actualizarla a la última versión directamente desde tu navegador (**Google Chrome** o **Microsoft Edge**) entrando a:
> **👉 [https://lu4hne.github.io/espcydaprs/](https://lu4hne.github.io/espcydaprs/)**
> 
> *Incluye grabación con 1 clic por WebSerial, actualización rápida conservando tu indicativo y monitor serie en vivo para ver paquetes.*

---

## Características Principales

* **3 Modos de Operación Configurables en Pantalla o vía Web (`CONF 1/7` o toque rápido en `PANEL`):**
  1. **`MODO TRACKER (Móvil / Portátil RF)`:**
     * Diseñado para operación vehicular o portátil con **GPS Externo NMEA** (`UART2`) o posición fija, SSID sugerido `-9` / `-7`, símbolo de vehículo (`/>`) o handy (`/[`).
     * Transmite balizas periódicas por RF (`1200 bps AFSK`) manteniendo el WiFi apagado para ahorrar batería.
     * **Acceso Exclusivo al Botón `[SOS]`:** En la barra inferior del panel se añade el botón de emergencia rápida.
  2. **`MODO ESTACIÓN FIJA + IGATE (RF <-> APRS-IS)`:**
     * Conecta por WiFi al servidor **APRS-IS** (por defecto `samerica.aprs2.net:14580` o `rotate.aprs2.net:14580`) con filtro activo de radio y mensajería (`filter r/lat/lon/100 g/CALL* b/CALL*`).
     * **Cálculo automático de Passcode APRS-IS** a partir de tu indicativo (`CALLSIGN`), con opción de ingreso manual.
     * **Inyección RF $\rightarrow$ APRS-IS (`qAR`):** Todo paquete AX.25/APRS válido decodificado desde el radio VHF/UHF se inyecta en tiempo real a la red mundial APRS-IS (visible en `aprs.fi` y `aprsdirect.com`).
     * **Mensajería Bidireccional APRS-IS $\leftrightarrow$ RF (`MSJS`):** Envía y recibe mensajes directos con confirmación **`ACK`**, auto-respuesta de `ACK` al recibir mensajes dirigidos a tu indicativo, botón de respuesta rápida tocando cualquier mensaje recibido y reintentos automáticos.
     * **Baliza de Estación Fija Configurable:** Permite emitir tu posición fija por **`RF + APRS-IS`**, **`SOLO APRS-IS (Internet)`** o **`SOLO RF`**.
  3. **`MODO ESTACIÓN FIJA SOLO RF / DIGIPEATER`:**
     * Ideal para estaciones base o repetidoras de campaña sin acceso a Internet, con soporte para **Digipeater Fill-in `WIDE1-1`**.
* **Sistema de Aviso de Emergencia APRS (`\!` — SOS / Auxilio Exclusivo en Modo Tracker):**
  * **Restricción Estricta de Modo:** Disponible **únicamente en `MODO TRACKER`** (el botón `[SOS]` permanece completamente oculto e inactivo en modo Estación Fija e IGate). Si se conmuta a otro modo con una emergencia activa, se cancela y normaliza automáticamente.
  * **Mecanismo Anti-Error de 3 Toques con Timeout:** Al presionar `[SOS]` se despliega un diálogo modal de advertencia que requiere tocar un botón de confirmación **3 veces consecutivas** dentro de una ventana de **5 segundos** (mostrada con barra regresiva). Si no se completan los 3 toques dentro del límite, el diálogo expira y se cierra sin transmitir nada.
  * **4 Categorías de Incidente:** Selector táctil para indicar el tipo de auxilio: **`SOS Gen`**, **`Médico`**, **`Vehículo`** o **`Extravío`**.
  * **Protocolo Oficial APRS 1.0.1 (`\!`):** Emite balizas periódicas con tabla `\` y símbolo `!` (**`\!`**), activando de inmediato el octágono rojo de emergencia en plataformas como `aprs.fi` y `APRS-IS`, incluyendo coordenadas GPS/fijas, altitud y comentario descriptivo del incidente.
  * **Cancelación Inmediata con Trama de Resolución:** Pantalla dedicada de alerta activa con botón destacado **`[CANCELAR EMERGENCIA]`**, que al confirmarse transmite instantáneamente una baliza de resolución (`[EMERGENCIA CANCELADA - SITUACION NORMALIZADA]`) restaurando el símbolo original del operador.
  * **Configuración Avanzada (`CONF 6/7: SOS / EMERGENCIA` y Portal Web):**
    * **Intervalo de Baliza SOS:** `120 s` por defecto (opciones: `60 s`, `120 s`, `180 s`).
    * **Mensaje Directo APRS SOS:** Opcional (`SI` / `NO`) con indicativo destinatario configurable (ej. para avisar directamente a tu estación base o grupo de rescate).
    * **Alerta Sonora Parlante (`GPIO 26`):** Opcional (`SI` / `NO`), emite una sirena de 2 tonos cada 15 segundos por el altavoz integrado (silenciosa por defecto para no agotar la batería).
* **Sensores Meteorológicos en Configuración (`CONF 5/7: SENSORES CLIMA` — `BMP280 / BME280 / AHT20 / DHT22 / DHT11`):**
  * Detección automática e instantánea de sensores **I2C (`BMP280`, `BME280`, `AHT10/AHT20`)** y **1-Wire (`DHT22`, `DHT11`)** sin librerías externas pesadas.
  * **Omisión Automática si no hay Sensor Conectado:** Si el sensor está desactivado o físicamente desconectado, **no se incluye ningún dato meteorológico en las balizas APRS** (la baliza sale limpia con tu comentario habitual).
  * **3 Modos de Inclusión en Baliza APRS:**
    1. **`COMENTARIO`:** Añade al final de tu comentario las lecturas disponibles (ej: `24.5C 48% 1013hPa` en `BME280` o `24.5C 1013hPa` en `BMP280`).
    2. **`WX OFICIAL (/_)`:** Formatea la baliza con el estándar meteorológico APRS (`_.../...g...t076h48b10132`) para que `aprs.fi` grafique las curvas de temperatura, humedad y presión atmosférica.
    3. **`NO INFORMAR`:** Muestra temperatura, humedad y presión únicamente en la pantalla (`PANEL` y `CONF 5/7`), sin transmitirlas en las balizas.
* **Control de Brillo de Pantalla TFT y LED Azul de Estado (`CONF 7/7: CREDITOS Y VERSION`):**
  * Ajuste de brillo de retroiluminación **TFT (`100%`, `65%`, `35%`, `15%`)** por PWM y control del **LED Azul (`TENUE`, `MEDIO`, `OFF`)** guardado en memoria permanente NVS.
* **Actualización de Firmware por MicroSD y OTA (`CONF 4/7`):**
  * **Auto-Actualización por MicroSD en Arranque:** Si insertas una tarjeta MicroSD con `/update.bin` o `/firmware.bin`, la placa lo detecta al encender, muestra la barra de progreso en pantalla, actualiza la Flash y renombra el archivo a `/update.bak` para evitar bucles.
  * **Escáner e Instalador MicroSD en Pantalla:** En `CONF 4/7` puedes pulsar **`[ESCANEAR SD]`** para listar archivos `.bin` en la tarjeta e instalarlos con un toque.
  * **Portal Web Local + Subida OTA por Navegador (`WebServer` + `ArduinoOTA`):** En modo IGate (o pulsando **`[PORTAL WEB / AP WIFI OTA]`** que levanta la red WiFi `CYD-APRS-<INDICATIVO>` en `http://192.168.4.1`), puedes configurar toda la estación o subir un archivo `firmware.bin` desde el celular o PC (`http://<IP>/update`).
  * **Actualización Remota HTTP/HTTPS (`OTA URL`):** Descarga directa desde URL `.bin` o servidor con manifiesto JSON y validación de versión semántica (`SemVer`).
* **Módem DSP AFSK 1200 bps (Bell 202) en Tiempo Real (`Core 0`):**
  * **Modulador TX:** Síntesis Digital Directa (DDS) de fase continua para tonos **Mark (`1200 Hz`)** y **Space (`2200 Hz`)** en `GPIO 26` (`DAC2`) y en el pin de expansión seleccionado (`IO4`, `IO27`, `IO21` o `IO17`) mediante modulador Sigma-Delta a `312.5 kHz`, con codificación NRZI, *bit-stuffing* HDLC, `CRC-16 CCITT` y control automático de PTT (`GPIO 22`).
  * **Demodulador RX Universal (Equipos Comerciales y Radiosondas RS41):** Muestreo ADC continuo (`GPIO 35`) a `9600 Hz`, filtro eliminador de continua (DC blocker), correlador en cuadratura óptimo (*Matched Filter* con detección de 1200/2200 Hz balanceada 1:1), lazo **DPLL**, compuerta de nivel de ruido (squelch digital), validación `CRC-16` y decodificación tanto de tramas estándar como **Comprimidas Base-91** y telemetría de radiosondas (`RS41ng`).
  * **Modo Test Loopback Integrado:** Prueba la codificación y decodificación AX.25/APRS directamente en la placa con un toque en pantalla.

---

## Arquitectura del Proyecto

```text
espcydaprs/
├── platformio.ini          # Configuración PlatformIO (min_spiffs.csv, TFT_eSPI, TinyGPS++, ArduinoJson)
├── include/
│   ├── config.h            # Pinout de hardware (Radio, GPS, Sensores, MicroSD), modos y estructura NVS
│   ├── cst820_touch.h      # Driver multi-touch auto-detectado (CST820 I2C + XPT2046 SPI dedicado/HSPI)
│   ├── afsk_modem.h        # Módem DSP Bell 202 (1200/2200 Hz, NRZI, HDLC, CRC-16)
│   ├── ax25_aprs.h         # Codificador/Decodificador AX.25, parser APRS, GPS, WX y modos
│   ├── sensor_manager.h    # Driver nativo I2C/1-Wire para BMP280, BME280, AHT10/20, DHT22 y DHT11
│   ├── igate_manager.h     # Cliente TCP APRS-IS (inyección qAR, Passcode hash, filtros IS<->RF)
│   ├── update_manager.h    # Actualizador por MicroSD, HTTP/HTTPS OTA y Portal Web / AP
│   └── ui_manager.h        # Interfaz gráfica táctil de 5 pestañas de radio + 7 páginas Config + QWERTY
└── src/
    ├── main.cpp            # Inicialización dual-core, PWM Backlight/LED, auto-update SD y comandos Serial
    ├── cst820_touch.cpp    # Implementación de controladores táctiles CST820 y XPT2046
    ├── afsk_modem.cpp      # Procesamiento digital de señales (Core 0 a 9600 Hz)
    ├── ax25_aprs.cpp       # Lógica APRS, telemetría WX, mensajería con ACK, Haversine y NVS
    ├── sensor_manager.cpp  # Lectura y calibración de sensores meteorológicos I2C / DHT
    ├── igate_manager.cpp   # Gestión WiFi, login APRS-IS e inyección/recepción de paquetes
    ├── update_manager.cpp  # Flasheo desde MicroSD (VSPI) y OTA Web
    └── ui_manager.cpp      # Renderizado TFT_eSPI y eventos táctiles (Core 1)
```

---

## Pinout en las Placas CYD (Modelos `ILI9341` y `ST7789`)

### ¿Por qué no aparece `IO26` en los conectores laterales de la CYD?
En todas las variantes de la familia **CYD (`ESP32-2432S028R`, `ESP32-2432S024C`, etc.)**, el pin **`GPIO 26` (`DAC2`)** **no sale a los conectores blancos de expansión (`P3` / `CN1`)** porque está cableado internamente en el PCB a la entrada (`pin 4 IN-`) del amplificador de audio integrado **`SC8002B` / `FM8002A`**, cuya salida va al conector blanco de 2 pines **`SPK` (`P4`)**.

Para resolver esto sin necesidad de soldar sobre el integrado, el firmware genera la onda senoidal AFSK (`1200 / 2200 Hz`) **simultáneamente por dos vías**:
1. **Por un pin libre de tus conectores blancos de expansión** (`IO4`, `IO27`, `IO21` o `IO17`, seleccionable en pantalla) usando el modulador hardware **Sigma-Delta DAC del ESP32 a `312.5 kHz`** (que al finalizar la transmisión vuelve a `HIGH` en reposo para cero ruido en recepción y sin encender el LED RGB activo en bajo).
2. **Por `GPIO 26` (`DAC2`)** hacia el conector `SPK` (por si en algún momento deseas tomar audio analógico de `IO26`).

---

### Tabla de Conexiones según tu Modelo de Placa CYD

Puedes cambiar entre ambos modelos en cualquier momento desde **`CONF` -> `PAG 2/5`**, manteniendo presionado el botón físico **`BOOT` durante 2 segundos**, enviando la letra **`d`** por el monitor serie USB o desde el **Portal Web**:

#### Modelo A: Placa CYD con Driver `ILI9341`
*(Conectores expuestos: `IO35`, `IO22`, `IO21`, `IO27`, `TX`, `RX`)*

| Función | Pin a Usar | Comentario / Ubicación |
| :--- | :---: | :--- |
| **Audio RX (desde SPK Radio)** | **`IO35`** | Conector `P3` (`ADC1_CH7` - Solo entrada analógica). |
| **Control PTT Radio** | **`IO22`** | Conector `CN1` / `P3` (Salida `3.3V` en TX, `0V` en RX). |
| **Audio TX (hacia MIC Radio)** | **`IO27`** *(o `IO21`)* | Conector `CN1` / `P3` (DAC Sigma-Delta `312.5 kHz`). Si tu placa usa `IO21` para el Backlight, usa **`IO27`** para TX (por defecto); si usa `IO27` para Backlight, selecciona **`IO21`** en `CONF 2/5`. |
| **Backlight Pantalla TFT** | **`IO21` / `IO27`** | El firmware controla por PWM únicamente el pin que corresponde al Backlight de la pantalla. |
| **Tierra Común** | **`GND`** | Conector `P3` / `CN1`. |

#### Modelo B: Placa CYD con Driver `ST7789` (1 solo USB)
*(Conectores expuestos: `IO35`, `IO22`, `IO21`, `IO4`, `IO16`, `IO17`, `TX`, `RX`)*

| Función | Pin a Usar | Comentario / Ubicación |
| :--- | :---: | :--- |
| **Audio RX (desde SPK Radio)** | **`IO35`** | Conector `P3` (`ADC1_CH7` - Solo entrada analógica). |
| **Control PTT Radio** | **`IO22`** | Conector `CN1` / `P3` (Salida `3.3V` en TX, `0V` en RX). |
| **Audio TX (hacia MIC Radio)** | **`IO21`** | Conector `CN1` / `P3` junto a `IO22` (DAC Sigma-Delta `312.5 kHz`). Al elegir `Placa: ST7789` se asigna **`IO21`** automáticamente, dejando `IO27` para el Backlight TFT y `IO16`/`IO17` libres en `P3` para **Sensores I2C/DHT** o **GPS**. |
| **Sensores I2C / DHT (o GPS)** | **`IO16` / `IO17`** | Conector `P3`: `SDA = IO16`, `SCL = IO17` (o `DATA = IO17` para `DHT22/11`), o bien `UART2` si se usa GPS externo en modo `Pos: GPS`. |
| **Backlight Pantalla TFT** | **`IO27`** | Iluminación de pantalla TFT con brillo regulable (`100% / 65% / 35% / 15%`) en `CONF 6/6`. |
| **MicroSD (`CS / SCK / MISO / MOSI`)** | **`5 / 18 / 19 / 23`** | Zócalo MicroSD integrado (Bus VSPI). |
| **Tierra Común** | **`GND`** | Conector `P3` / `CN1`. |

---

### Relación entre el LED RGB Trasero y el Conector `P3` (`IO4`, `IO16`, `IO17`)

En el diseño electrónico de fábrica de la placa CYD, el **LED RGB trasero** (activo en `LOW`, es decir, enciende cuando el pin baja a `0V` y se apaga en `3.3V`) está conectado **en paralelo** con los 3 pines del conector blanco de expansión **`P3`**:

| Pin del ESP32 | Color en el LED RGB (Activo en `LOW`) | Función en el Conector `P3` (`Placa: ST7789`) | Comportamiento en el Firmware |
| :---: | :---: | :--- | :--- |
| **`GPIO 4`** | **Rojo** | **LED Indicador de TX** | Permanece en `HIGH` (`3.3V`, apagado) en recepción y se enciende en **Rojo** únicamente mientras el equipo está transmitiendo por radio (`PTT` activo). |
| **`GPIO 16`** | **Verde** | **Sensor `SDA`** *(o GPS TX)* | En modo I2C mantiene pull-up a `3.3V` (LED Verde apagado; solo destella brevemente al leer el sensor cada 5 s). |
| **`GPIO 17`** | **Azul** | **Sensor `SCL` / `DHT`** *(o GPS RX)* | Si no usas sensores ni GPS en `IO17`, funciona como indicador de **IGate Conectado** con brillo regulable (`TENUE / MEDIO / OFF`) desde `CONF 6/6`. Si activas un sensor en `P3`, `IO17` pasa automáticamente a ser el reloj `SCL` (con pull-up a `3.3V`). |

---

### ¿Qué son los `ms` de Delay (`Dly: xxx ms` / `TX Delay`) en `CONF 2/6`?

Sí: el parámetro **`Dly: xxx ms` (`TX Delay`)** define el tiempo de **preámbulo desde que se activa el PTT (`GPIO 22 = HIGH`) hasta que comienza a enviarse la información real del paquete AX.25**:
1. Al iniciar una transmisión, el ESP32 levanta inmediatamente el **PTT (`IO22`)** para poner el handy en transmisión.
2. Durante los **`ms` configurados en `Dly`** (por ejemplo `450 ms` en el preset **`BAOFENG`** o `250 ms` en **`YAESU`**), el módem envía por **`IO21`** una ráfaga continua de **banderas de sincronización HDLC (`0x7E`, tonos alternados `1200/2200 Hz`)**.
3. Esto permite que:
   * Tu transmisor estabilice la portadora RF.
   * El receptor del otro corresponsal (o del IGate/Digipeater) alcance a abrir su *squelch*, despertar del ahorro de batería (*Battery Save* de los Baofeng) y sincronizar su reloj digital (DPLL) antes de que llegue la primera letra de tu indicativo.
4. Al terminar el paquete y su `CRC-16`, transmite `~60 ms` adicionales de cola (`TX Tail`) y suelta el PTT.

---

## Conexión de Sensores Meteorológicos (`Pestaña SENS`)

El firmware incluye soporte nativo para sensores de **Temperatura, Humedad y Presión Barométrica** desde la pestaña **`SENS`**:

### Sensores Soportados

| Selección en `SENS` | Sensores Compatibles | Magnitudes Medidas | Nota Técnica |
| :--- | :--- | :--- | :--- |
| **`AUTO I2C`** *(Recomendado)* | **`BME280`**, **`BMP280`**, **`AHT10 / AHT20`** | Temp + Humedad + Presión *(según chip)* | Escanea automáticamente el bus I2C en `0x76`, `0x77` y `0x38`. Si conectas un `BMP280` y un `AHT20` en paralelo sobre los mismos 4 cables, **combina ambos automáticamente** (Temp + Humedad del `AHT20` + Presión del `BMP280`). |
| **`BMP280 I2C`** | **`BMP280`** (`0x76` / `0x77`) | Temperatura (`°C`) + Presión (`hPa`) | El `BMP280` mide temperatura y presión barométrica; si deseas medir también humedad relativa, usa un **`BME280`** (mismo formato de 4 pines) o agrégale un **`AHT20`** en paralelo. |
| **`BME280 I2C`** | **`BME280`** (`0x76` / `0x77`) | Temperatura (`°C`) + Humedad (`%`) + Presión (`hPa`) | Estación meteorológica completa en un solo módulo I2C de 4 pines. |
| **`AHT10/20 I2C`** | **`AHT10`**, **`AHT20`**, **`AHT21`** (`0x38`) | Temperatura (`°C`) + Humedad (`%`) | Sensor digital I2C de alta precisión de temperatura y humedad. |
| **`DHT22 (1-WIRE)`** | **`DHT22 / AM2302`** | Temperatura (`°C`) + Humedad (`%`) | Sensor de 1 hilo conectado al segundo pin del perfil elegido (`IO17` o `IO22`). |
| **`DHT11 (1-WIRE)`** | **`DHT11`** | Temperatura (`°C`) + Humedad (`%`) | Sensor básico de 1 hilo conectado a `IO17` o `IO22`. |

### Cableado en el Conector Blanco `P3` (`Pines: P3 SDA=16 SCL=17`)

En la placa `ST7789` (en modo `Pos: FIJA`), el perfil por defecto es **`Pines: P3 SDA=16 SCL=17`**:

| Pin del Sensor (`BMP280 / BME280 / AHT20`) | Pin en la Placa CYD (`Conector P3 / CN1`) |
| :---: | :--- |
| **`VCC` / `VIN`** | **`3.3V`** |
| **`GND`** | **`GND`** |
| **`SDA`** | **`IO16`** |
| **`SCL`** *(o `DATA` en `DHT22/11`)* | **`IO17`** |

*(También puedes seleccionar en `SENS` los perfiles alternativos `CN1 SDA=21 SCL=22` o `CN1 SDA=27 SCL=22` según los pines libres de tu placa).*

### ¿Qué ocurre si el sensor no está conectado?
* Si seleccionas `Tipo: DESACTIVADO` o si el sensor **no está físicamente conectado** (estado `NO DETECTADO` en la pestaña `SENS`), **las balizas APRS no incluyen ningún dato meteorológico** y se transmiten únicamente con tu posición y comentario normal.

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
SPK Radio o---||-----+----------+-----> IO35 (ADC1_CH7)
             C1      |          |
            1 µF    [R2] 10 kΩ === C2 (10nf) 
                     |          |
                    GND        GND (Común)

* R1 y R2 (10 kΩ) crean un punto medio de 1.65V DC para que la onda de audio
  oscile entre 0V y 3.3V sin recortarse.
* C1 (1 µF cerámico o electrolítico) aísla el nivel DC del amplificador del radio.
* C2 (10 nF cerámico "103") en paralelo con R2 forma un filtro pasa-bajos pasivo
  con frecuencia de corte en ~3.1 kHz, eliminando el siseo y ruido blanco de alta frecuencia al abrir el squelch para una decodificación AFSK óptima y estable.

================================================================================
2. SALIDA DE AUDIO TX (IO4 / IO27 / IO21 / IO17 ---> Entrada MIC del Radio)
================================================================================

                                             C2 (1 µF)
Pin TX (IO4 o IO27) o---[R3 10 kΩ]---+---+------||--------> Entrada MIC del Radio
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

## Guía de Uso en la Pantalla Táctil (`5 Pestañas Inferiores + 6 Páginas de Configuración`)

La barra inferior mantiene **5 botones amplios dedicados a la operación de radio** y unifica todos los ajustes dentro de **`CONF`**:

1. **`PANEL` (Dashboard Principal de Radio):**
   * Muestra tu indicativo (`LU4HNE-7` / `LU4HNE-10`), el **Modo Activo** (`MODO: TRACKER RF`, `MODO: FIJA + IGATE` o `MODO: FIJA SOLO RF`), estado de posición (`POS:FIJA` o `GPS:xSAT`), estado de conexión **APRS-IS** (`IG:ON` en verde cuando está verificado) y contadores `TX`, `RX` e `IG`.
   * Si hay un sensor meteorológico conectado, muestra en vivo las lecturas de clima (`WX: 24.5C | 1013.2 hPa`).
   * **Cambio rápido de modo:** Toca la tarjeta superior izquierda en `PANEL` para alternar entre **Tracker**, **Estación Fija + IGate** y **Fija Solo RF**.
   * **Barra `AUDIO RX` (Vúmetro en vivo):** Ajusta el volumen de tu handy hasta que al recibir una señal el vúmetro se mantenga en la **zona verde (`30%` a `80%`)**.
   * **`[TX BALIZA]` / `[LOOPBACK]` / `[AUTO]`:** Transmite baliza inmediata (por RF y/o APRS-IS), ejecuta autodiagnóstico en bucle cerrado o activa el temporizador automático.
2. **`MONIT` (Monitor de Tráfico AX.25 / IGate):**
   * Lista en tiempo real de paquetes decodificados con distancia (`km`), rumbo cardinal y etiqueta verde **`[IGATE]`** cuando el paquete fue inyectado o recibido desde **APRS-IS**.
3. **`MSJS` (Mensajes APRS Bidireccionales por RF y APRS-IS):**
   * Redacta mensajes con el teclado táctil QWERTY, con confirmación automática **`ACK`** y reintentos.
   * **Respuesta con un toque:** Toca cualquier mensaje recibido en la lista para abrir directamente la ventana de respuesta hacia ese corresponsal.
   * **Autoprueba de ACK:** Si envías un mensaje a tu propio indicativo (`LU4HNE-10`), el sistema valida el ciclo completo y confirma `ACK OK` en verde.
4. **`ESTAC` (Directorio de Estaciones Escuchadas):**
   * Lista de estaciones únicas con distancia, acimut, coordenadas y cantidad de paquetes.
5. **`CONF` (Configuración Unificada en 6 Páginas + Guardado en Flash NVS):**
   * **Página 1 (`1/6: MODO Y ESTACION`):** Selector de Rol (`TRACKER MOVIL`, `ESTACION FIJA + IGATE`, `ESTACION FIJA SOLO RF`), Indicativo, SSID (`-0` a `-15`), Símbolo (`IGate I&`, `Handy`, `Auto`, `Casa`, `Camión`), fuente de posición (`FIJA` / `GPS`), Latitud, Longitud y Comentario.
   * **Página 2 (`2/6: RADIO Y BALIZA`):** Modelo de **`Placa: ILI9341 / ST7789`**, **`Pin TX`**, selector de preset **`BAOFENG`** / **`YAESU`**, `TX Delay (ms)`, `Nivel TX (%)`, intervalo de baliza (`60s` a `900s`), `Auto-ACK` y `Digipeater WIDE1-1`.
   * **Página 3 (`3/6: WIFI E IGATE`):** SSID y Clave WiFi (con teclado alfanumérico + símbolos `#?=`), servidor APRS-IS (`samerica.aprs2.net` / `rotate.aprs2.net`), `Passcode` (calculado automáticamente desde tu indicativo o manual), modo de baliza IGate (`RF + IS`, `SOLO IS`, `SOLO RF`) y botón **`[CONECTAR]`**.
   * **Página 4 (`4/6: SD Y WEB OTA`):**
     * **`[ESCANEAR SD]` / `[INSTALAR SD]`:** Busca e instala archivos `.bin` desde la tarjeta MicroSD con barra de progreso en pantalla.
     * **`[OTA URL]`:** Descarga e instala actualizaciones remotas por HTTP/HTTPS.
     * **`[PORTAL WEB / AP WIFI OTA]`:** Inicia el servidor web de configuración y actualización OTA en `http://<IP>` (o crea el punto de acceso WiFi `CYD-APRS-<INDICATIVO>` en `http://192.168.4.1`).
   * **Página 5 (`5/6: SENSORES CLIMA`):**
     * Estado en vivo de **Temperatura (`°C`)**, **Humedad (`%`)** y **Presión (`hPa`)**.
     * Selector de **`Tipo`** de sensor (`AUTO I2C`, `BMP/BME280`, `AHT10/20`, `DHT22`, `DHT11`, `DESACTIVADO`), **`Pines`** (`P3 SDA=16 SCL=17`, `CN1 21/22`, `CN1 27/22`), modo de envío en **`Baliza`** (`COMENTARIO`, `WX OFICIAL /_`, `NO INFORMAR`), calibración de **`Offset Temp`** (`-0.5°C` / `+0.5°C`) y botón **`[DETECTAR]`**.
     * Si el sensor no está conectado, las balizas APRS omiten automáticamente cualquier dato meteorológico.
   * **Página 6 (`6/6: CREDITOS Y VERSION` — Siempre la última página / About):**
     * Muestra créditos del proyecto (`LU4HNE`), versión instalada (`v1.5.2`), hardware activo (`Driver TFT`, `Touch`, `Pin Audio TX`, `IP`, `Passcode` y memoria RAM libre).
     * Incluye selectores de brillo del **LED Azul (`LED: TENUE / MEDIO / OFF`)**, brillo de pantalla **TFT (`TFT: 100% / 65% / 35% / 15%`)**, **`[GUARDAR]`** y **`[REINICIAR]`**.

---

## Demodulador DSP y Filtrado de Audio (1000 Hz a 2500 Hz)

El receptor AFSK a `1200 baudios` utiliza un **Filtro Adaptado en Cuadratura (*Matched Filter*)** a 9600 Hz con detección ortogonal ($I/Q$) sobre una ventana deslizante de 8 muestras:

1. **Rechazo Natural de Ruido Fuera de Banda (Filtro Pasabanda Intrínseco):**
   * El correlador DSP sintoniza por transformada exacta de Fourier las frecuencias centrales **`Mark (1200 Hz)`** y **`Space (2200 Hz)`**.
   * Las frecuencias fuera del rango útil son atenuadas naturalmente por el algoritmo:
     * Tonos sub-audibles CTCSS/PL (`67 Hz` a `250 Hz`): atenuación de **`-23 dB` a `-18 dB`**.
     * Ruido de conmutación y armónicos (`>3500 Hz`): atenuación superior a **`-15 dB`**.
   * Por esta razón **no es necesario un circuito activo complejo con amplificadores operacionales**.
2. **Filtro Pasabanda Pasivo Opcional de Entrada (Recomendado si hay mucho siseo):**
   * El circuito de entrada con capacitor $C_1$ (`1 µF`) y el divisor $R_1 / R_2$ (`10 kΩ` c/u) ya crea un filtro pasa-altos con corte en **`31.8 Hz`**.
   * Para limitar pasivamente las frecuencias altas y eliminar el siseo del receptor cuando abre el squelch, basta con soldar un pequeño **capacitor cerámico de `10 nF`** (marcado "103") o `22 nF` en paralelo con la resistencia $R_2$ (la que va a masa). Esto fija una frecuencia de corte superior en **`~3180 Hz`**, formando un filtro pasabanda pasivo completo sin componentes activos.
3. **Compuerta Digital de Ruido (Squelch DSP):**
   * Se evalúa el umbral mínimo de señal antes de alimentar transiciones al bucle DPLL y a la máquina de estados HDLC, impidiendo que el ruido blanco de canal genere falsas banderas (`0x7E`) que bloqueen la captura de balizas reales.
4. **Decodificación de Telemetría de Sondas (`WX`) y Coordenadas Comprimidas Base-91 (`ax25_aprs.cpp`):**
   * **Tramas Meteorológicas y de Radiosondas:** Reconoce y formatea automáticamente campos climáticos en paquetes de posición (`.../...g...t032h01b00000...` $\rightarrow$ `[0.0C 1%] RS41ng radiosonde firmware`), convirtiendo grados Fahrenheit a Celsius, humedad relativa (`%`) y presión barométrica (`hPa`).
   * **Filtro de Coordenadas Nulas (`0000.00S/00000.00W`):** Cuando una sonda o tracker transmite antes de adquirir satélites GPS (`Lat = 0.0, Lon = 0.0`), evita calcular una distancia falsa de `~6472 km` hacia el punto `(0°, 0°)`.
   * **Soporte Base-91 Comprimido (`/YYYYXXXX$csT`):** Decodifica latitud, longitud y altitud exponencial comprimida en Base-91, estándar muy utilizado en firmwares de radiosondas y trackers compactos.

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
3. Para actualizar por **Navegador Web (OTA)** o `curl`:
   * Abre `http://<IP_DE_LA_PLACA>/update` desde tu navegador y selecciona `.pio/build/esp32-2432s024c/firmware.bin`, o ejecuta:
     ```bash
     curl -F "update=@.pio/build/esp32-2432s024c/firmware.bin" http://<IP_DE_LA_PLACA>/update
     ```
4. Para actualizar por **MicroSD**:
   * Copia el archivo generado `.pio/build/esp32-2432s024c/firmware.bin` a la raíz de una tarjeta MicroSD FAT32 (como `/firmware.bin` o `/update.bin`).
   * Inserta la tarjeta en la placa CYD y reinicia (se instalará automáticamente al arrancar) o ve a **`CONF` -> `PAG 4/5` -> `[ESCANEAR SD]`**.
