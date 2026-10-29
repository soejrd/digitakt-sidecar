#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/v01
"${CC:-cc}" -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Icontrol tests/v01_remote.c -o build/v01/remote_codec
build/v01/remote_codec
bash scripts/test_v01_diag.sh
V01_TEST_FLAGS=-DV01_REMOTE_DIAGNOSTIC bash scripts/test_v01_diag.sh

"${CC:-cc}" -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Icontrol tests/v01_compact.c control/link.c control/device_link.c -o build/v01/compact
build/v01/compact
V01_COMPACT_TEST=1 bash scripts/test_i04_event_worker.sh
