@echo off
setlocal enabledelayedexpansion
title QUANTUM GAMMAWAVE — Web Studio Workstation
cd /d "%~dp0"

echo ============================================================================
echo   QUANTUM GAMMAWAVE (QG-1) — FM-1 Web Studio Workstation
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

echo   Starting local secure web server for Web MIDI & AudioWorklet...
echo   URL: http://localhost:8765/web/editor.html
echo.
echo   Note: Connect your M-VAVE FM-1 via USB, or use built-in Simulator offline!
echo.
start "" "http://localhost:8765/web/editor.html"
%PY_CMD% -m http.server 8765
