#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/x01 evidence/x01
includes=(-Itests -Icontrol -Iaudio -Imods/usb-track -Ideps/elekloader/elekloader/sdk/include
          -Ideps/mdkit/include -Ideps/mdkit/src/trx/b2)
sources=(control/link.c control/track_frame.c control/track_session.c control/track_host.c
         control/usbmidi.c control/sysex_split.c control/dt1_transport.c
         mods/usb-track/src_queue.c mods/usb-track/return_lanes.c
         audio/track_playout.c audio/trx_voice.c audio/src_44148.c deps/mdkit/src/trx/b2/mdkit_trx_b2.c)
"${CC:-cc}" -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  "${includes[@]}" tests/x01_transport.c "${sources[@]}" -lm -o build/x01/transport
build/x01/transport evidence/x01/native-transport.json
"${CC:-cc}" -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  "${includes[@]}" tests/x01_session.c "${sources[@]}" -lm -o build/x01/session
build/x01/session
for unit in mods/usb-track/x01_render.c mods/usb-track/x01_worker.c control/track_session.c control/track_frame.c; do
  m68k-elf-gcc -mcpu=54455 -O2 -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
    -Wall -Wextra -Werror "${includes[@]}" -c "$unit" -o "build/x01/$(basename "$unit").m68k.o"
  if m68k-elf-nm -u "build/x01/$(basename "$unit").m68k.o" | \
    rg '(^|[[:space:]_])(malloc|calloc|realloc|free|memcpy|memset|sleep|usleep|nanosleep|printf|puts)(@|$)'; then
    echo "unexpected runtime/allocator/wait import in X01 firmware object" >&2
    exit 1
  fi
done
echo "X01 ColdFire render/worker/protocol: no allocator/wait/libc imports: PASS"
