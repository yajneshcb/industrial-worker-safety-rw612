@echo off
title NXP FRDM-RW612 USB Serial Telemetry Bridge
cd /d "%~dp0"

echo ======================================================================
echo   NXP FRDM-RW612 Worker Safety Monitor - USB Serial Bridge Launcher
echo ======================================================================
echo.

where python >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Python was not found in PATH. Please install Python 3.
    pause
    exit /b 1
)

echo [*] Checking dependencies...
python -c "import serial" >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [*] Installing pyserial...
    pip install pyserial
)

echo [*] Launching Serial Telemetry Bridge...
echo [*] Telemetry will automatically forward to:
echo     https://worker-safety-dashboard.vercel.app/api/telemetry
echo.
python serial_bridge.py %1

pause
