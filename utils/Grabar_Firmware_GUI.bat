@echo off
title Grabador de Firmware ESP32 CYD APRS - LU4HNE
cd /d "%~dp0"
echo Iniciando interfaz grafica del Grabador de Firmware ESP32 CYD APRS...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0flasher_gui.ps1"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Hubo un problema al abrir la ventana grafica.
    pause
)
