#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/i04
"${CC:-cc}" -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Itests -Icontrol -Imods/usb-track -Ideps/elekloader/elekloader/sdk/include \
  tests/i04_effective_controls.c mods/usb-track/src_queue.c \
  control/link.c control/device_link.c control/host_link.c control/circle_link_adapter.c \
  control/usbmidi.c control/sysex_split.c control/dt1_transport.c \
  -o build/i04/effective_controls
build/i04/effective_controls
python3 - <<'PY'
from pathlib import Path
import json
r=json.loads(Path('evidence/i04/timing-trace.json').read_text())['rows']
text=str(r[0]['slack_frames'])+'\n'
text+=''.join(f"{x['index']} {x.get('sender_frames',0)} {x.get('rendered_frames',0)} {int(x['effective_engine_change'])}\n" for x in r)
Path('build/i04/control-timing.tsv').write_text(text)
PY
"${CC:-cc}" -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Icontrol -Iaudio -Ideps/mdkit/include -Ideps/mdkit/src/trx/b2 \
  tests/i04_timing_replay.c audio/playout.c audio/trx_voice.c audio/src_44148.c \
  deps/mdkit/src/trx/b2/mdkit_trx_b2.c -lm -o build/i04/timing_replay
build/i04/timing_replay build/i04/control-timing.tsv
