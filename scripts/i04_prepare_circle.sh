#!/usr/bin/env bash
# Apply patches 0001..0006 at sources.json's base first. Offline overlay only.
set -euo pipefail
cd "$(dirname "$0")/.."
test "$(git -C deps/mdkit rev-parse HEAD)" = 96157fe4fbda17bf4cae3168f52437d8232e6570
git -C deps/mdkit diff --quiet -- include/mdkit.h include/mdkit_voice.h src/trx/b2/mdkit_trx_b2.c src/trx/b2/mdkit_trx_b2.h
bash scripts/i03_prepare_circle.sh
out=deps/sidecar-firmware/pi/engine/dt_control
cp audio/trx_voice.{c,h} tools/r05_resampler.{c,h} "$out/"
for f in include/mdkit.h include/mdkit_voice.h src/trx/b2/mdkit_trx_b2.c src/trx/b2/mdkit_trx_b2.h; do
  cp "deps/mdkit/$f" "$out/"
done
