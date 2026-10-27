#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/i03
"${CC:-cc}" -std=c99 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Icontrol control/link.c tests/control_link.c -o build/i03/control_link
build/i03/control_link
