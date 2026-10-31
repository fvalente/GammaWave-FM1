#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Download JieLi's Linux toolchain (clang 4.0.1 for pi32v2) and link it as DEST/toolchain.
#   tools/get_toolchain.sh [DEST]      (default: ~/.jieli)
#
# (QG-1 stage 3) The archive is pinned by SHA-256 when JIELI_TOOLCHAIN_SHA256 is set (the CI reads it from
# the repository variable of that name): a mirror that serves something else is refused before anything is
# unpacked. Without it the download still works, and the hash it got is printed so it can be reviewed and
# pinned — the URL serves the latest build, so a pin only holds until JieLi replaces it upstream (then the
# job fails loudly instead of silently building with a compiler nobody chose).
set -e
DEST="${1:-$HOME/.jieli}"
mkdir -p "$DEST"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
curl -fL --retry 3 -o "$TMP/toolchain.tar.xz" https://pkgman.jieliapp.com/s/linux-toolchain
sha() { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi; }
GOT="$(sha "$TMP/toolchain.tar.xz")"
if [ -n "${JIELI_TOOLCHAIN_SHA256:-}" ]; then
    if [ "$GOT" != "$JIELI_TOOLCHAIN_SHA256" ]; then
        echo "error: the toolchain archive is sha256 $GOT, the pinned value is $JIELI_TOOLCHAIN_SHA256." >&2
        echo "       Nothing was unpacked. Either JieLi replaced the build upstream (review it, then update the" >&2
        echo "       repository variable) or the download did not come from them." >&2
        exit 1
    fi
    echo "toolchain archive: sha256 $GOT (pinned, verified)"
else
    echo "toolchain archive: sha256 $GOT (not pinned: set JIELI_TOOLCHAIN_SHA256 to this value once reviewed)"
fi
tar -xJ -f "$TMP/toolchain.tar.xz" -C "$DEST"
TC="$(ls -d "$DEST"/jieli-linux-toolchains-* | sort | tail -1)"
ln -sfn "$(basename "$TC")" "$DEST/toolchain"
echo "JIELI_TOOLCHAIN=$DEST/toolchain ($(basename "$TC"))"
