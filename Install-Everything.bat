@echo off
REM CarSide Installation Script
REM This batch file installs all required components for CarSide Windows server

echo ========================================
echo CarSide Installation Script
echo ========================================
echo.

REM Check for administrator privileges
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Administrator privileges confirmed.
) else (
    echo Please run this script as Administrator.
    pause
    exit /b 1
)

echo.
echo Step 1: Installing Visual Studio with C++ workload...
echo.

REM Download Visual Studio Installer if not present
if not exist "%TEMP%\vs_installer.exe" (
    echo Downloading Visual Studio Installer...
    powershell -Command "Invoke-WebRequest -Uri 'https://aka.ms/vs/17/release/vs_installer.exe' -OutFile '%TEMP%\vs_installer.exe'"
)

REM Install Visual Studio with Desktop development with C++ workload
echo Installing Visual Studio (this may take a while)...
"%TEMP%\vs_installer.exe" --quiet --wait --norestart --nocache ^
    --add Microsoft.VisualStudio.Workload.NativeDesktop ^
    --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ^
    --add Microsoft.Component.MSBuild ^
    --includeRecommended

if %errorLevel% neq 0 (
    echo Visual Studio installation failed.
    pause
    exit /b 1
)

echo Visual Studio installation completed.
echo.

echo Step 2: Installing Windows Driver Kit (WDK)...
echo.

REM Download WDK installer
if not exist "%TEMP%\wdksetup.exe" (
    echo Downloading WDK...
    powershell -Command "Invoke-WebRequest -Uri 'https://go.microsoft.com/fwlink/?linkid=2166289' -OutFile '%TEMP%\wdksetup.exe'"
)

REM Install WDK
echo Installing WDK (this may take a while)...
"%TEMP%\wdksetup.exe" /quiet /norestart

if %errorLevel% neq 0 (
    echo WDK installation failed.
    pause
    exit /b 1
)

echo WDK installation completed.
echo.

echo Step 3: Setting up CarSide components...
echo.

REM Run the PowerShell setup script
powershell -ExecutionPolicy Bypass -File "%~dp0Setup-CarSide.ps1"

if %errorLevel% neq 0 (
    echo PowerShell setup failed.
    pause
    exit /b 1
)

echo PowerShell setup completed.
echo.

echo Step 4: Building CarSide project...
echo.

REM Assume the project has a CarSide.sln file in the current directory
if exist "CarSide.sln" (
    REM Build using MSBuild (assuming it's in the default location)
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" CarSide.sln /p:Configuration=Release /p:Platform=x64 /verbosity:minimal
    
    if %errorLevel% neq 0 (
        echo Build failed.
        pause
        exit /b 1
    )
    
    echo Build completed successfully.
) else (
    echo CarSide.sln not found. Please ensure the project file is in the current directory.
    pause
    exit /b 1
)

echo.
echo ========================================
echo Installation completed successfully!
echo ========================================
echo.
echo To start the CarSide server:
echo 1. Ensure your iPad is connected to the same Wi-Fi network
echo 2. Run the CarSide server executable (ServerInitializer.exe or similar)
echo 3. On your iPad, the CarSide app should detect the server automatically
echo.
pause