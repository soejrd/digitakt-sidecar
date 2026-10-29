#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/x01 evidence/x01
includes=(-Icontrol -Iaudio -Imods/usb-track -Ideps/mdkit/include -Ideps/mdkit/src/trx/b2)
sources=(control/link.c control/track_frame.c audio/track_playout.c audio/trx_voice.c
         audio/src_44148.c deps/mdkit/src/trx/b2/mdkit_trx_b2.c)
"${CC:-cc}" -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  "${includes[@]}" tests/x01_tracks.c mods/usb-track/return_lanes.c "${sources[@]}" \
  -lm -o build/x01/tracks
build/x01/tracks
"${CC:-cc}" -std=c99 -O2 -Wall -Wextra -Werror "${includes[@]}" \
  tools/x01_bench.c "${sources[@]}" -lm -o build/x01/bench
build/x01/bench evidence/x01/host-benchmark.json
python3 - <<'PY'
import json
from pathlib import Path
import platform
path = Path('evidence/x01/host-benchmark.json')
data = json.loads(path.read_text())
data['host_system'] = platform.system()
data['host_machine'] = platform.machine()
path.write_text(json.dumps(data, indent=2) + '\n')
PY

# Existing one-voice scheduler and real-engine behavior after scalar extraction.
for test in i04_voice i04_playout i04_velocity i04_final_controls; do
  "${CC:-cc}" -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    "${includes[@]}" "tests/$test.c" audio/trx_voice.c audio/src_44148.c audio/playout.c \
    deps/mdkit/src/trx/b2/mdkit_trx_b2.c -lm -o "build/x01/$test"
  "build/x01/$test"
done
