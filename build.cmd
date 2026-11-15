@echo off
setlocal enabledelayedexpansion

:: Default configuration is Debug
set "CONFIG=Debug"

if /i "%~1"=="release" (
    set "CONFIG=Release"
) else if /i "%~1"=="-release" (
    set "CONFIG=Release"
) else if /i "%~1"=="--release" (
    set "CONFIG=Release"
) else if /i "%~1"=="/release" (
    set "CONFIG=Release"
) else if /i "%~1"=="debug" (
    set "CONFIG=Debug"
) else if /i "%~1"=="-debug" (
    set "CONFIG=Debug"
) else if /i "%~1"=="--debug" (
    set "CONFIG=Debug"
) else if /i "%~1"=="/debug" (
    set "CONFIG=Debug"
) else if not "%~1"=="" (
    echo [ERROR] Unknown configuration: "%~1"
    echo Usage: %~nx0 [debug^|release]
    exit /b 1
)

if /i "%CONFIG%"=="Release" (
    set "PRESET_CFG=release"
) else (
    set "PRESET_CFG=debug"
)

echo ========================================================
echo  Building ShiftSwitch via CMake (%CONFIG%)
echo ========================================================

echo.
echo --- [1/4] Configuring x64 (%CONFIG%) ---
cmake --preset x64-%PRESET_CFG%
if errorlevel 1 (
    echo [ERROR] CMake configure failed for x64-%PRESET_CFG%
    exit /b %errorlevel%
)

echo.
echo --- [2/4] Building x64 (%CONFIG%) ---
cmake --build --preset x64-%PRESET_CFG%
if errorlevel 1 (
    echo [ERROR] CMake build failed for x64-%PRESET_CFG%
    exit /b %errorlevel%
)

echo.
echo --- [3/4] Configuring x86 (%CONFIG%) ---
cmake --preset x86-%PRESET_CFG%
if errorlevel 1 (
    echo [ERROR] CMake configure failed for x86-%PRESET_CFG%
    exit /b %errorlevel%
)

echo.
echo --- [4/4] Building x86 (%CONFIG%) ---
cmake --build --preset x86-%PRESET_CFG%
if errorlevel 1 (
    echo [ERROR] CMake build failed for x86-%PRESET_CFG%
    exit /b %errorlevel%
)

echo.
echo ========================================================
echo  Build succeeded (%CONFIG%)!
echo  Binaries: %~dp0bin\%CONFIG%\
echo ========================================================

endlocal
exit /b 0
