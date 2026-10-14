@echo off
setlocal enabledelayedexpansion

echo ======================================================
echo  Building AutoShieldX Pico 2 Firmware (RP2350)
echo ======================================================

:: 1. Locate Visual Studio / MSVC vcvars64.bat
set "VCVARS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" (
            set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
        )
    )
)

if not defined VCVARS (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    )
)

if defined VCVARS (
    echo [INFO] Initializing MSVC host environment: "%VCVARS%"
    call "%VCVARS%" >nul 2>&1
) else (
    echo [WARN] vcvars64.bat not found, continuing with default PATH...
)

:: 2. Clear CC and CXX to prevent toolchain contamination
set "CC="
set "CXX="

:: 3. Paths Configuration
set "PROJECT_DIR=%~dp0"
set "BUILD_DIR=%PROJECT_DIR%build_ninja"
set "PICO_SDK_PATH=C:\Users\deepa\Documents\pico-sdk"
set "ARM_TOOLCHAIN_BIN=C:\Program Files\Arm\GNU Toolchain mingw-w64-x86_64-arm-none-eabi\bin"
set "PICOTOOL_DIR=C:\Users\deepa\Documents\picotool\lib\cmake\picotool"

if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
)

echo [INFO] Configuring CMake with Ninja...
cmake -G Ninja ^
    -S "%PROJECT_DIR%." ^
    -B "%BUILD_DIR%" ^
    -DPICO_BOARD=pico2 ^
    -DPICO_SDK_PATH="%PICO_SDK_PATH%" ^
    -DPICO_TOOLCHAIN_PATH="%ARM_TOOLCHAIN_BIN%" ^
    -Dpicotool_DIR="%PICOTOOL_DIR%"

if errorlevel 1 (
    echo [ERROR] CMake configuration failed!
    exit /b 1
)

echo [INFO] Building with Ninja...
ninja -C "%BUILD_DIR%"

if errorlevel 1 (
    echo [ERROR] Ninja build failed!
    exit /b 1
)

echo.
echo ======================================================
echo  BUILD SUCCESSFUL!
echo ======================================================
echo Output UF2: %BUILD_DIR%\autoshieldx_pico2_ecu.uf2
echo Output ELF: %BUILD_DIR%\autoshieldx_pico2_ecu.elf
echo Output BIN: %BUILD_DIR%\autoshieldx_pico2_ecu.bin
echo Output HEX: %BUILD_DIR%\autoshieldx_pico2_ecu.hex
echo ======================================================

exit /b 0
