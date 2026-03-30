@echo off
REM Build-CarSide.bat
REM Builds the CarSide server using CMake

echo.
echo ========================================
echo Building CarSide Server
echo ========================================
echo.

REM Check if CMake is available
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [Error] CMake not found in PATH
    echo Please install CMake from: https://cmake.org/download/
    echo Or add it to your PATH
    pause
    exit /b 1
)

REM Create build directory if it doesn't exist
if not exist build mkdir build
cd build

REM Configure with CMake
echo [1/2] Configuring build...
cmake .. -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 (
    echo [Error] CMake configuration failed
    pause
    exit /b 1
)

REM Build
echo [2/2] Building...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [Error] Build failed
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build Complete!
echo ========================================
echo.
echo Executable: bin\Release\CarSideServer.exe
echo.
echo To run: .\bin\Release\CarSideServer.exe
echo.
pause