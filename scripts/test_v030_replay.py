#!/usr/bin/env python3
"""Replay v0.3.0 DTSC patches and run the portable Output/session checks."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from prepare_v030_source import prepare, BASE, ROOT


def run():
    with tempfile.TemporaryDirectory(prefix='dtsc-v030-replay-') as folder:
        stage = Path(folder) / 'source'
        prepare(stage)
        engine = stage / 'pi/engine'
        overlay = engine / 'dt_control'
        flags = ['-O1', '-g', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined']
        includes = ['-I' + str(ROOT / 'tests/circle'), '-I' + str(overlay), '-I' + str(engine),
                    '-I' + str(stage / 'core')]
        objects = []
        for name in ('link.c', 'host_link.c', 'circle_link_adapter.c', 'trx_voice.c',
                     'src_44148.c', 'playout.c', 'mdkit_trx_b2.c'):
            obj = Path(folder) / (name + '.o')
            subprocess.run(['cc', '-std=c99', *flags, *includes, '-c', str(overlay / name),
                            '-o', str(obj)], check=True)
            objects.append(str(obj))
        obj = Path(folder) / 'out_measure.o'
        subprocess.run(['c++', '-std=c++17', *flags, *includes, '-c',
                        str(stage / 'core/fx/out_measure.cpp'), '-o', str(obj)], check=True)
        objects.append(str(obj))
        binary = Path(folder) / 'v030'
        subprocess.run(['c++', '-std=c++17', *flags, *includes,
                        str(ROOT / 'tests/i03_circle_glue.cpp'), str(engine / 'dt_link.cpp'),
                        str(engine / 'output.cpp'), *objects, '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], text=True, capture_output=True)
        print(result.stdout, end='')
        if result.returncode:
            print(result.stderr, end='')
            result.check_returncode()
        tests = stage / 'pi/engine/tests'
        subprocess.run(['make', '-C', str(tests), 'build/test_output'], check=True,
                       stdout=subprocess.DEVNULL)
        subprocess.run([str(tests / 'build/test_output')], check=True)
        evidence = {
            'scope': 'clean v0.3.0 archive replay plus portable DTSC/Output tests; no Pi or USB hardware',
            'dependency_base': BASE,
            'command': 'python3 scripts/test_v030_replay.py',
            'patches': [str(p.relative_to(ROOT)) for p in sorted((ROOT / 'patches/sidecar-firmware-v0.3.0').glob('*.patch'))],
            'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in sorted((ROOT / 'patches/sidecar-firmware-v0.3.0').glob('*.patch'))},
            'checks': [
                'clean v0.3.0 archive contains the target upgrade revision',
                '0001-0012 replay without the historical v0.2 stack',
                'actual Circle dt_link handshake/state/loss/reconnect/ACK congestion/H02 tone',
                'v0.3.0 Output counter and stereo/mixer tests',
            ],
            'cross_build': {
                'status': 'not-run',
                'required': 'Pi 4 AArch64 kernel cross-build and memory/stack inspection',
            },
            'pass': True,
            'limits': ['Desktop-only replay; no Pi kernel or hardware qualification.']
        }
        out = ROOT / 'evidence/upgrades/v0.3.0-replay.json'
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(evidence, indent=2) + '\n')


if __name__ == '__main__':
    run()
