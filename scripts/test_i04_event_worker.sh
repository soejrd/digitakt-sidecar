#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/i04
EXTRA_FLAGS=(-DSC_PLAYOUT_DELAY=128u)
if [[ "${V01_COMPACT_TEST:-0}" == 1 ]]; then EXTRA_FLAGS+=(-DV01_COMPACT_SNAPSHOTS); fi
"${CC:-cc}" -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  "${EXTRA_FLAGS[@]}" -Itests -Icontrol -Iaudio -Imods/usb-track -Ideps/elekloader/elekloader/sdk/include \
  -Ideps/mdkit/include -Ideps/mdkit/src/trx/b2 \
  tests/i04_event_worker.c mods/usb-track/src_queue.c \
  control/link.c control/device_link.c control/host_link.c control/circle_link_adapter.c \
  control/usbmidi.c control/sysex_split.c control/dt1_transport.c \
  audio/playout.c audio/trx_voice.c audio/src_44148.c deps/mdkit/src/trx/b2/mdkit_trx_b2.c \
  -lm -o build/i04/event_worker
if [[ "${1:-}" == legacy ]]; then
  build/i04/event_worker legacy
else
  build/i04/event_worker event 1
  build/i04/event_worker event 3
  build/i04/event_worker event 1 starvation
fi
