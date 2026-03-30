@echo off
REM Quick-Setup.bat - CarSide Windows Server Quick Setup
REM Run as Administrator

title CarSide Setup

echo.
echo ======================================
echo CarSide - Windows Server Setup
echo ======================================
echo.

REM Check for Administrator
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: This script must be run as Administrator
    echo Please right-click and select "Run as Administrator"
    pause
    exit /b 1
)

echo Step 1: Enable Test Signing (for unsigned drivers)
echo Run this ONCE and reboot:
echo.
echo   bcdedit /set testsigning on
echo   shutdown /r /t 0
echo.
pause

echo Step 2: Register IDD Virtual Display Driver
echo Run after reboot:
echo.
echo   pnputil /add-driver CarSideIdd.inf /install
echo.
pause

echo Step 3: Open Firewall Port 7878
echo Run as Administrator:
echo.
echo   netsh advfirewall firewall add rule name="CarSide" dir=in action=allow protocol=TCP localport=7878
echo.
echo Or run the PowerShell script for complete setup:
echo.
echo   powershell -ExecutionPolicy Bypass -File Setup-CarSide.ps1 -All
echo.
pause
