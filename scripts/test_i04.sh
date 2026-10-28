#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/i04
flags=(-std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined
       -Icontrol -Iaudio -Itools -Ideps/mdkit/include -Ideps/mdkit/src/trx/b2)
"${CC:-cc}" "${flags[@]}" tests/i04_voice.c audio/trx_voice.c tools/r05_resampler.c \
  deps/mdkit/src/trx/b2/mdkit_trx_b2.c -lm -o build/i04/voice
build/i04/voice
bash scripts/test_i03_integration.sh
"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -DI04_RENDER_TEST -Itests/circle -Icontrol -Ideps/sidecar-firmware/pi/engine -Ideps/sidecar-firmware/core \
  tests/i03_circle_glue.cpp deps/sidecar-firmware/pi/engine/dt_link.cpp \
  deps/sidecar-firmware/pi/engine/output.cpp build/i03/glue_*.o -o build/i04/circle_glue
build/i04/circle_glue
