#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/i04 evidence/i04
flags=(-std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined
       -Icontrol -Iaudio -Itools -Ideps/mdkit/include -Ideps/mdkit/src/trx/b2)
"${CC:-cc}" "${flags[@]}" tests/i04_voice.c audio/trx_voice.c audio/src_44148.c audio/playout.c \
  deps/mdkit/src/trx/b2/mdkit_trx_b2.c -lm -o build/i04/voice
build/i04/voice
"${CC:-cc}" "${flags[@]}" tests/i04_playout.c audio/playout.c audio/trx_voice.c audio/src_44148.c \
  deps/mdkit/src/trx/b2/mdkit_trx_b2.c -lm -o build/i04/playout
build/i04/playout
"${CC:-cc}" "${flags[@]}" tests/i04_filter.c audio/src_44148.c -lm -o build/i04/filter
build/i04/filter evidence/i04/filter.json
bash scripts/test_i03_integration.sh
"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -DI04_RENDER_TEST -Itests/circle -Icontrol -Ideps/sidecar-firmware/pi/engine -Ideps/sidecar-firmware/core \
  tests/i03_circle_glue.cpp deps/sidecar-firmware/pi/engine/dt_link.cpp \
  deps/sidecar-firmware/pi/engine/output.cpp build/i03/glue_*.o -o build/i04/circle_glue
build/i04/circle_glue
"${CC:-cc}" -std=c99 -O2 -Wall -Wextra -Werror \
  -Icontrol -Iaudio -Ideps/mdkit/include -Ideps/mdkit/src/trx/b2 \
  tools/i04_bench.c audio/playout.c audio/trx_voice.c audio/src_44148.c \
  deps/mdkit/src/trx/b2/mdkit_trx_b2.c -lm -o build/i04/bench
build/i04/bench evidence/i04/playout-benchmark.json

bash scripts/test_i04_generation.sh
