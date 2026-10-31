#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# (QG-1) One-click FM-1 install (Linux / macOS): a private Python environment with mido + python-rtmidi, then
# tools/fm1_install.py. Usage: install-fm1.sh [PACKAGE.fwsc] [--verify-only] (default: build/felucca.fwsc).
set -e
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd)
PKG=${1:-$ROOT/build/felucca.fwsc}; [ $# -gt 0 ] && shift
[ -f "$PKG" ] || { echo "no package: $PKG (build it with ./build.sh or download felucca-*.fwsc)"; exit 2; }
VENV=${FM1_VENV:-$HOME/.fm1-installer}
PY=$(command -v python3 || command -v python) || { echo "needs Python 3 (python.org)"; exit 1; }
[ -x "$VENV/bin/python" ] || "$PY" -m venv "$VENV"
"$VENV/bin/python" -c "import mido, rtmidi" 2>/dev/null || "$VENV/bin/python" -m pip install -q mido python-rtmidi
"$VENV/bin/python" "$ROOT/tools/fm1_install.py" "$PKG" --verify-only
[ "$1" = "--verify-only" ] && exit 0
exec "$VENV/bin/python" "$ROOT/tools/fm1_install.py" "$PKG" "$@"
