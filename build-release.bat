@echo off
REM Build release firmware for ESP32-S3.
REM Uses IDF Python env at C:\Espressif\python_env\idf5.5_py3.11_env (set IDF_PYTHON_ENV_PATH if yours differs).
REM Run from any CMD that has Python and Git on PATH.

set "IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf5.5_py3.11_env"

echo Activating ESP-IDF...
call "C:\Espressif\frameworks\esp-idf-v5.5.2\export.bat"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cd /d "%~dp0"
echo Building release for ESP32-S3...
idf.py -DCMAKE_BUILD_TYPE=Release build
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
python "%~dp0merge_flash_bin.py"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo.
echo Release build done. Single image: build\vibe_pocket_file_server_flash.bin
echo Flash: esptool.py -p COMx write_flash 0x0 build\vibe_pocket_file_server_flash.bin
