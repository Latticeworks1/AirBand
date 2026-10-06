@echo off
rem Copies AirBand.vst3 into the system VST3 folder. Right-click this file and
rem choose "Run as administrator" (writing to Program Files needs it).
setlocal
set "SRC=%~dp0AirBand.vst3"
set "DEST=%CommonProgramFiles%\VST3\AirBand.vst3"
if not exist "%SRC%" (
    echo Could not find AirBand.vst3 next to this script. Unzip the whole archive first.
    pause
    exit /b 1
)
xcopy "%SRC%" "%DEST%\" /E /I /Y /Q >nul
if errorlevel 1 (
    echo Install failed. Right-click this file and choose "Run as administrator".
    pause
    exit /b 1
)
echo Installed to "%DEST%".
echo Rescan plugins in your DAW to find AirBand.
pause
