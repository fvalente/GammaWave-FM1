@echo off
title QUANTUM GAMMAWAVE — Web Studio Workstation
cd /d "%~dp0"
echo ============================================================================
echo   QUANTUM GAMMAWAVE (QG-1) — FM-1 Web Studio Workstation
echo ============================================================================
echo.
echo   Starting local secure web server for Web MIDI & AudioWorklet...
echo   URL: http://localhost:8765/web/editor.html
echo.
echo   Note: Connect your M-VAVE FM-1 via USB, or use built-in Simulator offline!
echo.
start "" "http://localhost:8765/web/editor.html"
python -m http.server 8765
