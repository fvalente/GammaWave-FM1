@echo off
rem SPDX-License-Identifier: GPL-3.0-only
rem (QG-1) One-click FM-1 install (Windows): a private Python environment with mido + python-rtmidi, then
rem tools\fm1_install.py. Usage: install-fm1.bat [PACKAGE.fwsc] [--verify-only]
setlocal
set "ROOT=%~dp0..\.."
set "PKG=%~1"
if "%PKG%"=="" set "PKG=%ROOT%\build\felucca.fwsc"
if not exist "%PKG%" (echo no package: %PKG% & pause & exit /b 2)
set "VENV=%USERPROFILE%\.fm1-installer"
if not exist "%VENV%\Scripts\python.exe" (py -3 -m venv "%VENV%" || python -m venv "%VENV%" || (echo needs Python 3 from python.org & pause & exit /b 1))
"%VENV%\Scripts\python.exe" -c "import mido, rtmidi" 2>nul || "%VENV%\Scripts\python.exe" -m pip install -q mido python-rtmidi
"%VENV%\Scripts\python.exe" "%ROOT%\tools\fm1_install.py" "%PKG%" --verify-only || (pause & exit /b 1)
if /i "%~2"=="--verify-only" (pause & exit /b 0)
"%VENV%\Scripts\python.exe" "%ROOT%\tools\fm1_install.py" "%PKG%"
pause
