#!/usr/bin/env python3
"""Replay pinned host patches in a disposable tree and execute actual Pi glue."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from x01_prepare_host import prepare, ROOT


def run():
    with tempfile.TemporaryDirectory(prefix='x01-host-') as folder:
        stage = Path(folder)/'source'
        prepare(stage)
        engine = stage/'pi/engine'
        overlay = engine/'dt_control'
        flags = ['-O1', '-g', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined']
        includes = ['-I'+str(overlay), '-I'+str(engine), '-I'+str(stage/'core')]
        objects = []
        for name in ('link.c', 'track_frame.c', 'track_session.c', 'track_host.c',
                     'track_playout.c', 'trx_voice.c', 'src_44148.c', 'mdkit_trx_b2.c'):
            obj = Path(folder)/(name+'.o')
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', *flags, *includes,
                            '-c', str(overlay/name), '-o', str(obj)], check=True)
            objects.append(str(obj))
        binary = Path(folder)/'circle'
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', *flags, *includes,
                        str(ROOT/'tests/x01_circle.cpp'), str(engine/'dt_link.cpp'),
                        str(engine/'output.cpp'), *objects, '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], text=True, capture_output=True)
        print(result.stdout, end='')
        if result.returncode:
            print(result.stderr, end='')
            result.check_returncode()
        evidence = {'scope': 'actual opt-in Circle glue and patched Output on desktop; no Pi or USB hardware',
                    'dependency_base': '6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457',
                    'command': 'python3 scripts/test_x01_host.py',
                    'checks': ['patch replay from pinned base', 'explicit v5/v4 capability mapping',
                               'session-bound manual arm', 'real dual-engine PCM',
                               'independent Output L/R with unrelated columns discarded',
                               'asynchronous loss and stale FIFO epoch rejection',
                               'retired session isolation', 'ACK queue refusal closes output'],
                    'source_sha256': {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
                                      for name in ['host/x01_dt_link.cpp',
                                                   'patches/sidecar-firmware/experimental/0001-x01-stereo-return.patch']},
                    'pass': True}
        out = ROOT/'evidence/x01/host-integration.json'
        out.write_text(json.dumps(evidence, indent=2)+'\n')


if __name__ == '__main__':
    run()
