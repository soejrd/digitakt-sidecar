#!/usr/bin/env bash
# Production firmware C + native ABI + portable Circle peer. No hardware.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/i03
rm -f build/i03/glue_*.o
CFLAGS=(-std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined
        -Itests -Icontrol -Imods/usb-track -Ideps/elekloader/elekloader/sdk/include)
TRANSPORT=(control/usbmidi.c control/sysex_split.c control/dt1_transport.c)
"${CC:-cc}" "${CFLAGS[@]}" tests/dt1_native.c "${TRANSPORT[@]}" -o build/i03/native
build/i03/native
"${CC:-cc}" "${CFLAGS[@]}" tests/i03_integrated.c mods/usb-track/src_queue.c \
  control/link.c control/device_link.c control/host_link.c control/circle_link_adapter.c \
  "${TRANSPORT[@]}" -o build/i03/integrated
build/i03/integrated
"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Ideps/sidecar-firmware/pi/engine -Ideps/sidecar-firmware/core \
  tests/i03_output_gate.cpp deps/sidecar-firmware/pi/engine/output.cpp \
  -o build/i03/output_gate
build/i03/output_gate
for unit in link host_link circle_link_adapter; do
  "${CC:-cc}" "${CFLAGS[@]}" -c "control/$unit.c" -o "build/i03/glue_$unit.o"
done
for unit in audio/trx_voice.c audio/src_44148.c audio/playout.c deps/mdkit/src/trx/b2/mdkit_trx_b2.c; do
  "${CC:-cc}" "${CFLAGS[@]}" -Iaudio -Itools -Ideps/mdkit/include -Ideps/mdkit/src/trx/b2 -c "$unit" -o "build/i03/glue_$(basename "$unit").o"
done
"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Itests/circle -Icontrol -Ideps/sidecar-firmware/pi/engine -Ideps/sidecar-firmware/core \
  tests/i03_circle_glue.cpp deps/sidecar-firmware/pi/engine/dt_link.cpp \
  deps/sidecar-firmware/pi/engine/output.cpp build/i03/glue_*.o -o build/i03/circle_glue
build/i03/circle_glue
