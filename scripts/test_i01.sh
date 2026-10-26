#!/bin/sh
# I01 checks: the directional Overbridge layout codec and the R02 fixture.
# These need only a C++17 compiler; no hardware, firmware, Circle or network.
set -eu
R=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
CXX=${CXX:-c++}
T=${TMPDIR:-/tmp}/i01-tests.$$
mkdir -p "$T"
trap 'rm -rf "$T"' EXIT

INC="$R/deps/sidecar-firmware/teensy/libraries/RytmHost"
"$CXX" -std=c++17 -Wall -Wextra -I"$INC" \
  "$R/deps/sidecar-firmware/teensy/libraries/RytmHost/extras/ob_block_test.cpp" \
  -o "$T/ob_block_test"
"$T/ob_block_test"

"$CXX" -std=c++17 -Wall -Wextra -I"$INC" "$R/tests/test_usb_layout_policy.cpp" -o "$T/layout_policy"
"$T/layout_policy" "$R/evidence/r02-usb-descriptors-2026-10-26.txt"

"$CXX" -std=c++17 -Wall -Wextra -I"$INC" "$R/tests/test_digitakt_layout.cpp" -o "$T/digitakt_layout"
"$T/digitakt_layout" "$R/evidence/r02-endpoints-2026-10-26.json"

echo "I01 checks pass"
