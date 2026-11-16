@echo off
setlocal enabledelayedexpansion

set "ROOT=%~dp0"

if "%~1"=="/?" goto :help
if "%~1"=="-h" goto :help
if "%~1"=="--help" goto :help

echo ========================================================
echo  Cleaning ShiftSwitch project
echo  (Preserving 'bin' and 'installer' directories)
echo ========================================================
echo.

:: 1. Remove top-level build & intermediate directories
for /d %%D in ("%ROOT%build*") do (
    if exist "%%~fD\" (
        echo [DIR]  Removing %%~nxD...
        rd /s /q "%%~fD"
    )
)

for %%D in (obj lib out publish packages .vs) do (
    if exist "%ROOT%%%D\" (
        echo [DIR]  Removing %%D...
        rd /s /q "%ROOT%%%D"
    )
)

:: 2. Remove per-project intermediate build directories and temporary files
set "PROJECTS=ShiftSwitch ShiftSwitchCommon ShiftSwitchEnumLayouts ShiftSwitchUI ShiftSwitchUI2 tests"
for %%P in (%PROJECTS%) do (
    for %%D in (Debug Release x64 x86 ARM64 obj) do (
        if exist "%ROOT%%%P\%%D\" (
            echo [DIR]  Removing %%P\%%D...
            rd /s /q "%ROOT%%%P\%%D"
        )
    )
    if exist "%ROOT%%%P\*.aps" (
        echo [FILE] Removing %%P\*.aps...
        del /f /q "%ROOT%%%P\*.aps" 2>nul
    )
)

:: 3. Remove stray temporary and intermediate files from root
for %%E in (obj aps VC.db VC.opendb ipch tlog idb tmp) do (
    if exist "%ROOT%*.%%E" (
        echo [FILE] Removing *.%%E from project root...
        del /f /q "%ROOT%*.%%E" 2>nul
    )
)

if exist "%ROOT%crash_*.dmp" (
    echo [FILE] Removing crash dumps...
    del /f /q "%ROOT%crash_*.dmp" 2>nul
)

if exist "%ROOT%shiftswitch_*.log" (
    echo [FILE] Removing debug logs...
    del /f /q "%ROOT%shiftswitch_*.log" 2>nul
)

echo.
echo ========================================================
echo  Clean completed successfully!
echo ========================================================
endlocal
exit /b 0

:help
echo Usage: %~nx0
echo.
echo Cleans all temporary build artifacts, intermediate directories,
echo and cache files from the project.
echo.
echo Note: 'bin' and 'installer' directories are preserved.
exit /b 0
