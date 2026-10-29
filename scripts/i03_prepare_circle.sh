#!/usr/bin/env bash
# Overlay only our device-neutral protocol sources. Apply patches 0001..0003
# to the pinned dependency first. No build, hardware access or network here.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p deps/sidecar-firmware/pi/engine/dt_control
for f in link.c link.h host_link.c host_link.h circle_link_adapter.c circle_link_adapter.h sc_memory.h h02_test_tone.h v01_diag.h; do
  cp "control/$f" "deps/sidecar-firmware/pi/engine/dt_control/$f"
done
