#!/usr/bin/env bash
# I03 USB-MIDI transport adapter: host tests and the OS 1.54 device build.
# Needs only a C compiler; the build/lint half needs the m68k toolchain and a
# private stock OS 1.54 file (set DT154_SYX). No hardware, no emulator.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/i03
CC="${CC:-cc}"
CFLAGS="-std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer"
SRC="control/usbmidi.c control/sysex_split.c control/dt1_usbmidi_model.c control/dt1_transport.c"
"$CC" $CFLAGS -Icontrol $SRC tests/dt1_transport.c -o build/i03/dt1_transport
build/i03/dt1_transport

STOCK="${DT154_SYX:-private/i02-fixture/Digitakt_OS1.54.syx}"
if ! command -v "${ELEKLOADER_CROSS:-m68k-elf-}gcc" >/dev/null 2>&1 || [ ! -f "$STOCK" ]; then
  echo "device build skipped: need the m68k toolchain and a stock OS 1.54 file"
  exit 0
fi
OUT="${I03_BUILD_OUT:-$(mktemp -d)}"
export PYTHONPATH="deps/elekloader${PYTHONPATH:+:$PYTHONPATH}"
export ELEKLOADER_CROSS="${ELEKLOADER_CROSS:-m68k-elf-}"
python3 -m elekloader.sdk.build deps/elekloader/mods/core --stock "$STOCK" --out "$OUT"
python3 -m elekloader.sdk.build mods/usb-midi --stock "$STOCK" --out "$OUT"
python3 -m elekloader.lint "$OUT/usb-midi-0.1.elemod" --stock "$STOCK" \
  --with "$OUT/core-4.1-os1.54.elemod" --json \
  | python3 -c 'import json,sys; d=json.load(sys.stdin); assert d["ok"], d; print("device build + lint: PASS")'
