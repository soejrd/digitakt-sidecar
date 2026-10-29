#!/usr/bin/env python3
"""Replay local emulator patches at the pinned base, without checkout changes."""
import json
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
dependency = root / 'deps/digiemu'
base = next(r['revision'] for r in json.loads((root / 'sources.json').read_text())[
    'repositories'] if r['name'] == 'digiemu')
with tempfile.TemporaryDirectory(prefix='digiemu-replay-') as directory:
    archive = subprocess.Popen(['git', '-C', str(dependency), 'archive', base],
                               stdout=subprocess.PIPE)
    subprocess.run(['tar', '-xf', '-', '-C', directory], stdin=archive.stdout,
                   check=True)
    archive.stdout.close()
    assert archive.wait() == 0
    for patch in sorted((root / 'patches/digiemu').glob('*.patch')):
        subprocess.run(['git', 'apply', str(patch)], cwd=directory, check=True)
    changed = subprocess.check_output(
        ['git', '-C', str(dependency), 'diff', '--name-only', base], text=True).splitlines()
    for name in changed:
        assert (Path(directory) / name).read_bytes() == (dependency / name).read_bytes(), name
print('Emulator patches replay at pinned base; changed files match: PASS')
