@echo off
call "C:\Espressif\frameworks\esp-idf-v5.5.3\export.bat" >nul 2>&1
cd /d "C:\Users\user\Documents\ESP_PROJECT"
echo.
echo ============================================
echo   ESP_PROJECT - Build
echo ============================================
echo.
idf.py build
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo BUILD FAILED [exit code %ERRORLEVEL%]
    pause
    exit /b %ERRORLEVEL%
)
echo.
echo BUILD SUCCEEDED
echo.
echo To flash: idf.py -p COM4 flash monitor
pause
