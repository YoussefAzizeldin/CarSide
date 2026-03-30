@echo off
REM Install-Dependencies.bat
REM Downloads and installs required SDKs for CarSide

echo.
echo ========================================
echo CarSide Dependencies Installation
echo ========================================
echo.

setlocal enabledelayedexpansion

REM Create temp directory
if not exist "%TEMP%\CarSideSetup" mkdir "%TEMP%\CarSideSetup"
cd /d "%TEMP%\CarSideSetup"

echo [1] Visual Studio 2019+ with C++ Workload (REQUIRED)
echo.
echo Download from: https://visualstudio.microsoft.com/downloads/
echo In installer, select:
echo   - "Desktop development with C++"
echo   - Windows SDK (10.0.19041 or later)
echo.
echo Installation size: ~5-8 GB
echo.
pause

echo.
echo [2] Bonjour SDK (OPTIONAL - for mDNS service advertising)
echo.
echo Download from: https://support.apple.com/downloads/DL888/en_US/BonjourSDKforWindows.zip
echo This enables automatic service discovery on local network.
echo.
echo Press Enter to open Bonjour download page...
pause

REM Try to open the URL (Windows 10+)
start https://support.apple.com/downloads/DL888/en_US/BonjourSDKforWindows.zip

echo.
echo [3] Building the Project
echo.
echo After installing dependencies:
echo.
echo   1. Open CarSide.sln in Visual Studio
echo   2. Build Configuration: Release (x64)
echo   3. Build Solution (Ctrl+Shift+B)
echo.
echo Output: bin\Release\CarSideServer.exe
echo.
pause

echo.
echo [4] Next Steps
echo.
echo Run the setup script to configure Windows:
echo.
echo   powershell -ExecutionPolicy Bypass -File Setup-CarSide.ps1 -All
echo.
