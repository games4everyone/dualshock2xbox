@echo off
setlocal
title dualshock2xbox

REM double-click me. finds the exe, runs it, and keeps the window open on exit
REM so error messages don't vanish into the void.

set "EXE=dualshock2xbox.exe"
if not exist "%EXE%" set "EXE=build\Release\dualshock2xbox.exe"
if not exist "%EXE%" set "EXE=build\dualshock2xbox.exe"

if not exist "%EXE%" (
    echo Could not find dualshock2xbox.exe next to this file or in build\Release.
    echo Did you build it? See README.txt, section "Build".
    echo.
    pause
    exit /b 1
)

echo Plug in or pair your DualShock 4 / DualSense, then press a key.
pause >nul
echo Starting dualshock2xbox ; your pad now shows up as an Xbox 360 controller.
echo Close this window to stop.
echo.
"%EXE%" --check
if errorlevel 1 (
    echo.
    echo Something is missing ; the check above says what.
    echo If the driver is missing, run setup.ps1 first.
    echo.
    pause
    exit /b 1
)
"%EXE%" %*
echo.
echo dualshock2xbox stopped. Press any key to close this window.
pause >nul
