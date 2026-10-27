#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

CC_BIN="${CC:-cc}"
OUT_DIR="${OUT_DIR:-build/r05}"
mkdir -p "$OUT_DIR"

"$CC_BIN" -std=c99 -O2 -Wall -Wextra -Werror \
  -Ideps/mdkit/include -Ideps/mdkit/src/trx/b2 -Itools \
  tools/r05_bench.c tools/r05_resampler.c \
  deps/mdkit/src/trx/b2/mdkit_trx_b2.c -lm -o "$OUT_DIR/r05_bench"

"$OUT_DIR/r05_bench" evidence/r05/benchmark.json
