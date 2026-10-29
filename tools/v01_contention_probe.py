#!/usr/bin/env python3
"""V01 execution-model preflight, NOT a dense active-link qualification.

Runs native mk1 panel dispatch and LFO rendering with cycle timing. Refuses to
score Gate A while USB DMA/IRQ and ACK arrivals are absent. Firmware stays in
the private fixture; output contains only hashes, addresses and measurements.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

import r03_probe  # configures the pinned emulator import path
import i03_dynamic_probe as dynamic


def u32(uc, address):
    return int.from_bytes(uc.mem_read(address, 4), 'big')


class PanelArrival:
    """DMA arrivals on the shared clock; IRQ delivery respects INTC/IPL.

    Unlike Session._feed, this never raises an unlevelled vector immediately.
    No guest context is saved/restored, and bytes are consumed by the native ISR.
    """
    def __init__(self, session, events):
        self.s = session
        self.events = collections.deque(events)
        self.pending = False
        self.arrivals = []
        self.deliveries = []

    def step(self, done, remaining=None):
        from emu.pit import PENDING_STEP
        if self.events:
            until = max(1, self.events[0][0] - done)
            return min(until, PENDING_STEP) if self.pending else until
        return PENDING_STEP if self.pending else None

    def service(self, done):
        from emu import panelin
        from emu.pit import interrupt_level
        from unicorn.m68k_const import UC_M68K_REG_SR
        s, uc = self.s, self.s.m.uc
        while self.events and self.events[0][0] <= done:
            due, code, down = self.events.popleft()
            pos = s.held.press(code) if down else s.held.release(code)
            if pos is None:
                raise RuntimeError('button is not on the mk1 panel')
            base = u32(uc, s.profile.uart8_ring_ptr)
            dest = u32(uc, panelin.TCD34_DADDR)
            for byte in panelin.encode_buttons(*pos):
                uc.mem_write(dest, bytes([byte]))
                dest = base + ((dest - base + 1) & panelin.RING_MASK)
            uc.mem_write(panelin.TCD34_DADDR, struct.pack('>I', dest))
            self.arrivals.append({'code': code, 'down': down,
                                  'due_cycle': due - s._t0,
                                  'arrival_timer_cycle': done - s._t0,
                                  'arrival_cycle': s.clock.cycles})
            self.pending = True
        level = interrupt_level(s.m, panelin.RX_VECTOR)
        if (self.pending and level is not None and
                ((uc.reg_read(UC_M68K_REG_SR) >> 8) & 7) < level):
            if s.m.raise_vector(panelin.RX_VECTOR, level=level):
                self.deliveries.append(s.clock.cycles)
                self.pending = False
                return True
        return False


class PanelLatency:
    """Match queue records to native dispatch, then main-loop completion.

    Record layout is checked dynamically against the scheduled mk1 code and
    press/release flags. A main-loop return is dispatch-pass completion, not
    proof that the display is visible or an asynchronous operation completed.
    """
    def __init__(self, s):
        from unicorn.m68k_const import UC_M68K_REG_A2, UC_M68K_REG_A7, UC_M68K_REG_D0
        self.events = []
        self.pending = collections.defaultdict(collections.deque)
        self.popped = {}
        self.active = None
        self.max_depth = 0
        self.unmatched = 0
        self.s = s
        p, uc = s.profile, s.m.uc
        required = ('queue_send', 'ui_queue', 'mainloop', 'ui_key_dispatch')
        if any(getattr(p, name) is None for name in required):
            raise RuntimeError('mk1 UI hook resolution incomplete')

        def send(*_):
            sp = uc.reg_read(UC_M68K_REG_A7)
            if u32(uc, sp + 4) != p.ui_queue:
                return
            item = u32(uc, sp + 8)
            raw = bytes(uc.mem_read(item, 16))
            self.max_depth = max(self.max_depth, u32(uc, p.ui_queue + 4) + 1)
            if raw[0] == 0:
                code, flags, stamp = struct.unpack('>III', raw[4:])
                row = dict(item=item, code=code, flags=flags, stamp=stamp,
                           enqueue_cycle=s.clock.cycles)
                self.events.append(row)
                self.pending[item].append(row)

        def pop(*_):
            item = uc.reg_read(UC_M68K_REG_D0)
            if self.pending[item]:
                row = self.pending[item].popleft()
                row['pop_cycle'] = s.clock.cycles
                self.popped[item] = row

        def dispatch(*_):
            item = uc.reg_read(UC_M68K_REG_A2)
            row = self.popped.pop(item, None)
            if row is None:
                self.unmatched += 1
                return
            row['dispatch_cycle'] = s.clock.cycles
            self.active = row

        def done(*_):
            if self.active is not None:
                self.active['complete_cycle'] = s.clock.cycles
                self.active = None

        s.at(p.queue_send, send)
        s.at(p.mainloop + 12, pop)
        s.at(p.ui_key_dispatch, dispatch)
        s.at(p.mainloop, done)

    def report(self):
        rows = []
        for event in self.events:
            row = {k: v for k, v in event.items() if k not in ('item', 'stamp')}
            for name in ('pop', 'dispatch', 'complete'):
                if name + '_cycle' in row:
                    row['enqueue_to_' + name + '_ms'] = round(
                        (row[name + '_cycle'] - row['enqueue_cycle']) * 1000 /
                        self.s.ips, 6)
            rows.append(row)
        return {'events': rows, 'max_queue_depth': self.max_depth,
                'unmatched_dispatches': self.unmatched,
                'unfinished': sum('complete_cycle' not in e for e in rows)}


def add_reschedule(s):
    from emu import intfrc
    irq = intfrc.ForcedInterrupts(s.m, sources=(13,), base=0xfc050000,
                                 first_vector=192)
    if u32(s.m.uc, 0xfc050014) & (1 << 13):
        irq.asserted.add(13)
    s.sources += (irq,)


def prepare(candidate, destination):
    """Select USB1 through the native UI; HLE setup is outside measurement."""
    from emu.session import Session
    from emu import snapshot
    root, _, symbols, fw = dynamic._fixture(candidate)
    dynamic._configure(fw)
    s = Session(str(root / 'assigned.snap'), on_machine=add_reschedule)
    try:
        s.run_ms(150)
        dynamic._select_usb1(s, symbols)
        if s.halted:
            raise RuntimeError(s.halted)
        snapshot.save(s.m, str(destination),
                      components=s.ev['checkpoint_components'],
                      manifest=s.ev['checkpoint_manifest'])
    finally:
        s.close()


def run_profile(candidate, snapshot_path, speed, depth, fsys, duration):
    from emu.session import Session
    from emu.cftiming import deadline_report
    root, meta, symbols, fw = dynamic._fixture(candidate)
    dynamic._configure(fw)
    sym = lambda name: int(symbols[name], 0)
    state = {'render_blocks': 0, 'effective_changes': 0, 'starts': 0,
             'raw_param_changes': 0, 'native_usb_submit_calls': 0,
             'native_rx_bridge_calls': 0}
    last = None
    last_raw = None

    def setup(s):
        nonlocal last, last_raw
        add_reschedule(s)
        assert sym('worker_sem') in s.ev['unblock_skip']
        s.panel_latency = PanelLatency(s)
        uc = s.m.uc
        # Verified native LFO call site 0x40077ea2 loads this tempo word;
        # OS uses BPM*120. Set the disposable project to 120 BPM before run.
        uc.mem_write(0x4020df10, struct.pack('>I', 14400))
        kit = u32(uc, sym('fw_kit'))
        # Disposable native sound storage, as in i03_dynamic_probe. No fake
        # per-render parameter mutation. Shape/multiplier/etc remain explicit.
        slots = dynamic._sound_slots(uc, kit, 0)
        # Native formatters: SPD=raw-0x4000 (0x40065842),
        # DEP=2*raw-0x8000 (0x4006585c), both expressed in 8.8.
        # Verify the relevant instruction bytes before using this mapping.
        assert bytes(uc.mem_read(0x40065842, 10)) == bytes.fromhex('203cffffc000d1af0008')
        assert bytes(uc.mem_read(0x4006585c, 12)) == bytes.fromhex('202f0008d0800680ffff8000')
        # Positive SPD maximum is 0x7ffe (~+64), never signed 0x8000.
        for slot, value in {1: min(0x7ffe, (speed + 64) << 8),
                            2: 4 << 8, 3: 64 << 8, 4: 17 << 8, 5: 0,
                            6: 0, 7: 0, 8: 0x4000 + (depth << 7),
                            17: 60 << 8}.items():
            slots[slot] = value
        dynamic._write_sound_slots(uc, kit, 0, slots)
        # Trigger the native track, then request SRC; input is IRQ scheduled.
        schedule = [(10, 24, True), (40, 24, False),
                    (60, 20, True), (90, 20, False)]
        source = PanelArrival(s, [(s._t0 + int(ms * fsys / 1000), code, down)
                                 for ms, code, down in schedule])
        s.sources += (source,)
        s.panel_source = source

        def render(*_):
            nonlocal last, last_raw
            state['render_blocks'] += 1
            raw = bytes(uc.mem_read(sym('fw_voice_params'), 16))
            values = struct.unpack('>8h', raw)
            # Mirrors capture_control: integer TRX controls, clamped 0..127.
            effective = tuple(max(0, min(127, int(v / 256))) for v in values)
            if last is not None and effective != last:
                state['effective_changes'] += 1
            if last_raw is not None and raw != last_raw:
                state['raw_param_changes'] += 1
            last, last_raw = effective, raw
            state['starts'] += bool(u32(uc, sym('fw_voice_start')) & 1)

        def submit(*_):
            state['native_usb_submit_calls'] += 1

        def receive(*_):
            state['native_rx_bridge_calls'] += 1

        def lfo(*_):
            from unicorn.m68k_const import UC_M68K_REG_A7
            state['native_lfo_tempo_argument'] = u32(uc, uc.reg_read(UC_M68K_REG_A7) + 8)

        s.at(dynamic.PRE, render)
        s.at(0x400ed766, lfo)
        s.at(0x40004832, submit)
        s.at(sym('native_rx'), receive)
        state['sound_lfo_slots_1_to_8'] = slots[1:9]
        state['display_speed_8_8'] = slots[1] - 0x4000
        state['display_depth_8_8'] = 2 * slots[8] - 0x8000
        state['machine_id'] = uc.mem_read(kit + dynamic.KIT_SOUND + 126, 1)[0]

    s = Session(str(snapshot_path), audio=True, hle=False,
                timing={'fsys': fsys}, on_machine=setup)
    try:
        worker_before = u32(s.m.uc, sym('usb_track_worker_runs'))
        s.run_ms(duration)
        clock = s.clock.report()
        latency = s.panel_latency.report()
        for row, arrival in zip(latency['events'], s.panel_source.arrivals):
            if row['code'] == arrival['code'] and 'complete_cycle' in row:
                row['arrival_to_complete_ms'] = round(
                    (row['complete_cycle'] - arrival['arrival_cycle']) * 1000 / fsys, 6)
        state['effective_changes_per_s'] = round(
            state['effective_changes'] * 1000 / s.now_ms(), 3)
        state['worker_runs'] = u32(s.m.uc, sym('usb_track_worker_runs')) - worker_before
        state['worker_guard_faults'] = u32(s.m.uc, sym('usb_track_worker_guard_faults'))
        state['gate'] = u32(s.m.uc, sym('usb_track_gate'))
        state['usb_ready'] = u32(s.m.uc, 0x40610474)
        state['usb_queued'] = u32(s.m.uc, 0x40610478)
        state['usb_irq_vector209_count'] = clock['vectors'].get(209, {}).get('count', 0)
        hooks = {name: '0x%08x' % getattr(s.profile, name) for name in
                 ('queue_send', 'ui_queue', 'mainloop', 'ui_key_dispatch')}
        expected = [(24, 1), (24, 16), (20, 1), (20, 16)]
        observed = [(e['code'], e['flags'] & 17) for e in latency['events']]
        return {
            'profile': {'speed': speed, 'depth': depth,
                        'scope': 'native LFO/panel, disconnected; not physical profile replay'},
            'duration_ms': s.now_ms(), 'halted': s.halted,
            'cycle_minus_timer_clock': s.clock.cycles - (s.pits.now - s._t0),
            'hooks': hooks, 'panel_hook_sequence_valid': observed == expected,
            'panel': latency, 'panel_arrivals': s.panel_source.arrivals,
            'panel_irq_delivery_cycles': s.panel_source.deliveries,
            'counters': state, 'cycle_model': clock,
            'audio_irq_deadlines': deadline_report(s.clock, 191, fsys * 32 / 48000),
            'artificial_waits_by_sem': {'0x%08x' % k: v for k, v in
                                         sorted(s.ev['satisfied_by_sem'].items())},
            'artificial_waits_by_caller': {'0x%08x' % k: v for k, v in
                                            sorted(s.ev['satisfied_by'].items())},
            'native_worker_wait_faked': s.ev['satisfied_by_sem'].get(sym('worker_sem'), 0),
            'irq_sources': [type(x).__name__ for x in s.sources],
            'timer_sources': [type(x).__name__ for x in s.pits.sources],
        }
    finally:
        s.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fsys', type=int, default=250_000_000,
                        help='assumed core Hz, not calibrated DTIM0 frequency')
    parser.add_argument('--duration-ms', type=int, default=500)
    args = parser.parse_args()
    if args.duration_ms < 120 or args.fsys <= 0:
        parser.error('duration must be >=120 ms and fsys positive')
    root, meta, _, _ = dynamic._fixture(args.candidate)
    reports = []
    with tempfile.TemporaryDirectory(prefix='v01-contention-', dir=root) as temp:
        selected = Path(temp) / 'usb1.snap'
        print('Preparing disposable USB1 snapshot (outside timing window)', flush=True)
        prepare(root, selected)
        prepared_sha = hashlib.sha256(selected.read_bytes()).hexdigest()
        for speed, depth in ((32, 32), (64, 48)):
            print('Native model preflight SPD%d/DEP%d' % (speed, depth), flush=True)
            reports.append(run_profile(root, selected, speed, depth, args.fsys, args.duration_ms))
    valid = all(not r['halted'] and r['panel_hook_sequence_valid'] and
                not r['panel']['unfinished'] and
                not r['panel']['unmatched_dispatches'] and
                r['counters']['machine_id'] == 121 and
                r['counters']['starts'] == 1 and
                r['counters']['effective_changes'] > 1 and
                not r['cycle_model']['decode_mismatches'] and
                not r['native_worker_wait_faked'] for r in reports)
    report = {
        'task': 'V01 Gate A', 'status': 'emulator-gap' if valid else 'invalid-preflight',
        'schema': 'v01-contention-preflight/1',
        'hardware_test': False, 'gate_a_verdict': 'NOT SCORED',
        'candidate_sha256': meta['syx_sha256'],
        'stock_main_sha256': meta['stock_main_sha256'],
        'snapshot_sha256': hashlib.sha256((root / 'assigned.snap').read_bytes()).hexdigest(),
        'prepared_snapshot_sha256': prepared_sha,
        'probe_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'emulator_revision': subprocess.check_output(
            ['git', '-C', str(r03_probe.EMU), 'rev-parse', 'HEAD'], text=True).strip(),
        'profiles': reports,
        'limits': [
            'No USB controller DMA/IRQ or ACK arrival model; active G transport is not exercised.',
            'No UI runnable/wait attribution or active-link regression verdict is possible here.',
            'Cycle estimates assume zero-wait memory, omit DDR/bus contention and misalignment.',
            'Core frequency is an assumption; 132MHz DTIM0 calibration is not CPU frequency.',
            'Native sound slot seeds do not establish the complete physical LFO/pattern profile.',
            'Main-loop return measures dispatch-pass completion, not visible screen latency.',
            'CycleClock precharges whole blocks; hook timestamps are block-granular estimates.',
            'Asynchronous exception-entry costs advance CycleClock outside the timer step; clocks differ slightly.',
            'Snapshot was created with emulator shortcuts; native replay does not validate its history.',
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(report['status'] + '; Gate A NOT SCORED', flush=True)
    return 0 if valid else 1


if __name__ == '__main__':
    raise SystemExit(main())
