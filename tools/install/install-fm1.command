#!/bin/sh
# (QG-1) macOS: double-click in Finder. The same as install-fm1.sh.
cd "$(dirname "$0")" && sh ./install-fm1.sh "$@"; echo; printf "Press Enter to close. "; read _
