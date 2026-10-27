#!/usr/bin/env bash
# Portable host/device handshake and silence-gating checks. No hardware, Circle
# link, emulator, synthesis library or network dependency.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/i03/cobj evidence/i03
CC=${CC:-cc}
CXX=${CXX:-c++}
CFLAGS=(-std=c99 -O2 -g -Wall -Wextra -Werror
        -fsanitize=address,undefined -fno-sanitize-recover=all -Icontrol)
CXXFLAGS=(-std=c++17 -O2 -g -Wall -Wextra -Werror
          -fsanitize=address,undefined -fno-sanitize-recover=all
          -Icontrol -Icontrol/circle_shim)

"$CC" "${CFLAGS[@]}" control/link.c tests/control_link.c \
  -o build/i03/control_link
"$CC" "${CFLAGS[@]}" control/link.c control/host_link.c \
  control/device_link.c control/circle_link_adapter.c tests/control_handshake.c \
  -o build/i03/control_handshake
for f in link host_link device_link circle_link_adapter circle_usbmidi; do
  "$CC" "${CFLAGS[@]}" -c "control/$f.c" -o "build/i03/cobj/$f.o"
done
"$CXX" "${CXXFLAGS[@]}" tests/control_circle_adapter.cpp \
  build/i03/cobj/*.o -o build/i03/control_circle_adapter

# When the pinned Circle headers are present, also compile the adapter test
# against Circle's real CUSBMIDIDevice declaration (the normal run uses the
# declarations-only shim so the check remains dependency-free).
circle_include=deps/sidecar-firmware/pi/circle/build/circle/include
if [ -f "$circle_include/circle/usb/usbmidi.h" ]; then
  "$CXX" -std=c++17 -Wall -Wextra -Werror -fsyntax-only \
    -Icontrol -I"$circle_include" tests/control_circle_adapter.cpp
  circle_header=passed
else
  circle_header=unavailable
fi

build/i03/control_link
build/i03/control_handshake
build/i03/control_circle_adapter

{
cat <<EOF
{
  "scope": "portable host/device state machine and Circle-shaped adapter; no hardware, Circle link, emulator, synthesis or USB transport",
  "wire": {"v1_snapshot_bytes": 61, "v2_control_bytes": 24,
           "framing": "explicit 7-bit little-endian; canonical top-group checks"},
  "pass": true,
  "circle_real_header_check": "$circle_header",
  "checks": [
    "signed extrema and malformed/truncated/noncanonical snapshots",
    "HELLO_ACK; START only after HELLO_ACK; install before ACTIVATE",
    "PCM gate stays closed until ACTIVATE is successfully queued",
    "device gate opens only after receiving the matching ACTIVATE",
    "ordered snapshots remain active; sequence gap/duplicate/overflow latch silence",
    "conflicting replay payload and wrong-block device keepalive are rejected",
    "bounded device replay-window overflow latches invalid",
    "activation/reply timeout and transport loss close the gate",
    "stale session and reconnect data are rejected; fresh session reactivates",
    "Circle wrapper sends only from task processing, receives chunked MIDI bytes",
    "Circle adapter compiles against the pinned real CUSBMIDIDevice declaration"
  ],
  "limits": [
    "No Digitakt firmware or native USB-MIDI send/receive is exercised.",
    "No physical USB congestion, DMA lifetime or disconnect behavior is measured.",
    "No synthesis or PCM is rendered; the gate is only state.",
    "Circle atomics are portable C11 atomics in this test, not a linked Circle build."
  ]
}
EOF
} > evidence/i03/handshake-portable.json
