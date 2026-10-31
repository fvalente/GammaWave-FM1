@echo off
title QUANTUM GAMMAWAVE — Live Browser Synthesizer & Groovebox Emulator
cd /d "%~dp0"
echo ============================================================================
echo   QUANTUM GAMMAWAVE (QG-1) — Browser Emulator (No hardware required)
echo ============================================================================
echo.
echo   Starting local web server for WebAssembly & AudioWorklet...
echo   URL: http://localhost:8765/web/emu/index.html
echo.
start "" "http://localhost:8765/web/emu/index.html"
python -m http.server 8765
