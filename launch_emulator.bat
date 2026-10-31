@echo off
setlocal enabledelayedexpansion
title QUANTUM GAMMAWAVE — Live Browser Synthesizer & Groovebox Emulator
cd /d "%~dp0"

echo ============================================================================
echo   QUANTUM GAMMAWAVE (QG-1) — Browser Emulator (No hardware required)
echo ============================================================================
echo.

set "PY_CMD="
for %%C in (python py python3) do (
    if not defined PY_CMD (
        %%C --version >nul 2>&1
        if !errorlevel! equ 0 set "PY_CMD=%%C"
    )
)

if not defined PY_CMD (
    echo [ERROR] Python not found in PATH!
    echo Please install Python 3 (from https://www.python.org or Microsoft Store)
    echo and ensure "Add Python to PATH" is checked during installation.
    echo.
    pause
    exit /b 1
)

:: Check for emulator binary (felucca.wasm)
set "EMU_READY=0"
if exist "build\emu\felucca.wasm" (
    set "EMU_READY=1"
    set "EMU_URL=http://localhost:8765/build/emu/index.html"
) else if exist "web\emu\felucca.wasm" (
    set "EMU_READY=1"
    set "EMU_URL=http://localhost:8765/web/emu/index.html"
)

if "!EMU_READY!"=="0" (
    echo [NOTE] Precompiled WebAssembly binary 'felucca.wasm' not found locally.
    echo.
    echo If you have Emscripten (emcc) installed, compile it with:
    echo     sh web/emu/build.sh
    echo or run the development server with auto-rebuild:
    echo     python tools/emu_dev.py
    echo.
    echo Alternatively, test the online emulator at:
    echo     https://guillel51.github.io/Quantum-GammaWave/
    echo.
    echo Serving web directory so you can browse Web Studio and documentation...
    set "EMU_URL=http://localhost:8765/web/emu/index.html"
)

echo   Starting local web server for WebAssembly & AudioWorklet...
echo   URL: !EMU_URL!
echo.
start "" "!EMU_URL!"
%PY_CMD% -m http.server 8765
