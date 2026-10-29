#!/usr/bin/env python3
"""Build/boot an emulator-only dual-track firmware fixture; never flash hardware."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import i02_probe as i02
import r03_probe as r03
from r07_handoff_map import MAIN_SHA, SYX_SHA, load_main

ROOT = Path(__file__).resolve().parents[1]


def build(root, stock, fixture):
    from emu.bootstrap import first_run
    from emu.fwcheck import prepare
    root, stock, fixture = root.resolve(), stock.resolve(), fixture.resolve()
    assert r03.sha(stock) == SYX_SHA
    load_main(fixture/'sections')
    root.mkdir(parents=True, exist_ok=False)
    source = root/'source'
    track = source/'mods/usb-track'
    track.mkdir(parents=True)
    (source/'control').symlink_to(ROOT/'control', target_is_directory=True)
    (source/'mods/usb-midi').symlink_to(ROOT/'mods/usb-midi', target_is_directory=True)
    files = ['x01_render.c', 'x01_worker.c', 'x01_track.h', 'usb_track.h',
             'return_lanes.c', 'return_lanes.h', 'src_queue.c', 'src_queue.h',
             'task_worker.c', 'core_candidate_rtos.h', 'core_candidate_rtos.s']
    for name in files:
        shutil.copyfile(ROOT/'mods/usb-track'/name, track/name)
    manifest = json.loads((ROOT/'mods/usb-track/mod.json').read_text())
    manifest['version'] = '0.6'
    manifest['description'] = 'X01E emulator-only dual-mono v4/v5 experiment; not a hardware candidate'
    manifest['sources'] = ['x01_render.c', 'src_queue.c', 'return_lanes.c', 'x01_worker.c',
                           '../../control/link.c', '../../control/track_frame.c',
                           '../../control/track_session.c', 'task_worker.c', 'core_candidate_rtos.s']
    (track/'mod.json').write_text(json.dumps(manifest, indent=2)+'\n')
    artifacts = root/'artifacts'
    artifacts.mkdir()
    env = dict(os.environ, PYTHONPATH=str(ROOT/'deps/elekloader'),
               ELEKLOADER_CROSS=os.environ.get('ELEKLOADER_CROSS', 'm68k-elf-'))
    with (root/'build.log').open('w') as log:
        def run(*args):
            subprocess.run([sys.executable, *args], cwd=ROOT, env=env,
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        for mod in (ROOT/'deps/elekloader/mods/core', ROOT/'deps/elekloader/mods/machine-pages',
                    ROOT/'mods/usb-midi', track):
            run('-m', 'elekloader.sdk.build', str(mod), '--stock', str(stock), '--out', str(artifacts))
        mods = [artifacts/name for name in ('core-4.1-os1.54.elemod', 'machine-pages-1.5-os1.54.elemod',
                                           'usb-midi-0.1.elemod', 'usb-track-0.6.elemod')]
        run('-m', 'elekloader.lint', str(mods[-1]), '--stock', str(stock),
            *[v for mod in mods[:-1] for v in ('--with', str(mod))], '--json')
        run('-m', 'elekloader.patch', '--stock', str(stock),
            *[v for mod in mods for v in ('--mod', str(mod))],
            '--out', str(artifacts/'I02.syx'), '--version', 'X01E')
    paths, _ = prepare(str(artifacts/'I02.syx'), str(root/'emulator'))
    shutil.copyfile(fixture/'plusdrive.img', paths.card)
    os.environ.update(paths.env())
    first_run(paths, progress=lambda e: print(e.step,e.kind,flush=True) if e.kind in ('start','done') else None)
    i02.assign_project(paths, root)
    inputs = [ROOT/'mods/usb-track'/name for name in files]
    inputs += [ROOT/'mods/usb-midi'/name for name in ('mod.json', 'usb_midi.c', 'usb_midi.h')]
    inputs += [ROOT/'control'/name for name in ('link.c', 'link.h', 'track_frame.c', 'track_frame.h',
                'track_session.c', 'track_session.h', 'sc_memory.h', 'dt1_transport.c', 'dt1_transport.h',
                'usbmidi.c', 'usbmidi.h', 'sysex_split.c', 'sysex_split.h')]
    metadata = {'firmware_root': str(Path(paths.root).relative_to(root)), 'map': 'artifacts/I02.syx.map.json',
                'firmware_label': 'X01E', 'usb_track_version': '0.6', 'emulator_only': True,
                'stock_main_sha256': MAIN_SHA, 'syx_sha256': r03.sha(artifacts/'I02.syx'),
                'manifest_sha256': hashlib.sha256((track/'mod.json').read_bytes()).hexdigest(),
                'source_hashes': {str(p.relative_to(ROOT)): r03.sha(p) for p in sorted(inputs)}}
    (root/'fixture.json').write_text(json.dumps(metadata, indent=2)+'\n')
    print('X01E EMULATOR-ONLY candidate booted/assigned. No hardware candidate or flash.', flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--stock', type=Path, required=True)
    p.add_argument('--fixture', type=Path, default=ROOT/'private/i02-fixture')
    a = p.parse_args()
    build(a.root, a.stock, a.fixture)
