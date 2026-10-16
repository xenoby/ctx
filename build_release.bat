@echo off
setlocal enabledelayedexpansion

set "PATH=D:\programs\mingw64\bin;%PATH%"
set "LOG_FILE=%~dp0build.log"

echo ========================================= > "%LOG_FILE%"
echo RELEASE BUILDING LOG                      >> "%LOG_FILE%"
echo Date: %DATE%                              >> "%LOG_FILE%"
echo Time: %TIME%                              >> "%LOG_FILE%"
echo ========================================= >> "%LOG_FILE%"
echo. >> "%LOG_FILE%"

echo [INFO] Configuring and building Release...

cmake --preset mingw-release >> "%LOG_FILE%" 2>&1
cmake --build --preset release >> "%LOG_FILE%" 2>&1

if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Release build finished. Binary saved to 'bin/ctx.exe'
) else (
    echo [ERROR] Build failed. Check build.log for details.
    pause
)
