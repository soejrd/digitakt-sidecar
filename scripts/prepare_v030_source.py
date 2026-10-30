#!/usr/bin/env python3
"""Stage clean sidecar-firmware v0.3.0 plus the DTSC patch series.

Never edits deps/: the source comes from the pinned v0.3.0 tag/commit in the
existing checkout (fetch it first if necessary), the root patch series is
applied, and the portable DTSC/mdkit overlay is copied into pi/engine/dt_control.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
def _upgrade_pin():
    for repo in json.loads((ROOT / 'sources.json').read_text())['repositories']:
        if repo['name'] == 'sidecar-firmware' and 'upgrade' in repo:
            return repo['upgrade']['revision']
    raise SystemExit('sources.json has no sidecar-firmware upgrade pin')


BASE = _upgrade_pin()
PATCHES = ROOT / 'patches/sidecar-firmware-v0.3.0'
OVERLAY = ROOT / 'audio'
PORTABLE = (
    'link.c', 'link.h', 'host_link.c', 'host_link.h',
    'circle_link_adapter.c', 'circle_link_adapter.h', 'sc_memory.h',
    'h02_test_tone.h', 'v01_diag.h',
)
MDKIT = ('include/mdkit.h', 'include/mdkit_voice.h',
         'src/trx/b2/mdkit_trx_b2.c', 'src/trx/b2/mdkit_trx_b2.h')


def git(*args, **kwargs):
    return subprocess.run(['git', *args], check=True, **kwargs)


def prepare(output, base=BASE, experimental=False):
    output = output.resolve()
    if output.exists() and any(output.iterdir()):
        raise SystemExit(f'{output}: output directory is not empty')
    output.mkdir(parents=True, exist_ok=True)
    archive = subprocess.Popen(
        ['git', '-C', str(ROOT / 'deps/sidecar-firmware'), 'archive', base],
        stdout=subprocess.PIPE)
    subprocess.run(['tar', '-xf', '-', '-C', str(output)], stdin=archive.stdout, check=True)
    archive.stdout.close()
    assert archive.wait() == 0, 'sidecar-firmware archive failed'
    patches = sorted(PATCHES.glob('*.patch'))
    if experimental:
        patches += sorted((PATCHES / 'experimental').glob('*.patch'))
    for patch in patches:
        git('apply', str(patch), cwd=output,
            env={**__import__('os').environ, 'GIT_CEILING_DIRECTORIES': str(output.parent)})
    engine = output / 'pi/engine'
    dt = engine / 'dt_control'
    dt.mkdir(parents=True, exist_ok=True)
    portable = PORTABLE + (
        'track_frame.c', 'track_frame.h', 'track_session.c', 'track_session.h',
        'track_host.c', 'track_host.h',
    ) if experimental else PORTABLE
    for name in portable:
        shutil.copyfile(ROOT / 'control' / name, dt / name)
    for source in OVERLAY.iterdir():
        if source.suffix in ('.c', '.h'):
            shutil.copyfile(source, dt / source.name)
    for name in MDKIT:
        shutil.copyfile(ROOT / 'deps/mdkit' / name, dt / Path(name).name)
    if experimental:
        shutil.copyfile(ROOT / 'host/x01_dt_link.cpp', engine / 'dt_link.cpp')
        makefile = engine / 'Makefile'
        text = makefile.read_text()
        extra = ' '.join('dt_control/' + name for name in
                         ('track_frame.c', 'track_session.c', 'track_host.c', 'track_playout.c'))
        if 'PORT_SRC :=' not in text:
            raise SystemExit('Makefile has no PORT_SRC')
        makefile.write_text(text.replace('PORT_SRC :=', 'PORT_SRC := ' + extra, 1))
        with (output / 'NOTICE').open('a') as stream:
            stream.write('\nX01 experimental host glue and dt_control/track_*: '
                         'GPL-2.0-or-later, digitakt-sidecar.\n')
    print(f'staged {output} at sidecar-firmware {base}' +
          (' with X01' if experimental else ''))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--base', default=BASE)
    parser.add_argument('--x01', action='store_true')
    args = parser.parse_args()
    prepare(args.output, args.base, args.x01)
