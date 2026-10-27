#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

python3 -m unittest tests.test_dynamic_probe

if [[ "${I03_RUN_DYNAMIC:-0}" == "1" ]]; then
  : "${PY:?set PY to the Python 3.12 interpreter with the pinned emulator dependencies}"
  PYTHONPATH="$PWD/private/r03-runtime:$PWD/deps/digiemu${PYTHONPATH:+:$PYTHONPATH}" \
    "$PY" tools/i03_dynamic_probe.py \
      --candidate private/i03-final \
      --output evidence/i03/dynamic-154.json \
      "$@"
else
  echo "I03 dynamic probe unit checks PASS; set I03_RUN_DYNAMIC=1 to run the emulator probe"
fi
