#!/usr/bin/env python3
"""Reproduce stock playback and pre-filter PCM injection without host audio.

Run with a compatible patched Unicorn and the pinned deps/digiemu source.
prepare creates PRIVATE firmware/card/snapshots from the user's stock 1.53 OS;
check emits metrics only. No physical MIDI/USB or sound-card access is used.
"""
import argparse
import array
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import struct
import sys
import wave

PROJECT = Path(__file__).resolve().parents[1]
EMU = PROJECT / 'deps/digiemu'
sys.path.insert(0, str(EMU))

# Pinned OS 1.53. Source: elekloader core/fw.s and machine-pages/pages.s.
SYX_SHA = '9bdd44bb6102fb25c143cfab97bc92b7a89c463f795d3112dce89771e29bcc92'
MAIN_SHA = '4b47a9507758ca5669ca02ab2c0374d2c04c98aece445408295cc1dcb265c5df'
TRACK_BLOCK = 0x80001A18
PRE_DRIVE = 0x40077FBA
ACTIVE_TRACK = 0x4197B6B4
RATE = 48000


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def configure(root):
    from emu.bootstrap import FirmwarePaths
    paths = FirmwarePaths(str(root), 'Digitakt_OS1.53.syx', str(EMU / 'devices'))
    os.environ.update(paths.env())
    return paths


def verify(paths):
    if sha(paths.syx) != SYX_SHA or sha(paths.main_img) != MAIN_SHA:
        raise RuntimeError('Probe addresses require the pinned stock OS 1.53 images')


def tap(s, code, after=100):
    s.tap(code, hold_ms=30, after_ms=after)


def rotate(s, code, amount):
    # Device uses four wire counts/detent. Pace small batches to avoid
    # wrapping/coalescing its encoder accumulator before the UI handles it.
    while amount:
        step = max(-4, min(4, amount))
        s.turn(code, step, after_ms=25)
        amount -= step


def select_track(s, track):
    s.press(2)  # TRK
    s.run_ms(50)
    tap(s, 24 + track)
    s.release(2)
    s.run_ms(100)
    actual = struct.unpack('>I', s.m.uc.mem_read(ACTIVE_TRACK, 4))[0]
    if actual != track:
        raise RuntimeError(f'Track selection failed: wanted {track}, got {actual}')


def save_session(s, target):
    from emu.snapshot import save
    s.ev['claim_checkpoint_component']('timers', s.pits)
    tasks = {hex(k): v for k, v in s.st.get('task_create_hits', {}).items()}
    save(s.m, str(target), extra={'tasks': tasks},
         components=s.ev['checkpoint_components'],
         manifest=s.ev['checkpoint_manifest'])


def prepare(args):
    from emu import bootstrap, ekfsformat as ek
    from emu.session import Session
    root = args.root.resolve()
    # A fresh directory avoids mixing a boot's cached filesystem index with
    # a different card, which otherwise produces misleading loader failures.
    root.mkdir(parents=True, exist_ok=False)
    syx = root / 'Digitakt_OS1.53.syx'
    if sha(args.syx) != SYX_SHA:
        raise RuntimeError('prepare requires the tested stock OS 1.53 container')
    shutil.copyfile(args.syx, syx)
    paths = configure(root)

    def progress(event):
        if event.kind in ('start', 'done'):
            print(event.step, event.kind, flush=True)

    bootstrap.first_run(paths, progress=progress)
    tone = root / 'r03-tone.wav'
    with wave.open(str(tone), 'wb') as out:
        out.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        out.writeframes(b''.join(struct.pack('<h', round(6000 * math.sin(
            2 * math.pi * 440 * i / RATE))) for i in range(RATE)))
    fs = ek.Ekfs(paths.card, write=True)
    try:
        # Same library conversion/hash/index writer as emu.samples. A root
        # fixture gives the UI sequence a single file and two directories.
        inode, name, rate, frames = fs.add_sample(ek.ROOT_INODE, tone.name,
                                                tone.read_bytes())
    finally:
        fs.close()
    bootstrap.first_run(paths, progress=progress)  # refresh the boot index
    verify(paths)

    s = Session(paths.gui, audio=False)
    try:
        s.run_ms(700)
        # SETTINGS -> SAMPLES; root rows Factory, incoming, r03-tone.
        for code in (6, 15, 12, 15, 15):
            tap(s, code, after=400)
        # Right sidebar: SELECT ALL, then LOAD TO PROJECT, confirm.
        for code in (17, 12, 15, 15, 12, 12):
            tap(s, code, after=400)
        s.run_ms(300)
        for _ in range(4):
            tap(s, 13)
        save_session(s, root / 'unassigned.snap')
        # Both stock tracks use project sample slot 1. Loading a file into
        # the project alone does not replace the tracks' missing factory slots.
        for track in (0, 1):
            select_track(s, track)
            tap(s, 20)  # SRC
            s.turn(4, 1, after_ms=150)  # D opens the sample slot chooser
            for _ in range(130):
                tap(s, 14, after=70)  # clamp to OFF, irrespective of old slot
            tap(s, 15)  # slot 1
            tap(s, 12)  # confirm
        save_session(s, root / 'assigned.snap')
    finally:
        s.close()
    manifest = {'syx_sha256': SYX_SHA, 'main_sha256': MAIN_SHA,
                'tone_sha256': sha(tone), 'sample_inode': inode,
                'sample_name': name, 'sample_rate': rate, 'sample_frames': frames}
    (root / 'fixture.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('PRIVATE FIXTURE READY', flush=True)


def metrics(pcm):
    a = array.array('h')
    a.frombytes(pcm)
    if sys.byteorder != 'little':
        a.byteswap()
    mono = [(a[i] + a[i + 1]) / 2 for i in range(0, len(a), 2)]
    # Coherent projection distinguishes injected 3 kHz from the 440 Hz source.
    def tone_amplitude(hz):
        real = sum(x * math.cos(2 * math.pi * hz * i / RATE)
                   for i, x in enumerate(mono))
        imag = sum(x * math.sin(2 * math.pi * hz * i / RATE)
                   for i, x in enumerate(mono))
        return 2 * math.hypot(real, imag) / max(1, len(mono))
    return {'frames': len(mono), 'peak': max(map(abs, a), default=0),
            'rms': math.sqrt(sum(x*x for x in a) / max(1, len(a))),
            'amplitude_440_hz': tone_amplitude(440),
            'amplitude_3000_hz': tone_amplitude(3000),
            'pcm_sha256': hashlib.sha256(pcm).hexdigest()}


def capture(root, name, *, inject=False, zero=False, cutoff=None,
            amp_zero=False, track=0, unassigned=False):
    from emu.session import Session
    from unicorn import UC_HOOK_CODE
    hits = 0
    phase = 0

    def on_machine(s):
        def callback(uc, address, size, data):
            nonlocal hits, phase
            hits += 1
            values = [0] * 32 if zero else [round(math.sin(
                2 * math.pi * 3000 * (phase + i) / RATE) * 0.1 * (2**31 - 1))
                for i in range(32)]
            phase += 32
            uc.mem_write(TRACK_BLOCK, struct.pack('>32i', *values))
        if inject:
            s.m.uc.hook_add(UC_HOOK_CODE, callback, begin=PRE_DRIVE, end=PRE_DRIVE)

    snap = root / ('unassigned.snap' if unassigned else 'assigned.snap')
    s = Session(str(snap), audio=True, on_machine=on_machine)
    try:
        s.run_ms(100)
        select_track(s, track)
        if cutoff is not None:
            tap(s, 21)  # FLTR page 1; E = frequency
            rotate(s, 5, 127 if cutoff == 'open' else -127)
        if amp_zero:
            tap(s, 22)  # AMP page; H = volume
            rotate(s, 8, -127)
        s.pcm.clear()
        s.tap(24 + track, hold_ms=50, after_ms=900)
        result = metrics(s.pcm)
        result.update(name=name, injection_blocks=hits, halted=s.halted)
        if s.halted:
            raise RuntimeError(s.halted)
        print(json.dumps(result, sort_keys=True), flush=True)
        return result
    finally:
        s.close()


def check(args):
    from emu import unicorn_compat
    paths = configure(args.root.resolve())
    verify(paths)
    compatibility = unicorn_compat.require_compatible_unicorn()
    if not compatibility['compatible']:
        raise RuntimeError('Unicorn compatibility checks failed')
    root = args.root.resolve()
    results = {}
    cases = [
        ('unassigned', dict(unassigned=True)),
        ('stock', {}),
        ('injected_open', dict(inject=True, cutoff='open')),
        ('injected_closed', dict(inject=True, cutoff='closed')),
        ('injected_amp_zero', dict(inject=True, cutoff='open', amp_zero=True)),
        ('injected_zero', dict(inject=True, zero=True)),
        ('second_stock', dict(track=1)),
        ('second_with_injection', dict(track=1, inject=True)),
    ]
    for name, kwargs in cases:
        results[name] = capture(root, name, **kwargs)
    r = results
    assertions = {
        'negative_control_silent': r['unassigned']['peak'] == 0,
        'stock_audible': r['stock']['peak'] > 100,
        'hook_executed': r['injected_open']['injection_blocks'] > 100,
        'injected_tone_present': r['injected_open']['amplitude_3000_hz'] >
            max(50, 20 * r['stock']['amplitude_3000_hz']),
        'filter_attenuates': r['injected_closed']['rms'] <
            r['injected_open']['rms'] / 100,
        'amp_mutes': r['injected_amp_zero']['peak'] <= 1,
        'zero_replaces_source': r['injected_zero']['peak'] <= 1,
        'second_track_audible': r['second_stock']['peak'] > 100,
        'second_track_unchanged': r['second_stock']['pcm_sha256'] ==
            r['second_with_injection']['pcm_sha256'],
    }
    report = {'firmware': 'Digitakt mk1 OS 1.53', 'hardware_test': False,
              'compatibility': compatibility, 'results': results,
              'assertions': assertions, 'pass': all(assertions.values())}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('R03 PASS' if report['pass'] else 'R03 FAIL', assertions, flush=True)
    if not report['pass']:
        raise SystemExit(1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest='command', required=True)
    setup = subs.add_parser('prepare')
    setup.add_argument('--root', type=Path, required=True, help='new private directory')
    setup.add_argument('--syx', type=Path, required=True)
    run = subs.add_parser('check')
    run.add_argument('--root', type=Path, required=True)
    run.add_argument('--output', type=Path)
    args = parser.parse_args()
    (prepare if args.command == 'prepare' else check)(args)


if __name__ == '__main__':
    main()
