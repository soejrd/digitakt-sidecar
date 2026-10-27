#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/i03

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Imods/usb-track mods/usb-track/src_queue.c tests/src_queue.c \
  -o build/i03/src_queue
build/i03/src_queue

# Compile the real render hook against host I/O arrays. The production file is
# unchanged; the override macros redirect only its four hardware globals.
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -DUSB_TRACK_IO_OVERRIDES -include tests/usb_track_queue_io.h \
  -Imods/usb-track -Ideps/elekloader/elekloader/sdk/include \
  mods/usb-track/usb_track.c mods/usb-track/src_queue.c tests/usb_track_queue_host.c \
  -o build/i03/usb_track_queue_host
build/i03/usb_track_queue_host

# The producer is compiled separately, without sanitizer runtimes, so this is
# a check of its own imports rather than the test harness's.
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin \
  -Imods/usb-track -c mods/usb-track/src_queue.c -o build/i03/src_queue_host.o
if nm -u build/i03/src_queue_host.o | grep -E \
  '(^|[[:space:]_])(malloc|calloc|realloc|free|pthread_|sleep|usleep|nanosleep|sem_|mutex|printf|puts|write)(@|$)'; then
  echo "render queue has an allocator/blocking/stdio import" >&2
  exit 1
fi

if command -v m68k-elf-gcc >/dev/null 2>&1; then
  m68k-elf-gcc -mcpu=54455 -O2 -ffreestanding -fno-builtin -nostdlib \
    -fno-pic -fno-pie -fomit-frame-pointer -Wall -Wextra -Werror \
    -Imods/usb-track -c mods/usb-track/src_queue.c -o build/i03/src_queue_m68k.o
  if [ -n "$(m68k-elf-nm -u build/i03/src_queue_m68k.o)" ]; then
    echo "ColdFire render queue has undefined symbols" >&2
    m68k-elf-nm -u build/i03/src_queue_m68k.o >&2
    exit 1
  fi
  if m68k-elf-objdump -d build/i03/src_queue_m68k.o \
      | sed -n '/<ut_session_capture>:/,/^$/p' \
      | grep -Eq '[[:space:]](jsr|jbsr|bsr)([[:space:]]|$)'; then
    echo "ut_session_capture calls out; keep the render producer call-free" >&2
    exit 1
  fi

  # The render hook also calls machine-pages' cm_src_value; that function's
  # source is outside this queue object and is reported as the only render-side
  # external callback. The hook itself must not import allocators or waits.
  m68k-elf-gcc -mcpu=54455 -O2 -ffreestanding -fno-builtin -nostdlib \
    -fno-pic -fno-pie -fomit-frame-pointer -Wall -Wextra -Werror \
    -Imods/usb-track -Ideps/elekloader/elekloader/sdk/include \
    -c mods/usb-track/usb_track.c -o build/i03/usb_track_m68k.o
  if m68k-elf-nm -u build/i03/usb_track_m68k.o | grep -E \
    '(^|[[:space:]_])(malloc|calloc|realloc|free|pthread_|sleep|usleep|nanosleep|sem_|mutex|printf|puts|write)(@|$)'; then
    echo "render hook has an allocator/blocking/stdio import" >&2
    exit 1
  fi
  echo "ColdFire queue/render object: no allocator or wait imports: PASS"
else
  echo "ColdFire cross-check: skipped (m68k-elf-gcc not installed)"
fi
