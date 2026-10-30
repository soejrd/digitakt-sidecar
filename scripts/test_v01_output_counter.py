#!/usr/bin/env python3
"""Measure default Output counter semantics after replaying pinned host patches.

No dependency edits, firmware build, device access or hardware acceptance.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run():
    pin = next(r['revision'] for r in json.loads((ROOT/'sources.json').read_text())['repositories']
               if r['name'] == 'sidecar-firmware')
    patches = sorted((ROOT/'patches/sidecar-firmware').glob('*.patch'))
    with tempfile.TemporaryDirectory(prefix='v01-output-counter-') as directory:
        stage = Path(directory)
        archive = subprocess.Popen(['git', '-C', str(ROOT/'deps/sidecar-firmware'),
                                    'archive', pin], stdout=subprocess.PIPE)
        subprocess.run(['tar', '-xf', '-', '-C', str(stage)], stdin=archive.stdout, check=True)
        archive.stdout.close()
        assert archive.wait() == 0
        for patch in patches:
            subprocess.run(['git', 'apply', str(patch)], cwd=stage,
                           env=dict(os.environ, GIT_CEILING_DIRECTORIES=str(stage.parent)), check=True)
        engine = stage/'pi/engine'
        binary = stage/'counter-test'
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-O1', '-g',
                        '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                        '-I'+str(engine), '-I'+str(stage/'core'),
                        str(ROOT/'tests/v01_output_counter.cpp'), str(engine/'output.cpp'),
                        '-o', str(binary)], check=True)
        negative = subprocess.run([str(binary), '--require-no-flush-underflows'],
                                  capture_output=True, text=True)
        assert negative.returncode == 1, negative.stderr
        print(negative.stderr.strip())
        result = json.loads(subprocess.check_output([str(binary)], text=True))
        report = {
            'scope': 'default Circle Output replayed from pinned host base/root patches, ASan/UBSan desktop; no hardware',
            'command': 'python3 scripts/test_v01_output_counter.py',
            'dependency_base': pin,
            'checks': {'fifo_prefilled_before_each_transition': True,
                       'closed_epoch_outputs_silence': True,
                       'reopened_epoch_discards_old_fifo': True,
                       'counter_also_counts_intentional_flush_silence': True,
                       'steady_prefilled_output_does_not_add_underflows': True},
            'measured_counter_deltas_frames': result,
            'negative_check': 'The claim that every counter increase requires producer starvation is false: a prefilled FIFO is flushed on epoch changes.',
            'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in [ROOT/'scripts/test_v01_output_counter.py',
                                        ROOT/'tests/v01_output_counter.cpp', *patches]},
            'replayed_output_sha256': hashlib.sha256((engine/'output.cpp').read_bytes()).hexdigest(),
            'limits': ['Does not attribute any physical counter increase to this mechanism.',
                       'Does not explain or fix SC_PF_LATE.',
                       'No host/device code or installed kernel changed.'],
            'pass': True}
        output = ROOT/'evidence/v01/output-counter-semantics.json'
        output.write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps(result))


if __name__ == '__main__':
    run()
