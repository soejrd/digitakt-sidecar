#!/usr/bin/env python3
"""Prepare an isolated opt-in X01 host source tree; never edits deps/ or hardware."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def target_revision():
    pins = json.loads((ROOT/'sources.json').read_text())['repositories']
    return next(r for r in pins if r['name'] == 'sidecar-firmware')


def prepare(output):
    pins = {r['name']: r['revision'] for r in json.loads((ROOT/'sources.json').read_text())['repositories']}
    target = target_revision()
    assert subprocess.check_output(['git', '-C', str(ROOT/'deps/mdkit'), 'rev-parse', 'HEAD'], text=True).strip() == pins['mdkit']
    subprocess.run(['git', '-C', str(ROOT/'deps/mdkit'), 'diff', '--quiet', '--',
                    'include/mdkit.h', 'include/mdkit_voice.h',
                    'src/trx/b2/mdkit_trx_b2.c', 'src/trx/b2/mdkit_trx_b2.h'], check=True)
    output.mkdir(parents=True, exist_ok=False)
    archive = subprocess.Popen(['git', '-C', str(ROOT/'deps/sidecar-firmware'), 'archive', target['revision']], stdout=subprocess.PIPE)
    subprocess.run(['tar', '-xf', '-', '-C', str(output)], stdin=archive.stdout, check=True)
    archive.stdout.close()
    assert archive.wait() == 0
    patches = sorted((ROOT/'patches/sidecar-firmware').glob('*.patch'))
    patches += sorted((ROOT/'patches/sidecar-firmware/experimental').glob('*x01*.patch'))
    for patch in patches:
        subprocess.run(['git', 'apply', str(patch)], cwd=output,
                       env=dict(os.environ, GIT_CEILING_DIRECTORIES=str(output.parent)), check=True)
    engine = output/'pi/engine'
    overlay = engine/'dt_control'
    overlay.mkdir(exist_ok=True)
    portable = ('link.c', 'link.h', 'host_link.c', 'host_link.h',
                'circle_link_adapter.c', 'circle_link_adapter.h', 'sc_memory.h',
                'h02_test_tone.h', 'v01_diag.h', 'track_frame.c', 'track_frame.h',
                'track_session.c', 'track_session.h', 'track_host.c', 'track_host.h')
    for name in portable:
        shutil.copyfile(ROOT/'control'/name, overlay/name)
    for source in (ROOT/'audio').iterdir():
        if source.suffix in ('.h', '.c'):
            shutil.copyfile(source, overlay/source.name)
    for name in ('include/mdkit.h', 'include/mdkit_voice.h', 'src/trx/b2/mdkit_trx_b2.c', 'src/trx/b2/mdkit_trx_b2.h'):
        shutil.copyfile(ROOT/'deps/mdkit'/name, overlay/Path(name).name)
    shutil.copyfile(ROOT/'host/x01_dt_link.cpp', engine/'dt_link.cpp')
    makefile = engine/'Makefile'
    source = makefile.read_text()
    extra = ' '.join('dt_control/'+name for name in
                     ('track_frame.c', 'track_session.c', 'track_host.c', 'track_playout.c'))
    assert 'PORT_SRC :=' in source
    source = source.replace('PORT_SRC :=', 'PORT_SRC := '+extra, 1)
    makefile.write_text(source)
    with (output/'NOTICE').open('a') as f:
        f.write('\nX01 experimental host glue and dt_control/track_*: GPL-2.0-or-later, digitakt-sidecar.\n')
    print('X01 isolated host tree prepared; default dependency and kernel unchanged.')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    prepare(p.parse_args().output.resolve())
