# Grabador Fácil de Firmware — ESP32 CYD APRS (`Sin Saber Programar`)

Esta carpeta (`utils/`) contiene todo lo necesario para que **cualquier radioaficionado o principiante** pueda instalar o actualizar el firmware **ESP32 CYD APRS** en su placa **`ESP32-2432S024C` / `ESP32-2432S028R`** en menos de 1 minuto, **sin tener que instalar VS Code, ni PlatformIO, ni compilar código**.

---

## ¿Qué incluye esta carpeta?

```text
utils/
├── Grabar_Firmware_GUI.bat               # OPCIÓN 1 (Windows): Ventana gráfica con 1 clic
├── Grabador_Web_Chrome_Edge.html         # OPCIÓN 2 (Chrome/Edge): Grabador directo desde el navegador
├── flasher_gui.ps1                       # Motor de la ventana gráfica para Windows
├── Actualizar_Binarios_Desde_PlatformIO.bat # (Solo desarrolladores) Regenera los .bin tras compilar
└── firmware/
    ├── ESP32_CYD_APRS_FULL.bin           # Imagen completa de fábrica (0x0000): Bootloader + Particiones + App
    └── update.bin                        # Imagen de actualización (0x10000) para MicroSD, Web OTA o USB rápido
```

---

## Cómo Grabar tu Placa por Primera Vez (Elige cualquiera de las 3 opciones)

### Opción 1: Grabador Gráfico para Windows (Recomendado)

1. Conecta tu placa **ESP32 CYD** a la computadora con un cable USB de datos.
2. Haz doble clic en **`Grabar_Firmware_GUI.bat`**.
3. Se abrirá la ventana del instalador:
   * **Paso 1:** Verifica que aparezca seleccionado el puerto **`COM`** de tu placa (ej. `COM3 - USB-SERIAL CH340`). Si acabas de enchufarla, pulsa **`[Buscar Puertos]`**.
   * **Paso 2:**
     * Si tu placa es **nueva (primera vez)**, deja marcada la opción **"Instalación Completa desde Cero (Offset 0x0000)"**.
     * Si ya tenías una versión anterior y **no quieres perder tu Indicativo ni tu clave WiFi**, elige **"Solo Actualizar Firmware (Offset 0x10000)"**.
   * **Paso 3:** Haz clic en el botón verde **`[GRABAR FIRMWARE EN LA PLACA (USB)]`**.
4. Espera unos 25 segundos hasta que la barra llegue al **100%** y aparezca el cartel **`¡Grabación Exitosa!`**.

> [!TIP]
> Si es la primera vez que lo usas en una PC nueva sin herramientas instaladas, el programa descargará automáticamente la herramienta oficial `esptool.exe` de Espressif en la subcarpeta `utils/bin/`.

---

### Opción 2: Grabador Web desde Google Chrome o Microsoft Edge (Windows / Mac / Linux)

Si prefieres grabar directamente desde el navegador web sin ejecutar scripts:

1. Haz doble clic en **`Grabador_Web_Chrome_Edge.html`** para abrirlo en **Google Chrome** o **Microsoft Edge**.
2. En el **Paso 1**, haz clic en **`Seleccionar archivo`** y elige:
   * `utils/firmware/ESP32_CYD_APRS_FULL.bin` (si dejas marcado **Instalación Completa `0x0000`**), o
   * `utils/firmware/update.bin` (si eliges **Solo Actualizar `0x10000`**).
3. Haz clic en **`CONECTAR PUERTO USB Y GRABAR FIRMWARE`**, selecciona el puerto `COM` de tu placa en la ventanita del navegador y espera a que la barra llegue al 100%.

---

### Opción 3: Actualizar sin cables usando una Tarjeta MicroSD (Si ya tienes el firmware instalado)

Si tu placa ya tiene instalado el firmware `ESP32 CYD APRS` y quieres actualizarla a una nueva versión sin conectarla a la PC:

1. Inserta una tarjeta **MicroSD (formateada en FAT32)** en tu PC.
2. Copia el archivo **`utils/firmware/update.bin`** a la raíz de la tarjeta MicroSD (o usa el botón naranja **`[Copiar update.bin a MicroSD]`** dentro de `Grabar_Firmware_GUI.bat`).
3. Coloca la tarjeta MicroSD en la ranura de la placa CYD y enciéndela:
   * La placa detectará automáticamente `/update.bin` al arrancar, mostrará una barra de progreso en su propia pantalla táctil, se actualizará sola y renombrará el archivo a `/update.bak`.

---

## Preguntas Frecuentes para Principiantes

* **¿Qué hago si al terminar de grabar la pantalla queda blanca o con colores invertidos?**
  * Existen dos variantes de fábrica de la placa CYD (`ILI9341` y `ST7789`). Simplemente **mantén presionado el botón físico `BOOT` (en la parte trasera de la placa) durante 2 segundos** y la placa cambiará automáticamente al otro controlador de pantalla y lo guardará en memoria.
* **¿Qué hago si no aparece ningún puerto `COM` al conectar la placa?**
  * Asegúrate de usar un **cable USB con transferencia de datos** (algunos cables de carga de auriculares solo tienen 2 hilos de energía y no pasan datos).
  * Si en el Administrador de Dispositivos de Windows aparece `USB2.0-Serial` con un signo de exclamación amarillo, instala el driver gratuito **CH340** del fabricante WCH.
* **¿Qué hago si se queda en `Connecting....` al intentar grabar?**
  * Mantén presionado el botón **`BOOT`** de la placa durante 2 segundos mientras aparece `Connecting...` y luego suéltalo.
