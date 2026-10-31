@echo off
setlocal enabledelayedexpansion
title QUANTUM GAMMAWAVE — M-VAVE FM-1 Firmware Flasher & Recovery
cd /d "%~dp0"

echo ============================================================================
echo   QUANTUM GAMMAWAVE (QG-1) — M-VAVE FM-1 Web Installer & Hardware Tools
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

echo   Starting local secure web server for Web MIDI (SysEx enabled)...
echo   URL: http://localhost:8765/web/index_pkg.html
echo.
echo   Note: If your browser warns about Web MIDI permissions, click "Allow".
echo   Works on Microsoft Edge, Google Chrome, Brave, and Chromium-based browsers.
echo.
start "" "http://localhost:8765/web/index_pkg.html"
%PY_CMD% -m http.server 8765
