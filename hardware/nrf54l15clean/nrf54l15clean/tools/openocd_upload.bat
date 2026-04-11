@echo off
setlocal

set "OPENOCD_BIN=%~1"
set "OPENOCD_SCRIPT=%~2"
set "OPENOCD_SPEED=%~3"
set "HEX_PATH=%~4"

"%OPENOCD_BIN%" -f "%OPENOCD_SCRIPT%" -c "adapter speed %OPENOCD_SPEED%" -c "program {%HEX_PATH%} reset" -c "shutdown"
set "RC=%ERRORLEVEL%"
endlocal & exit /b %RC%
