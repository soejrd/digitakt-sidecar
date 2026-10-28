#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/v01
"${CC:-cc}" -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Itests -Icontrol -Imods/usb-track -Ideps/elekloader/elekloader/sdk/include \
  tests/v01_diag_time.c mods/usb-track/src_queue.c \
  control/link.c control/device_link.c control/host_link.c control/circle_link_adapter.c \
  control/usbmidi.c control/sysex_split.c control/dt1_transport.c -o build/v01/diag_time
build/v01/diag_time
