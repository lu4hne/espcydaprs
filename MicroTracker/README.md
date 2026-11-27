# ESP32-C3 SuperMini — APRS MicroTracker (`v1.0.0`)

Complemento ultracompacto de la familia **espcydaprs** diseñado sobre una placa **ESP32-C3 SuperMini** (RISC-V 160 MHz, USB-C nativo, BLE 5.0 + WiFi), reutilizando el motor **AFSK Bell 202 1200 bps 100% entero** y **AX.25** con prioridad absoluta de recepción RF.

> ### 📱 Consola Móvil Web Bluetooth (Sin Instalar Apps)
> Conecta tu celular por Bluetooth BLE al MicroTracker para enviar/recibir mensajes APRS, disparar balizas o gestionar SOS directamente desde Chrome en Android:
> **👉 [https://lu4hne.github.io/espcydaprs/microtracker/](https://lu4hne.github.io/espcydaprs/microtracker/)**

---

## Características Principales

- **Envío de posición GPS por RF (1200 bps AFSK Bell 202 / AX.25):** soporte para intervalo fijo y **SmartBeaconing™** (ajusta dinámicamente la frecuencia de baliza según velocidad y giros en ruta, con posición fija de respaldo si no hay fix GPS).
- **Prioridad absoluta de recepción RF:** la tarea `AFSK_RX` corre a prioridad máxima (`10`) disparada por Hardware Timer 0 a `9600 Hz` con demodulador DSP 100% entero. Durante la operación normal el **WiFi permanece completamente apagado** para cero interferencia RF y mínima corriente.
- **Configuración por WiFi bajo demanda:** levanta un Portal Web AP (`MicroTracker-<INDICATIVO>` en `http://192.168.4.1`, potencia ajustada a `8.5 dBm` para la antena cerámica del SuperMini) para configurar Indicativo, SSID, Icono, Ruta, Comentario, SmartBeaconing, Preset Baofeng/Yaesu, TX Delay, Volumen, SOS y **Web OTA (`/update`)**. Se apaga automáticamente al guardar o tras 5 minutos de inactividad.
- **Consola Bluetooth BLE (Nordic UART Service — NUS):** se anuncia como `MT-APRS-<INDICATIVO>`. Permite envío y recepción de mensajes APRS bidireccionales con **auto-ACK y hasta 3 reintentos**, compatible con la **PWA Web Bluetooth** y con apps gratuitas como *Serial Bluetooth Terminal* (Android) o *Bluefruit Connect* (iOS).
- **Botón SOS por Hardware (`GPIO 4`):** mantener pulsado **3 segundos** activa la baliza oficial de emergencia APRS (`\!`) + mensaje directo opcional; mantener **3 segundos** nuevamente cancela y transmite la trama de situación normalizada.

---

## Pinout — ESP32-C3 SuperMini

El **ESP32-C3 SuperMini** utiliza USB CDC nativo en `GPIO 18/19`, lo que deja libres los pines `GPIO 20 (RX)` y `GPIO 21 (TX)` para el módulo GPS sin interferir con la consola USB ni la programación. Se evita además el pin de strapping `GPIO 2` en PTT y SOS para prevenir disparos falsos al encender.

```text
                ┌─────────────────────┐
                │     [ USB-C ]       │
                │  (GPIO 18 / 19)     │
   5V (VIN)  o──┤ 5V           GPIO 5 ├──o (Libre)
   GND       o──┤ GND          GPIO 6 ├──o (Libre)
   3.3V      o──┤ 3V3          GPIO 7 ├──o (Libre)
   SOS BTN   o──┤ GPIO 4       GPIO 8 ├──o [LED Azul Integrado - Activo LOW]
   PTT OUT   o──┤ GPIO 3       GPIO 9 ├──o [Botón BOOT Integrado]
   (Strapp.) o──┤ GPIO 2      GPIO 10 ├──o (Libre)
   AUDIO TX  o──┤ GPIO 1      GPIO 20 ├──o GPS RX (conectar a TX del GPS)
   AUDIO RX  o──┤ GPIO 0      GPIO 21 ├──o GPS TX (conectar a RX del GPS)
                └─────────────────────┘
```

| Función | Pin ESP32-C3 SuperMini | Tipo / Detalle | Conexión |
| :--- | :---: | :--- | :--- |
| **Audio RX** (desde radio) | **`GPIO 0`** | `ADC1_CH0` | Divisor `10k/10k` (`3.3V`–`GND`) + cap `1 µF` + `10 nF` desde SPK del handy |
| **Audio TX** (hacia radio) | **`GPIO 1`** | Sigma-Delta `312.5 kHz` | Atenuador/filtro `10k` / `1k` + `10–100 nF` + cap `1 µF` hacia MIC del handy |
| **Control PTT** | **`GPIO 3`** | Salida digital (`3.3V` TX) | Resistencia `2.2 kΩ` (o `1 kΩ`) a base de NPN `2N2222`/`BC547` (emisor `GND`, colector PTT) |
| **Botón SOS** | **`GPIO 4`** | `INPUT_PULLUP` | Pulsador momentáneo normalmente abierto entre `GPIO 4` y `GND` |
| **LED Azul Estado** | **`GPIO 8`** | Integrado (activo `LOW`) | Indicador visual de estado (TX, SOS, WiFi, GPS Fix) |
| **Botón BOOT** | **`GPIO 9`** | Integrado (activo `LOW`) | Toque corto = Baliza manual · Mantener 3 s = Abrir/Cerrar Portal WiFi |
| **GPS RX (`Serial1`)** | **`GPIO 20`** | UART1 RX (`9600 bps`) | Conectar al pin **`TX`** del módulo GPS NMEA (`NEO-6M/8M/ATGM336H`) |
| **GPS TX (`Serial1`)** | **`GPIO 21`** | UART1 TX (`9600 bps`) | Conectar al pin **`RX`** del módulo GPS NMEA (opcional) |
| **Alimentación** | **`5V` / `3V3` / `GND`** | USB-C o fuente externa | `5V` regulados (USB / powerbank / regulador 5V) y masa común con radio y GPS |

---

## Esquema Eléctrico Completo de Conexiones

### 1. Interfaz de Audio y PTT con el Handy (Baofeng / Yaesu / Kenwood)

```text
================================================================================
A. ENTRADA DE AUDIO RX (Salida Auricular / SPK del Handy ---> GPIO 0)
================================================================================

                  +3.3V (ESP32-C3)
                     |
                    [R1] 10 kΩ
                     |
SPK Radio o---||-----+----------+-----> GPIO 0 (ADC1_CH0)
             C1      |          |
            1 µF    [R2] 10 kΩ === C2 (10 nF cerámico "103")
                     |          |
                    GND        GND (Común)

* R1 y R2 (10 kΩ) polarizan la entrada en 1.65V DC (punto medio del ADC).
* C1 (1 µF) bloquea el nivel DC del amplificador de audio del handy.
* C2 (10 nF) junto con R1||R2 forma un filtro pasabajos en ~3.1 kHz que
  reduce el siseo de alta frecuencia al abrir el squelch.

================================================================================
B. SALIDA DE AUDIO TX (GPIO 1 Sigma-Delta ---> Entrada MIC del Handy)
================================================================================

                                            C3 (1 µF)
GPIO 1 (TX) o---[R3 10 kΩ]---+---+-------------||--------> Entrada MIC del Handy
                             |   |
                    [R4] 1 kΩ   === C4 (10 nF a 100 nF cerámico "103"/"104")
                             |   |
                            GND GND (Común)

* R3 (10 kΩ) y R4 (1 kΩ) atenúan ~10:1 los ~3.3 Vpp a ~250-300 mVpp (nivel de
  micrófono electret).
* R3 + C4 filtran la portadora Sigma-Delta de 312.5 kHz, entregando tonos
  senoidales limpios de 1200 Hz y 2200 Hz.
* C3 (1 µF) bloquea el voltaje fantasma (bias) del micrófono del handy.

================================================================================
C. CONTROL DE PTT (GPIO 3 ---> Línea PTT del Handy)
================================================================================

                               Colector
                                  |-------------> Pin PTT del Handy
GPIO 3 o-----[R5 2.2 kΩ]----|  2N2222 / BC547
                            |     |
                             \    v Emisor
                                  |
                                 GND (Común)

* En handies tipo Baofeng / Kenwood (conector doble K1 3.5 mm + 2.5 mm):
  - El PTT se dispara uniendo el "Sleeve" (masa) del plug de 2.5 mm con el
    "Sleeve" (masa) del plug de 3.5 mm mediante el transistor NPN (o colector
    a Sleeve 2.5 mm y emisor a GND común del plug 3.5 mm).
  - La entrada MIC está en el anillo ("Ring") del plug de 3.5 mm.
  - La salida SPK está en el anillo ("Ring") o punta ("Tip") del plug de 2.5 mm
    (verificar con multímetro según cable K1).
```

### 1B. Versión Mejorada de Filtros de Audio (Recomendada para PCB / Montaje Definitivo)

Para tonos TX más puros (sin atenuar `2200 Hz` y con rechazo >55 dB de la portadora Sigma-Delta de `312.5 kHz`), ajuste fino de nivel por preset y protección/filtrado de ruido en RX (100% compatible con el firmware actual en `ADC 12 dB / 0–3.3V`):

```text
================================================================================
RX MEJORADO (Vref Filtrada + Protección 1N4148 ---> GPIO 0)
================================================================================

  +3.3V o---[100 Ω]---+----+----+-----> Vref (~3.3V filtrado)
                      |    |    |
                     ===  ===  [10 kΩ]
                  100nF  22µF   |
                      |    |    +----|<|----+ (D1: 1N4148 cátodo a Vref)
                     GND  GND   |           |
  SPK Radio o---||--------------+-----------+---> GPIO 0 (ADC1_CH0, ~1.65V DC)
               4.7 µF           |           |
                             [10 kΩ]       =|= (D2: 1N4148 ánodo a GND)
                                |           |
                               GND         GND (Común)

* 100 Ω + 100 nF + 22 µF eliminan ruido de la fuente 3.3V/USB en el ADC.
* D1 y D2 (1N4148) protegen GPIO 0 contra picos altos del parlante.

================================================================================
TX MEJORADO (Filtro 2 Polos + Preset VR1K ---> Entrada MIC del Handy)
================================================================================

  GPIO 1 o---[1.5 kΩ]---+---[1.5 kΩ]---+---[R1 22–68 kΩ*]---+
                        |              |                    |
                       === 10 nF      === 10 nF           [VR1K] (Preset 1 kΩ)
                        |              |                   /  |---> [1 µF] ---> MIC
                       GND            GND                  |
                                                          GND (Común)

* *R1: 47–68 kΩ para Baofeng (micrófono muy sensible) o 15–22 kΩ para Yaesu.
* Mantener siempre el transistor NPN (2N2222/BC547) en PTT (GPIO 3).
```

### 2. Conexión del Módulo GPS NMEA y Botón SOS

```text
================================================================================
D. MÓDULO GPS (NEO-6M / NEO-7M / NEO-8M / ATGM336H — 9600 bps)
================================================================================

  Módulo GPS            ESP32-C3 SuperMini
  ──────────            ──────────────────
     VCC    ──────────►  3V3 (o 5V si el módulo tiene regulador 3.3V onboard)
     GND    ──────────►  GND
     TX     ──────────►  GPIO 20 (UART1 RX)
     RX     ──────────►  GPIO 21 (UART1 TX)  [Opcional]

================================================================================
E. BOTÓN DE EMERGENCIA SOS (Hardware)
================================================================================

  GPIO 4    ──────┬───── [ Pulsador Momentáneo NA ] ─────► GND
                  │
       (Pull-up interno activo en firmware; no requiere resistencia externa)
```

---

## Lista de Materiales (BOM)

| Cantidad | Componente | Uso |
| :---: | :--- | :--- |
| 1 | **ESP32-C3 SuperMini** | Microcontrolador principal (USB-C, BLE 5.0, WiFi) |
| 1 | **Módulo GPS NMEA** (`NEO-6M`, `NEO-8M` o `ATGM336H`) | Posición, velocidad, rumbo y altitud (`9600 bps`) |
| 3 | Resistencias **`10 kΩ`** (1/4 W) | `R1`, `R2` (polarización RX) y `R3` (atenuador TX) |
| 1 | Resistencia **`1 kΩ`** (1/4 W) | `R4` (atenuador TX) |
| 1 | Resistencia **`2.2 kΩ`** (o `1 kΩ`, 1/4 W) | `R5` (base transistor PTT) |
| 2 | Capacitores **`1 µF`** (cerámico o electrolítico) | `C1` (acople RX) y `C3` (acople TX) |
| 2 | Capacitores **`10 nF`** cerámicos (`103`) | `C2` (filtro RX) y `C4` (filtro TX; hasta `100 nF`) |
| 1 | Transistor NPN **`2N2222`** o **`BC547`** | Conmutación de PTT a masa |
| 1 | Pulsador momentáneo normalmente abierto | Botón físico de emergencia SOS (`GPIO 4` a `GND`) |
| 1 | Cable/ficha de audio para tu handy (K1 Baofeng o Yaesu) | Interfaz hacia el transceptor VHF/UHF |

---

## Patrones del LED Azul Integrado (`GPIO 8`)

| Patrón del LED | Significado |
| :--- | :--- |
| **Encendido fijo** | Transmitiendo paquete RF (`PTT` activo) |
| **Parpadeo muy rápido** (`~10 Hz`) | **Emergencia SOS activa** |
| **3 destellos rápidos** periódicos | **Portal Web WiFi AP activo** (`192.168.4.1`) |
| **1 destello breve cada 3 s** | Operación normal con **GPS Fix válido** |
| **2 destellos breves cada 3 s** | Operación normal **buscando satélites GPS** |

---

## Puesta en Marcha y Configuración

### 1. Primer Encendido (Portal WiFi Automático)
- Si la placa nunca fue configurada (indicativo por defecto `N0CALL`), **al arrancar abre automáticamente el Portal Web WiFi**:
  - **Red WiFi (AP):** `MicroTracker-N0CALL` (abierta, sin clave)
  - **Dirección Web:** `http://192.168.4.1`
- Desde el celular o PC configura tu **Indicativo**, **SSID** (`-9` móvil / `-7` portátil), **Símbolo**, **SmartBeaconing™** o intervalo fijo, **Preset de radio** (`BAOFENG` / `YAESU`), **Nivel TX**, y ajustes de **SOS**.
- Al presionar **Guardar**, el MicroTracker almacena todo en memoria permanente NVS y **apaga el WiFi automáticamente** para pasar a operación RF + BLE.
- Para volver a abrir el portal en cualquier momento: **mantén presionado el botón `BOOT` (`GPIO 9`) durante 3 segundos**, o envía `wifi` por la consola BLE/USB. El portal también incluye actualización **Web OTA** en `http://192.168.4.1/update`.

### 2. Consola Bluetooth BLE (Celular) y USB Serial (`115200 bps`)
El MicroTracker expone el servicio **Nordic UART Service (NUS)** bajo el nombre **`MT-APRS-<INDICATIVO>`**. Puedes conectarte usando:
- **Web App Oficial (PWA Web Bluetooth):** [https://lu4hne.github.io/espcydaprs/microtracker/](https://lu4hne.github.io/espcydaprs/microtracker/) (Chrome en Android / PC).
- **Apps nativas:** *Serial Bluetooth Terminal* (Android) o *Bluefruit Connect* (iOS).

#### Comandos Disponibles (BLE NUS y USB Serial)
- `msg <INDICATIVO> <texto>` — Envía un mensaje APRS directo con confirmación **ACK** y hasta 3 reintentos automáticos.
- `bcn` — Transmite una baliza de posición inmediatamente.
- `loop` — Ejecuta la autoprueba interna de loopback AX.25/AFSK.
- `status` — Muestra estado completo (GPS, satélites, coordenadas, velocidad, contadores TX/RX, nivel Audio RX, SOS y WiFi).
- `gpstest` — Prueba bidireccional del módulo GPS (envía consultas `UBX-MON-VER`, `$PCAS06` y `$PMTK605` por `GPIO 21` y verifica respuesta y tramas NMEA en `GPIO 20`).
- `gpsraw` — Muestra las tramas NMEA crudas del GPS en el monitor serie durante 5 segundos.
- `gpscmd <comando>` — Envía un comando NMEA personalizado al GPS (calcula el checksum `*XX` automáticamente si falta) y muestra la respuesta.
- `sos` / `sos off` — Activa o cancela el modo de emergencia SOS.
- `wifi` — Enciende o apaga el Portal Web WiFi de configuración.
- `help` — Lista los comandos disponibles.

### 3. Botón de Emergencia SOS (`GPIO 4`)
- **Activar:** mantener pulsado el botón SOS durante **3 segundos**. Emitirá de inmediato una baliza con símbolo oficial de emergencia **`\!`** (y el mensaje directo SOS si está habilitado) repitiéndola cada `sosIntervalSec` segundos.
- **Cancelar:** mantener pulsado el botón SOS nuevamente durante **3 segundos** (o enviar `sos off` por BLE). Transmitirá una baliza de normalización restaurando tu símbolo original.

---

## Compilación y Grabación con PlatformIO

```bash
cd MicroTracker
pio run
pio run -t upload
```

> **Tip ESP32-C3 SuperMini:** Si la PC no detecta el puerto USB o falla al sincronizar `esptool`, mantén presionado el botón **`BOOT`**, pulsa y suelta **`RST`** (o conecta el cable USB manteniendo `BOOT` presionado) y luego ejecuta `pio run -t upload`. Al finalizar, pulsa `RST` una vez para iniciar el firmware.
