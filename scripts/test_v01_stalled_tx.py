#!/usr/bin/env python3
"""Replay the one-voice TX stall against old and current actual worker sources."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASELINE = 'a8e63fa'
SOURCES = ['tests/v01_stalled_tx.c', 'mods/usb-track/src_queue.c',
           'control/link.c', 'control/device_link.c', 'control/host_link.c',
           'control/circle_link_adapter.c', 'control/usbmidi.c',
           'control/sysex_split.c', 'control/dt1_transport.c']


def run(root, binary):
    subprocess.run(['cc', '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-Itests', '-Icontrol', '-Iaudio',
                    '-Imods/usb-track', '-Ideps/elekloader/elekloader/sdk/include',
                    *SOURCES, '-o', str(binary)], cwd=root, check=True)
    return [subprocess.run([str(binary), *mode], cwd=root, capture_output=True, text=True)
            for mode in ([], ['wrap'])]


def main():
    with tempfile.TemporaryDirectory(prefix='v01-stalled-tx-') as directory:
        stage = Path(directory)/'before'; stage.mkdir()
        archive = subprocess.Popen(['git', 'archive', BASELINE], cwd=ROOT, stdout=subprocess.PIPE)
        subprocess.run(['tar', '-xf', '-', '-C', str(stage)], stdin=archive.stdout, check=True)
        archive.stdout.close(); assert archive.wait() == 0
        (stage/'deps').symlink_to(ROOT/'deps', target_is_directory=True)
        shutil.copyfile(ROOT/'tests/v01_stalled_tx.c', stage/'tests/v01_stalled_tx.c')
        before = run(stage, Path(directory)/'before-test')
        after = run(ROOT, Path(directory)/'after-test')
        for result in before:
            assert result.returncode == 1 and 'stalled TX still active' in result.stderr, result.stderr
            print(result.stderr.strip())
        for result in after:
            assert result.returncode == 0, result.stdout+result.stderr
            print(result.stdout.strip())
        report = {'scope':'actual one-voice render/worker/native adapter and portable host; modelled USB/RTOS; ASan/UBSan; no hardware',
            'command':'python3 scripts/test_v01_stalled_tx.py', 'before_revision':BASELINE,
            'before_results':[r.stderr.strip() for r in before],
            'after_results':[r.stdout.strip() for r in after],
            'modes':['ordinary render clock','render clock wraps uint32'],
            'checks':['valid old keepalives cannot renew a stalled TX queue indefinitely',
                      'no native TX submission while native queue refuses sends',
                      'gate closes within lease plus bounded worker-service cadence',
                      'fresh-session recovery after native progress resumes'],
            'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                for p in [ROOT/n for n in SOURCES+['mods/usb-track/worker.c',
                    'mods/usb-midi/usb_midi.c','tests/i03_integrated.c','tests/dt1_native_io.h',
                    'scripts/test_v01_stalled_tx.py']]},
            'limits':['Render-clock age bound requires a running render clock.',
                      'Not a physical freeze fix or timing/CPU qualification.',
                      'Source fix is not installed on the Digitakt.'], 'pass':True}
        (ROOT/'evidence/v01/stalled-tx-bound.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__ == '__main__': main()
