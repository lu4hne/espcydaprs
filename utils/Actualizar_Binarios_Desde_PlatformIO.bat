@echo off
title Actualizar Binarios de Distribucion (ESP32 CYD APRS)
cd /d "%~dp0.."

echo ============================================================================
echo  1. Compilando proyecto con PlatformIO...
echo ============================================================================
pio run
if %ERRORLEVEL% NEQ 0 (
    echo ERROR al compilar con PlatformIO.
    pause
    exit /b 1
)

echo.
echo ============================================================================
echo  2. Generando binarios listos para grabar en utils\firmware\ y docs\firmware\...
echo ============================================================================
if not exist "utils\firmware" mkdir "utils\firmware"
if not exist "docs\firmware"  mkdir "docs\firmware"

copy /Y ".pio\build\esp32-2432s024c\firmware.bin" "utils\firmware\update.bin" >nul
copy /Y ".pio\build\esp32-2432s024c\firmware.bin" "docs\firmware\update.bin" >nul

"%USERPROFILE%\.platformio\packages\tool-esptool\esptool.exe" --chip esp32 merge-bin -o "utils\firmware\ESP32_CYD_APRS_FULL.bin" --flash-mode dio --flash-freq 40m --flash-size 4MB 0x1000 ".pio\build\esp32-2432s024c\bootloader.bin" 0x8000 ".pio\build\esp32-2432s024c\partitions.bin" 0xe000 "%USERPROFILE%\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin" 0x10000 ".pio\build\esp32-2432s024c\firmware.bin"

copy /Y "utils\firmware\ESP32_CYD_APRS_FULL.bin" "docs\firmware\ESP32_CYD_APRS_FULL.bin" >nul

echo.
echo ============================================================================
echo  LISTO! Binarios actualizados en utils\firmware\ y docs\firmware\:
echo    - ESP32_CYD_APRS_FULL.bin (Instalacion completa desde 0x0000)
echo    - update.bin              (MicroSD / Web OTA / Web Flasher 0x10000)
echo ============================================================================
pause
