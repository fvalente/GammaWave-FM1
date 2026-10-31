@echo off
title QUANTUM GAMMAWAVE — M-VAVE FM-1 Firmware Flasher & Recovery
cd /d "%~dp0"
echo ============================================================================
echo   QUANTUM GAMMAWAVE (QG-1) — M-VAVE FM-1 Web Installer & Hardware Tools
echo ============================================================================
echo.
echo   Starting local secure web server for Web MIDI (SysEx enabled)...
echo   URL: http://localhost:8765/web/index_pkg.html
echo.
echo   Note: If your browser warns about Web MIDI permissions, click "Allow".
echo   Works on Microsoft Edge, Google Chrome, Brave, and Chromium-based browsers.
echo.
start "" "http://localhost:8765/web/index_pkg.html"
python -m http.server 8765
