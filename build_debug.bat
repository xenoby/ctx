@echo off
setlocal enabledelayedexpansion

set "PATH=D:\programs\mingw64\bin;%PATH%"
set "LOG_FILE=%~dp0build.log"

echo ========================================= > "%LOG_FILE%"
echo DEBUG BUILDING LOG                        >> "%LOG_FILE%"
echo Date: %DATE%                              >> "%LOG_FILE%"
echo Time: %TIME%                              >> "%LOG_FILE%"
echo ========================================= >> "%LOG_FILE%"
echo. >> "%LOG_FILE%"

echo [INFO] Configuring and building Debug...

cmake --preset mingw-debug >> "%LOG_FILE%" 2>&1
cmake --build --preset debug >> "%LOG_FILE%" 2>&1

if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Debug build finished. Binary saved to 'bin/ctx_debug.exe'
) else (
    echo [ERROR] Build failed. Check build.log for details.
    pause
)
