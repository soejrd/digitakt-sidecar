#!/usr/bin/env python3
"""Verify OS 1.54 render-hook control changes in the emulator.

The probe watches the machine-pages render point (0x40077FBA) and records
what the firmware presents for one audio track: final 8.8 A-H values, note,
velocity, the fw_voice_start bit and the order of render blocks. It runs four
scenarios through the panel/pattern path:

  baseline_manual   a manual pad trigger, used as the unmodulated reference
  plock_restore     a held step's p-lock, then the next unlocked step
  lfo_modulation    documented LFO sound slots seeded before the trigger
  repeated_starts   three separate manual triggers, every start bit retained

This is an emulator probe only. It makes no hardware timing claim and does not
decide whether a Digitakt retrigger is a distinct sequencer event. Unicorn is
run in child processes because a JIT/mem_map failure otherwise takes down the
caller before the failure can be recorded.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
PRE = 0x40077FBA
MAIN_SHA = '5c58bf9e3949ef09977c5fc007a61e8d026931f67f1621238379dfb8ee4d31a2'
PARAMS = 0x80002794
START = 0x80001228
NOTE = 0x80001F28
VELOCITY = 0x80001F18
PATTERN_POINTER = 0x4199EC38
TRACK_STRIDE = 911
TRIG_BIT = 0x0001
SOUND_STRIDE = 162
KIT_SOUND = 0x20


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _u32(uc, address):
    return struct.unpack('>I', bytes(uc.mem_read(address, 4)))[0]


def _u16(uc, address):
    return struct.unpack('>H', bytes(uc.mem_read(address, 2)))[0]


def _i16s(uc, address, count):
    return list(struct.unpack('>%dh' % count, bytes(uc.mem_read(address, 2 * count))))


def _write_i16s(uc, address, values):
    uc.mem_write(address, struct.pack('>%dh' % len(values), *values))


def _fixture(candidate):
    candidate = Path(candidate).resolve()
    meta = json.loads((candidate / 'fixture.json').read_text())
    symbols = json.loads((candidate / meta['map']).read_text())
    firmware = candidate / meta['firmware_root'] / 'I02.syx'
    if sha256(firmware) != meta['syx_sha256']:
        raise RuntimeError('candidate changed since the fixture was built')
    if meta.get('stock_main_sha256') != MAIN_SHA:
        raise RuntimeError('fixture is not the pinned OS 1.54 stock MAIN image')
    for name, digest in meta.get('source_hashes', {}).items():
        if sha256(ROOT / name) != digest:
            raise RuntimeError('candidate source changed since the fixture was built: ' + name)
    return candidate, meta, symbols, firmware


def _candidate_summary(meta):
    return {
        'firmware_label': meta.get('firmware_label'),
        'usb_track_version': meta.get('usb_track_version'),
        'syx_sha256': meta.get('syx_sha256'),
        'stock_main_sha256': meta.get('stock_main_sha256'),
        'source_hashes': meta.get('source_hashes', {}),
    }


def _configure(firmware):
    import r03_probe as r03
    from emu.bootstrap import FirmwarePaths

    paths = FirmwarePaths(str(firmware.parent.resolve()), firmware.name,
                          str(r03.EMU / 'devices'))
    os.environ.update(paths.env())
    return paths


def _select_usb1(session, symbols, track=0):
    import r03_probe as r03

    r03.ACTIVE_TRACK = int(symbols['fw_active_track'], 0)
    r03.select_track(session, track)
    session.press(session.code('FUNC'))
    session.run_ms(80)
    r03.tap(session, 20)  # SRC machine chooser
    session.release(session.code('FUNC'))
    for _ in range(4):
        r03.tap(session, 15)
    r03.tap(session, 12)
    r03.tap(session, 12)
    session.run_ms(100)
    kit = _u32(session.m.uc, int(symbols['fw_kit'], 0))
    machine = session.m.uc.mem_read(kit + KIT_SOUND + SOUND_STRIDE * track + 126, 1)[0]
    if machine != 121:
        raise RuntimeError('USB1 was not selected on track %d (machine %d)' % (track, machine))
    return kit


def _sound_slots(uc, kit, track):
    base = kit + KIT_SOUND + SOUND_STRIDE * track
    return list(struct.unpack('>53h', bytes(uc.mem_read(base + 20, 106))))


def _write_sound_slots(uc, kit, track, values):
    base = kit + KIT_SOUND + SOUND_STRIDE * track
    _write_i16s(uc, base + 20, values)


def _pattern_steps(uc, track, count=8):
    pointer = _u32(uc, PATTERN_POINTER)
    if not 0x40000000 <= pointer < 0x48000000:
        return {'pointer': pointer, 'readable': False, 'words': []}
    words = [_u16(uc, pointer + TRACK_STRIDE * track + 2 * step)
             for step in range(count)]
    return {'pointer': pointer, 'readable': True, 'words': words}


def _render_hook(session, state, events, track):
    def callback(uc, address, size, data):
        if not state['active']:
            return
        state['block'] += 1
        mask = _u32(uc, START)
        note = _u32(uc, NOTE + 4 * track)          # int32, 16.16
        velocity = struct.unpack('>h', bytes(
            uc.mem_read(VELOCITY + 2 * track, 2)))[0]  # int16, 8.8
        params = _i16s(uc, PARAMS + 2 * 53 * track, 8)
        events.append({
            'sequence': state['sequence'],
            'block': state['block'],
            'emulated_ms': round(session.now_ms(), 3),
            'track': track,
            'start_mask': mask,
            'voice_start': bool(mask & (1 << track)),
            'note_16_16': note,
            'velocity_8_8': velocity,
            'params_8_8': params,
        })
        state['sequence'] += 1

    session.m.uc.hook_add(__import__('unicorn').UC_HOOK_CODE, callback,
                          begin=PRE, end=PRE)


def _session(candidate, symbols, on_machine):
    from emu.session import Session

    return Session(str(candidate / 'assigned.snap'), audio=True,
                   on_machine=on_machine)


def _start_capture(state, events):
    events.clear()
    state.update(active=True, block=0, sequence=0)


def _stop_capture(state):
    state['active'] = False


def _start_events(events):
    return [event for event in events if event['voice_start']]


def _ordered(events):
    return all(b['block'] == a['block'] + 1 and b['sequence'] == a['sequence'] + 1
               for a, b in zip(events, events[1:]))


def _scenario_baseline(candidate, symbols):
    events = []
    state = {'active': False, 'block': 0, 'sequence': 0}

    def on_machine(session):
        _render_hook(session, state, events, 0)

    session = _session(candidate, symbols, on_machine)
    try:
        session.run_ms(150)
        _select_usb1(session, symbols, 0)
        _start_capture(state, events)
        session.tap(24, hold_ms=50, after_ms=350)
        if session.halted:
            raise RuntimeError(session.halted)
        starts = _start_events(events)
        if not starts:
            raise AssertionError('baseline trigger produced no fw_voice_start bit')
        if not _ordered(events):
            raise AssertionError('render blocks are not in contiguous order')
        first = starts[0]
        return {
            'name': 'baseline_manual',
            'scope': 'manual trigger; final render-hook values',
            'render_blocks': len(events),
            'start_events': len(starts),
            'first_start': first,
            'last_event': events[-1],
            'final_params_8_8': events[-1]['params_8_8'],
            'final_note_16_16': events[-1]['note_16_16'],
            'final_velocity_8_8': events[-1]['velocity_8_8'],
            'block_order_contiguous': True,
        }
    finally:
        session.close()


def _scenario_plock(candidate, symbols):
    import r03_probe as r03

    events = []
    state = {'active': False, 'block': 0, 'sequence': 0}

    def on_machine(session):
        _render_hook(session, state, events, 0)

    session = _session(candidate, symbols, on_machine)
    try:
        uc = session.m.uc
        session.run_ms(150)
        kit = _select_usb1(session, symbols, 0)
        _start_capture(state, events)
        session.tap(24, hold_ms=50, after_ms=350)
        baseline = _start_events(events)[0]
        baseline_params = baseline['params_8_8'][:]
        _stop_capture(state)

        sound_before = _sound_slots(uc, kit, 0)
        r03.tap(session, session.code('RECORD'))
        session.press(24)
        session.run_ms(500)
        r03.rotate(session, 1, 8)  # PTCH
        session.release(24)
        session.run_ms(300)
        r03.tap(session, 25)       # step 2, no lock
        r03.tap(session, session.code('RECORD'))
        sound_after = _sound_slots(uc, kit, 0)
        if sound_before != sound_after:
            raise AssertionError('holding the step changed the sound instead of only its lock')

        pattern = _pattern_steps(uc, 0)
        if not pattern['readable']:
            raise AssertionError('active pattern pointer is not readable')
        if pattern['words'][0] & TRIG_BIT == 0 or pattern['words'][1] & TRIG_BIT == 0:
            raise AssertionError('grid recording did not create trigs on steps 1 and 2: %r' % pattern)

        _start_capture(state, events)
        session.tap(session.code('PLAY'), hold_ms=40, after_ms=1200)
        session.tap(session.code('STOP'), hold_ms=40, after_ms=100)
        if session.halted:
            raise RuntimeError(session.halted)
        starts = _start_events(events)
        if len(starts) < 2:
            raise AssertionError('expected at least two sequencer starts, got %d' % len(starts))
        if not _ordered(events):
            raise AssertionError('render blocks are not in contiguous order')
        first, second = starts[0], starts[1]
        locked_value = first['params_8_8'][0]
        restored = [event for event in events
                    if event['block'] >= second['block'] and
                    event['params_8_8'][0] == baseline_params[0]]
        if locked_value == baseline_params[0]:
            raise AssertionError('PTCH p-lock did not change the final render value')
        if not restored:
            raise AssertionError('PTCH did not return to its baseline after the unlocked step')
        tail = events[-8:]
        if any(event['params_8_8'][0] != baseline_params[0] for event in tail):
            raise AssertionError('PTCH was not restored for the tail of the run')
        return {
            'name': 'plock_restore',
            'scope': 'panel-created p-lock on step 1; unlocked step 2',
            'pattern': pattern,
            'baseline_params_8_8': baseline_params,
            'locked_start': first,
            'unlocked_start': second,
            'restored_after_block': restored[0]['block'],
            'sound_unchanged_by_lock_edit': True,
            'render_blocks': len(events),
            'start_events': len(starts),
            'final_params_8_8': events[-1]['params_8_8'],
            'final_note_16_16': events[-1]['note_16_16'],
            'final_velocity_8_8': events[-1]['velocity_8_8'],
            'block_order_contiguous': True,
            'tail_params_8_8': [event['params_8_8'] for event in tail],
        }
    finally:
        session.close()


def _scenario_lfo(candidate, symbols):
    events = []
    state = {'active': False, 'block': 0, 'sequence': 0}

    def on_machine(session):
        _render_hook(session, state, events, 0)

    session = _session(candidate, symbols, on_machine)
    try:
        uc = session.m.uc
        session.run_ms(150)
        kit = _select_usb1(session, symbols, 0)
        slots = _sound_slots(uc, kit, 0)
        # LFO 1 fields are sound slots 1-8 in the pinned 1.54 probe
        # (deps/elekloader/tests/probe_digitakt_src.py). Slot 4 is DST and
        # takes a sound-slot index; 17 is this page's PTCH value slot.
        seeds = {
            1: 0x5000,       # speed
            4: 17 << 8,      # destination: PTCH sound slot
            5: 2 << 8,       # waveform: square, as the pinned LFO probe uses
            6: 0x4000,       # start phase
            7: 1 << 8,       # trigger reset
            8: 0x4800,       # nonzero depth
            17: 60 << 8,     # PTCH baseline
        }
        for slot, value in seeds.items():
            slots[slot] = value
        _write_sound_slots(uc, kit, 0, slots)
        _start_capture(state, events)
        session.tap(24, hold_ms=50, after_ms=500)
        if session.halted:
            raise RuntimeError(session.halted)
        starts = _start_events(events)
        if not starts:
            raise AssertionError('LFO scenario produced no voice start')
        if not _ordered(events):
            raise AssertionError('render blocks are not in contiguous order')
        values = [event['params_8_8'][0] for event in events]
        if len(set(values)) < 3:
            raise AssertionError('PTCH did not vary under the seeded LFO: %r' % sorted(set(values)))
        if min(values) < 0 or max(values) > 0x7F00:
            raise AssertionError('LFO produced PTCH outside the 0..0x7f00 range: %r' % (min(values), max(values)))
        return {
            'name': 'lfo_modulation',
            'scope': 'documented sound slots seeded before a manual trigger; render-hook values',
            'seeded_sound_slot_indexes': {str(k): v for k, v in seeds.items()},
            'ptch_values_8_8': values,
            'ptch_distinct_values': sorted(set(values)),
            'ptch_min': min(values),
            'ptch_max': max(values),
            'render_blocks': len(events),
            'start_events': len(starts),
            'first_event': events[0],
            'last_event': events[-1],
            'final_params_8_8': events[-1]['params_8_8'],
            'final_note_16_16': events[-1]['note_16_16'],
            'final_velocity_8_8': events[-1]['velocity_8_8'],
            'block_order_contiguous': True,
        }
    finally:
        session.close()


def _scenario_repeated(candidate, symbols):
    events = []
    state = {'active': False, 'block': 0, 'sequence': 0}

    def on_machine(session):
        _render_hook(session, state, events, 0)

    session = _session(candidate, symbols, on_machine)
    try:
        session.run_ms(150)
        _select_usb1(session, symbols, 0)
        _start_capture(state, events)
        for _ in range(3):
            session.tap(24, hold_ms=40, after_ms=250)
        if session.halted:
            raise RuntimeError(session.halted)
        starts = _start_events(events)
        if len(starts) < 3:
            raise AssertionError('three triggers produced %d start bits' % len(starts))
        if not _ordered(events):
            raise AssertionError('render blocks are not in contiguous order')
        blocks = [event['block'] for event in starts]
        if blocks != sorted(set(blocks)):
            raise AssertionError('start bits are not preserved in render-block order')
        return {
            'name': 'repeated_starts',
            'scope': 'three separate manual triggers; every render-hook start bit retained',
            'trigger_count': 3,
            'start_events': len(starts),
            'start_events_detail': starts,
            'start_blocks': blocks,
            'start_sequences': [event['sequence'] for event in starts],
            'render_blocks': len(events),
            'final_params_8_8': events[-1]['params_8_8'],
            'final_note_16_16': events[-1]['note_16_16'],
            'final_velocity_8_8': events[-1]['velocity_8_8'],
            'block_order_contiguous': True,
            'retrigger_semantics': 'unresolved: no claim that a Digitakt retrigger is a distinct sequencer event',
        }
    finally:
        session.close()


def dynamic_worker(candidate, output):
    candidate, meta, symbols, firmware = _fixture(candidate)
    _configure(firmware)
    import r03_probe as r03

    r03.EMU = ROOT / 'deps/digiemu'

    scenarios = {
        'baseline_manual': _scenario_baseline(candidate, symbols),
        'plock_restore': _scenario_plock(candidate, symbols),
        'lfo_modulation': _scenario_lfo(candidate, symbols),
        'repeated_starts': _scenario_repeated(candidate, symbols),
    }
    report = {
        'schema': 'i03-dynamic-154/1',
        'status': 'verified',
        'level': 'compiled OS 1.54 emulator; render hook; no hardware timing',
        'hardware_test': False,
        'candidate': _candidate_summary(meta),
        'render_contract': {
            'render_hook': '0x40077FBA',
            'final_params_8_8': '0x80002794 + 2 * (53 * track + k), k = 0..7',
            'voice_start': '0x80001228, bit t for track t',
            'note_16_16': '0x80001F28 + 4 * track (int32)',
            'velocity_8_8': '0x80001F18 + 2 * track (int16)',
            'block_order': 'one render-hook snapshot per block, in invocation order',
        },
        'scenarios': scenarios,
        'verified': [
            'final A-H values, note, velocity and start bit are observable at the render hook',
            'a panel-created p-lock changes PTCH at the render hook and the unlocked next step restores it',
            'seeded LFO settings change PTCH across render blocks',
            'every snapshotted repeated manual start bit remains in block order',
        ],
        'unresolved': [
            'Retrigger is not established as a distinct Digitakt sequencer event; only repeated start bits were observed.',
            'No hardware timing, callback latency, USB-MIDI delivery or device audio claim follows from these emulator values.',
            'The LFO scenario seeds documented sound slots rather than proving a complete panel-only LFO edit.',
        ],
    }
    Path(output).write_text(json.dumps(report, indent=2) + '\n')


def preflight_worker():
    print('unicorn-import', flush=True)
    from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN
    print('unicorn-construct', flush=True)
    uc = Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN)
    print('mem-map-start', flush=True)
    uc.mem_map(0x10000000, 0x1000)
    print('mem-map-ok', flush=True)
    uc.mem_write(0x10000000, b'\x4e\x71')  # nop
    uc.emu_start(0x10000000, 0x10000002, count=1)
    print('exec-ok', flush=True)


def _run_child(args, timeout):
    return subprocess.run([sys.executable, str(Path(__file__).resolve())] + args,
                          cwd=str(ROOT), text=True, capture_output=True,
                          timeout=timeout)


def _preflight_record(result):
    output = (result.stdout or '') + (result.stderr or '')
    record = {
        'command': 'python tools/i03_dynamic_probe.py --worker preflight',
        'returncode': result.returncode,
        'passed': result.returncode == 0,
        'output_tail': output.strip().splitlines()[-8:],
    }
    if result.returncode is not None and result.returncode < 0:
        record['signal'] = -result.returncode
        try:
            record['signal_name'] = signal.Signals(-result.returncode).name
        except ValueError:
            pass
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--worker', choices=('preflight', 'dynamic'), help=argparse.SUPPRESS)
    parser.add_argument('--allow-blocked', action='store_true')
    parser.add_argument('--timeout', type=int, default=180)
    args = parser.parse_args()

    if args.worker == 'preflight':
        preflight_worker()
        return 0
    if args.worker == 'dynamic':
        if not args.candidate or not args.output:
            parser.error('dynamic worker needs --candidate and --output')
        dynamic_worker(args.candidate, args.output)
        return 0

    if not args.candidate or not args.output:
        parser.error('--candidate and --output are required')
    candidate, meta, symbols, firmware = _fixture(args.candidate)
    probe = {
        'path': 'tools/i03_dynamic_probe.py',
        'sha256': sha256(Path(__file__)),
    }
    common = {
        'schema': 'i03-dynamic-154/1',
        'hardware_test': False,
        'level': 'compiled OS 1.54 emulator; no hardware timing',
        'candidate': _candidate_summary(meta),
        'probe': probe,
        'runtime': {
            'python': sys.version.split()[0],
            'platform': sys.platform,
        },
        'scope': {
            'render_hook': '0x40077FBA',
            'values': 'final 8.8 A-H, note (16.16), velocity (8.8) and fw_voice_start at the render hook',
            'ordering': 'render-block invocation order',
        },
        'retrigger_semantics': (
            'unresolved: this probe records repeated start bits; it does not establish a '
            'distinct Digitakt retrigger event or its hardware timing'
        ),
        'planned_scenarios': [
            'baseline_manual',
            'plock_restore',
            'lfo_modulation',
            'repeated_starts',
        ],
        'limits': [
            'Emulator values only; no hardware timing, latency or audio result is claimed.',
            'The LFO scenario seeds documented sound slots and is not a full panel-edit protocol test.',
            'A blocked Unicorn preflight produces evidence of the environment failure, not a PASS.',
        ],
        'reproduce': (
            'PYTHONPATH="$PWD/private/r03-runtime:$PWD/deps/digiemu" "$PY" '
            'tools/i03_dynamic_probe.py --candidate private/i03-final '
            '--output evidence/i03/dynamic-154.json'
        ),
    }

    try:
        preflight = _run_child(['--worker', 'preflight'], timeout=args.timeout)
    except subprocess.TimeoutExpired as exc:
        preflight = {'passed': False, 'returncode': None,
                     'output_tail': ['preflight timed out'],
                     'error': str(exc)}
    preflight_record = _preflight_record(preflight) if isinstance(preflight, subprocess.CompletedProcess) else preflight
    if not preflight_record.get('passed'):
        report = dict(common)
        report.update({
            'status': 'blocked_environment',
            'preflight': preflight_record,
            'verified': [],
            'environment_conclusion': (
                'Unicorn cannot map executable memory in this execution sandbox; '
                'no firmware scenario ran and no dynamic behavior is claimed.'
            ),
        })
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n')
        print('I03 dynamic probe BLOCKED: ' + str(preflight_record.get('output_tail', [])[-2:]))
        return 0 if args.allow_blocked else 2

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=args.output.parent,
                                     prefix=args.output.stem + '.',
                                     suffix='.worker.json', delete=False) as tmp:
        temp_path = Path(tmp.name)
    try:
        result = _run_child(['--worker', 'dynamic', '--candidate', str(candidate),
                             '--output', str(temp_path)], timeout=args.timeout)
        if result.returncode == 0 and temp_path.exists():
            report = json.loads(temp_path.read_text())
        else:
            report = dict(common)
            report.update({
                'status': 'worker_failed',
                'preflight': preflight_record,
                'verified': [],
                'worker': {
                    'returncode': result.returncode,
                    'stdout_tail': (result.stdout or '').strip().splitlines()[-12:],
                    'stderr_tail': (result.stderr or '').strip().splitlines()[-12:],
                },
            })
        report['preflight'] = preflight_record
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    finally:
        temp_path.unlink(missing_ok=True)

    print('I03 dynamic probe', report['status'].upper())
    return 0 if report['status'] == 'verified' else 1


if __name__ == '__main__':
    raise SystemExit(main())
