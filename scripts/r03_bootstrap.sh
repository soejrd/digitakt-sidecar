#!/bin/sh
# Prepare a clean stock Digitakt 1.53 emulator session for R03.
# Requires the pinned digiemu source and a compatible (patched) Unicorn.
# The firmware stays outside this repository; set DT1_SYX to its lawful path.
set -eu
R=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
SYX=${DT1_SYX:?set DT1_SYX to a lawful Digitakt_OS1.53.syx}
ROOT=${R03_ROOT:-/tmp/r03-fw}
PY=${DIGIEMU_PYTHON:-python3}
mkdir -p "$ROOT"
cp "$SYX" "$ROOT/Digitakt_OS1.53.syx"
cd "$R/deps/digiemu"
PYTHONPATH=. DT2_SYX="$ROOT/Digitakt_OS1.53.syx" \
DT2_SECTIONS="$ROOT/sections" DT2_SNAPSHOTS="$ROOT/snapshots" \
DT2_PLUSDRIVE="$ROOT/plusdrive.img" DT2_MAIN_IMG="$ROOT/sections/section_3_MAIN_OS.bin" \
DT2_DEVICES="$PWD/devices" "$PY" - <<'PY'
import os
from emu import bootstrap
root = os.environ['DT2_SYX'].rsplit('/', 1)[0]
p = bootstrap.FirmwarePaths(root=root, syx_name='Digitakt_OS1.53.syx',
                            devices_dir=os.environ['DT2_DEVICES'])
os.environ.update(p.env())
print(bootstrap.first_run(p, progress=lambda e: print(e.step, e.kind, e.text, flush=True)
                          if e.kind in ('start', 'done', 'note') else None))
PY
