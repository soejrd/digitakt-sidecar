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


def audio_stream_valid(counters):
    """Require sustained native production, continuous counters and return credits.

    Eleven native startup packets plus one render block bound edge effects;
    completing only the initial descriptor queue cannot pass a longer run.
    """
    audio = counters.get('peer', {}).get('audio', {})
    packets = audio.get('in_packets', 0)
    return (packets > 0 and not audio.get('counter_gaps', 1) and
            audio.get('returns_scheduled') == packets and
            abs(packets * 7 - counters['render_blocks'] * 32) <= 11 * 7 + 32)


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


class UsbStatusArrival:
    """Raise native USB status209 at fixed shared-clock times.

    ISR209 itself forces INTC1 source6; the separate ForcedInterrupts source
    delivers deferred ISR134 only when the CPU IPL permits it. This models the
    demonstrated interrupt handoff, not USB DMA, descriptor completion or RX.
    """
    STATUS = 0xFC0B0144

    def __init__(self, session, schedule):
        self.s = session
        self.events = collections.deque(schedule)
        self.pending = False
        self.arrivals = []
        self.deliveries = []

    def step(self, done, remaining=None):
        from emu.pit import PENDING_STEP
        if self.events:
            until = max(1, self.events[0] - done)
            return min(until, PENDING_STEP) if self.pending else until
        return PENDING_STEP if self.pending else None

    def service(self, done):
        from emu.pit import interrupt_level
        from unicorn.m68k_const import UC_M68K_REG_SR
        s, uc = self.s, self.s.m.uc
        if not self.pending and self.events and self.events[0] <= done:
            due = self.events.popleft()
            uc.mem_write(self.STATUS, struct.pack('>I', 4))
            self.arrivals.append({'due_timer_cycle': due - s._t0,
                                  'arrival_timer_cycle': done - s._t0,
                                  'arrival_cycle': s.clock.cycles})
            self.pending = True
        level = interrupt_level(s.m, 209, respect_mask=False)
        if (self.pending and level is not None and
                ((uc.reg_read(UC_M68K_REG_SR) >> 8) & 7) < level):
            if s.m.raise_vector(209, level=level):
                self.deliveries.append(s.clock.cycles)
                self.pending = False
                return True
        return False

    def report(self):
        rows = []
        for row in self.arrivals:
            item = dict(row)
            item['arrival_lateness_ms'] = round(
                (row['arrival_timer_cycle'] - row['due_timer_cycle']) * 1000 /
                self.s.ips, 6)
            rows.append(item)
        return {'scheduled': len(self.arrivals), 'delivered_209': len(self.deliveries),
                'pending': int(self.pending), 'events': rows}


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


def add_usb_dispatch(s):
    """Watch INTC1 source6, the native ISR209 -> deferred ISR134 handoff."""
    from emu import intfrc
    irq = intfrc.ForcedInterrupts(s.m, sources=(6,), base=0xfc04c000,
                                 first_vector=128)
    if u32(s.m.uc, 0xfc04c014) & (1 << 6):
        irq.asserted.add(6)
    s.sources += (irq,)
    return irq


def prepare(candidate, destination, usb_device=False, usb_audio=False):
    """Select USB1 through the native UI; HLE setup is outside measurement."""
    from emu.session import Session
    from emu import snapshot
    root, _, symbols, fw = dynamic._fixture(candidate)
    dynamic._configure(fw)
    s = Session(str(root / 'assigned.snap'), on_machine=add_reschedule)
    try:
        s.run_ms(150)
        dynamic._select_usb1(s, symbols)
        if usb_device:
            from emu.harness import call
            from v01_usb_device_model import UsbDeviceModel
            from unicorn.m68k_const import UC_M68K_REG_PC
            # Native configuration/descriptor allocation outside measurement.
            # This substitutes enumeration, not submission or RX callbacks.
            model = UsbDeviceModel(s.m, s.ips)
            model.configure_component()
            ctx = s.m.uc.context_save()
            try:
                s.m._fast_stepper_obj.left = 1000000
                call(s.m, 0x4000463e, [1], limit=1000000)
                assert s.m.uc.reg_read(UC_M68K_REG_PC) == 0xdeadbee0
                s.m._fast_stepper_obj.left = 1000000
                call(s.m, 0x40004516, [], limit=1000000)
                assert s.m.uc.reg_read(UC_M68K_REG_PC) == 0xdeadbee0
                if usb_audio:
                    # QH setup stores from native mode6 initializer 0x4000386c;
                    # run native pool clearing and audio stream constructors.
                    # The checkpoint already has synchronized audio flags.
                    # Native mode6 setup first stops both streams (0x40003852);
                    # retain those native resets before rebuilding queue heads.
                    for target in (0x4000332a, 0x40003394):
                        s.m._fast_stepper_obj.left = 1000000
                        call(s.m, target, [], limit=1000000)
                        assert s.m.uc.reg_read(UC_M68K_REG_PC) == 0xdeadbee0
                    for qh in (0x80000180, 0x800001c0):
                        s.m.uc.mem_write(qh, bytes(64))
                        s.m.uc.mem_write(qh, struct.pack('>I', 0x04000000))
                        s.m.uc.mem_write(qh + 8, struct.pack('>I', 0xdead0001))
                    for target in (0x400038fc, 0x4000335e, 0x400032e4):
                        s.m._fast_stepper_obj.left = 1000000
                        call(s.m, target, [], limit=1000000)
                        assert s.m.uc.reg_read(UC_M68K_REG_PC) == 0xdeadbee0
                    s.m.uc.mem_write(0xfc0b01cc, struct.pack('>I', 0x00cc00cc))
            finally:
                s.m.uc.context_restore(ctx)
            # Component precondition: already synchronized Overbridge route.
            # Enumeration is omitted; --usb-audio runs native stream queues.
            # Never repeat these synchronization stores per render.
            s.m.uc.mem_write(0x406104f4, struct.pack('>I', 6))
            s.m.uc.mem_write(0x4025c294, struct.pack('>I', 1))
        if s.halted:
            raise RuntimeError(s.halted)
        snapshot.save(s.m, str(destination),
                      components=s.ev['checkpoint_components'],
                      manifest=s.ev['checkpoint_manifest'])
    finally:
        s.close()


def run_profile(candidate, snapshot_path, speed, depth, fsys, duration,
                usb_status_interval_ms=None, usb_device=False, peer_library=None,
                profile_execution=False, usb_periodic=False, usb_audio=False):
    from emu.session import Session
    from emu.cftiming import deadline_report
    root, meta, symbols, fw = dynamic._fixture(candidate)
    dynamic._configure(fw)
    sym = lambda name: int(symbols[name], 0)
    state = {'render_blocks': 0, 'effective_changes': 0, 'starts': 0,
             'raw_param_changes': 0, 'native_usb_submit_calls': 0,
             'native_rx_bridge_calls': 0, 'sampled_source_queue_peak': 0,
             'activate_service_age_blocks_max': 0}
    last = None
    last_raw = None

    def setup(s):
        nonlocal last, last_raw
        add_reschedule(s)
        usb_forced = (add_usb_dispatch(s)
                      if (usb_status_interval_ms or usb_device) else None)
        s.usb_forced = usb_forced
        s.usb_device = None
        s.usb_irq = None
        s.usb_peer = None
        if usb_device:
            from v01_usb_device_model import UsbDeviceModel, UsbStatusIrq
            model = UsbDeviceModel(s.m, fsys, periodic=usb_periodic)
            model.adopt_configured_queues(audio=usb_audio)
            irq = UsbStatusIrq(model)
            s.sources += (model, irq)
            s.usb_device, s.usb_irq = model, irq
            if peer_library is not None:
                from v01_usb_peer import UsbPeer
                peer = UsbPeer(s, model, sym('usb_track_clock'), peer_library, audio=usb_audio)
                s.usb_peer = peer
                s.sources += (peer,)
        assert sym('worker_sem') in s.ev['unblock_skip']
        s.panel_latency = PanelLatency(s)
        s.execution_profile = None
        if profile_execution:
            from v01_execution_profile import ExecutionProfile
            s.execution_profile = ExecutionProfile(s, symbols)
            from v01_worker_spans import WorkerSpans
            s.worker_spans = WorkerSpans(s, symbols)
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
        if peer_library is not None and duration >= 500:
            schedule += [(duration - 150, s.code('FLTR'), True),
                         (duration - 120, s.code('FLTR'), False),
                         (duration - 70, s.code('SRC'), True),
                         (duration - 40, s.code('SRC'), False)]
        s.expected_panel_edges = [(code, 1 if down else 16) for _, code, down in schedule]
        source = PanelArrival(s, [(s._t0 + int(ms * fsys / 1000), code, down)
                                 for ms, code, down in schedule])
        s.sources += (source,)
        s.panel_source = source
        s.usb_status = None
        if usb_status_interval_ms:
            due = [s._t0 + int(ms * fsys / 1000)
                   for ms in range(usb_status_interval_ms, duration - 10,
                                   usb_status_interval_ms)]
            status = UsbStatusArrival(s, due)
            s.sources += (status,)
            s.usb_status = status

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
            sample_queue()

        def sample_queue(*_):
            queue = sym('control') + 36 * 256
            occupancy = (u32(uc, queue) - u32(uc, queue + 4)) & 0xffffffff
            state['sampled_source_queue_peak'] = max(state['sampled_source_queue_peak'], occupancy)

        def control_receive(*_):
            from unicorn.m68k_const import UC_M68K_REG_A7
            control = u32(uc, uc.reg_read(UC_M68K_REG_A7) + 8)
            if uc.mem_read(control + 12, 1)[0] == 3:  # ACTIVATE names source block
                age = (u32(uc, sym('usb_track_clock')) - u32(uc, control + 8)) & 0xffffffff
                state['activate_service_age_blocks_max'] = max(state['activate_service_age_blocks_max'], age)

        def submit(*_):
            state['native_usb_submit_calls'] += 1

        def receive(*_):
            from unicorn.m68k_const import UC_M68K_REG_SR
            state['native_rx_bridge_calls'] += 1
            levels = state.setdefault('native_rx_interrupt_levels', {})
            level = (uc.reg_read(UC_M68K_REG_SR) >> 8) & 7
            levels[str(level)] = levels.get(str(level), 0) + 1

        def lfo(*_):
            from unicorn.m68k_const import UC_M68K_REG_A7
            state['native_lfo_tempo_argument'] = u32(uc, uc.reg_read(UC_M68K_REG_A7) + 8)

        if usb_audio:
            state['native_audio_tx_alignment_samples'] = []
            def audio_alignment(*_):
                rows = state['native_audio_tx_alignment_samples']
                if len(rows) < 8:
                    rows.append({hex(a): u32(uc, a) for a in (
                        0x4399eb74, 0x4399eca4, 0xfc07800c, 0x4025c258,
                        0x4025c268, 0x4025c26c, 0x4025c25c, 0x4025c298)})
            s.at(0x4000343a, audio_alignment)
        s.at(dynamic.PRE, render)
        s.at(0x400ed766, lfo)
        s.at(0x40004832, submit)
        s.at(sym('native_rx'), receive)
        s.at(sym('usb_track_tick'), sample_queue)
        s.at(sym('sc_dev_receive_control'), control_receive)
        state['sound_lfo_slots_1_to_8'] = slots[1:9]
        state['display_speed_8_8'] = slots[1] - 0x4000
        state['display_depth_8_8'] = 2 * slots[8] - 0x8000
        state['machine_id'] = uc.mem_read(kit + dynamic.KIT_SOUND + 126, 1)[0]

    s = Session(str(snapshot_path), audio=True, hle=False,
                timing={'fsys': fsys}, on_machine=setup)
    try:
        worker_before = u32(s.m.uc, sym('usb_track_worker_runs'))
        diagnostic_names = tuple(name for name in (
            'v01_send_calls', 'v01_send_ticks', 'v01_work_ticks', 'v01_render_ticks')
            if name in symbols)
        diagnostic_before = {name: u32(s.m.uc, sym(name)) for name in diagnostic_names}

        s.run_ms(duration)
        clock = s.clock.report()
        latency = s.panel_latency.report()
        for row, arrival in zip(latency['events'], s.panel_source.arrivals):
            if row['code'] == arrival['code'] and 'complete_cycle' in row:
                row['arrival_to_complete_ms'] = round(
                    (row['complete_cycle'] - arrival['arrival_cycle']) * 1000 / fsys, 6)
        diagnostic_delta = {name: (u32(s.m.uc, sym(name)) - value) & 0xffffffff
                            for name, value in diagnostic_before.items()}
        source_seconds = state['render_blocks'] * 32 / 48000
        state['existing_firmware_elapsed_diagnostics'] = {
            'deltas': diagnostic_delta, 'source_elapsed_s': source_seconds,
            'model_timer_hz': 132000000,
            'fractions': {name: (diagnostic_delta[name] / (132000000 * source_seconds)
                                  if source_seconds else None)
                          for name in diagnostic_names if name.endswith('_ticks')},
            'counter_read_clock': ('instruction-position' if any(
                callable(getattr(t, 'counter_clock', None)) for t in s.pits.sources)
                else 'step-boundary'),
            'suitable_for_hardware_cost_calibration': False,
            'limits': 'Model brackets depend on assumed clocks and zero-wait costs. '
                      'Worker/render overlap: never add as CPU. Bus assumed132MHz.'}
        state['effective_changes_per_s'] = round(
            state['effective_changes'] * 1000 / s.now_ms(), 3)
        state['worker_runs'] = u32(s.m.uc, sym('usb_track_worker_runs')) - worker_before
        state['worker_guard_faults'] = u32(s.m.uc, sym('usb_track_worker_guard_faults'))
        state['gate'] = u32(s.m.uc, sym('usb_track_gate'))
        state['usb_ready'] = u32(s.m.uc, 0x40610474)
        state['usb_queued'] = u32(s.m.uc, 0x40610478)
        state['native_midi_index'] = u32(s.m.uc, 0x4061047c)
        state['native_rx_slot'] = '0x%08x' % u32(s.m.uc, 0x4061046c)
        state['adapter_state'] = {name: s.m.uc.mem_read(sym(name), 1)[0]
                                  for name in ['usb-midi:opened', 'usb-midi:failed',
                                               'usb-track:running', 'usb-track:initialized']}
        state['route'] = u32(s.m.uc, sym('usb_track_route'))
        queue = sym('control') + 36 * 256
        state['source_queue_end'] = (u32(s.m.uc, queue) - u32(s.m.uc, queue + 4)) & 0xffffffff
        state['source_queue_overflow'] = u32(s.m.uc, queue + 8)
        state['source_session_fatal'] = s.m.uc.mem_read(queue + 32, 1)[0]
        state['usb_irq_vector209_count'] = clock['vectors'].get(209, {}).get('count', 0)
        state['usb_irq_vector134_count'] = clock['vectors'].get(134, {}).get('count', 0)
        # STATUS is the injected peripheral register and is not self-clearing
        # in this bounded model. ISR209 latches into 0x406104d8; ISR134 clears
        # that native deferred-status word.
        state['usb_status_register'] = u32(s.m.uc, UsbStatusArrival.STATUS)
        state['usb_deferred_status_pending'] = u32(s.m.uc, 0x406104d8)
        state['usb_intc1_source6_pending'] = bool(
            u32(s.m.uc, 0xfc04c014) & (1 << 6))
        state['usb_status_source6_fired'] = (
            dict(s.usb_forced.fired) if s.usb_forced is not None else {})
        if s.usb_device is not None:
            state['usb_device'] = s.usb_device.report()
            state['usb_209_deliveries'] = s.usb_irq.deliveries
            state['usb_209_blocked_by_mask'] = s.usb_irq.blocked_by_mask
            state['usb_209_blocked_by_ipl'] = s.usb_irq.blocked_by_ipl
        if s.usb_peer is not None:
            state['peer'] = s.usb_peer.report()
        usb = s.usb_status.report() if s.usb_status is not None else None
        hooks = {name: '0x%08x' % getattr(s.profile, name) for name in
                 ('queue_send', 'ui_queue', 'mainloop', 'ui_key_dispatch')}
        expected = s.expected_panel_edges
        observed = [(e['code'], e['flags'] & 17) for e in latency['events']]
        return {
            'profile': {'speed': speed, 'depth': depth,
                        'scope': ('native LFO/panel + native USB MIDI and silent audio component' if usb_audio else
                                  'native LFO/panel + native USB MIDI control component; no USB audio'
                                  if peer_library else
                                  ('configured USB descriptor component; no active peer' if usb_device else
                                   'native LFO/panel, disconnected; not physical profile replay'))},
            'duration_ms': s.now_ms(), 'halted': s.halted,
            'cycle_minus_timer_clock': s.clock.cycles - (s.pits.now - s._t0),
            'hooks': hooks, 'panel_hook_sequence_valid': observed == expected,
            'panel': latency, 'panel_arrivals': s.panel_source.arrivals,
            'panel_irq_delivery_cycles': s.panel_source.deliveries,
            'counters': state, 'cycle_model': clock,
            'execution_profile': s.execution_profile.report() if s.execution_profile else None,
            'worker_spans': s.worker_spans.report() if s.execution_profile else None,
            'usb_status': usb,
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
    parser.add_argument('--usb-status-interval-ms', type=int, default=None,
                        help='schedule native USB status209 at this interval')
    parser.add_argument('--usb-device', action='store_true',
                        help='model native USB descriptor prime/completion')
    parser.add_argument('--usb-peer', action='store_true',
                        help='schedule portable peer replies through native RX DMA')
    parser.add_argument('--profile-execution', action='store_true',
                        help='partition estimated block costs by native task/IRQ and selected PC ranges')
    parser.add_argument('--usb-periodic', action='store_true',
                        help='drive native USB GPT0 and high-speed SOF events from documented hardware periods')
    parser.add_argument('--usb-audio', action='store_true',
                        help='drive native mode6 audio descriptors with H01 credit-based silent return packets')
    args = parser.parse_args()
    if args.duration_ms < 120 or args.fsys <= 0:
        parser.error('duration must be >=120 ms and fsys positive')
    if args.usb_status_interval_ms is not None and args.usb_status_interval_ms < 2:
        parser.error('USB status interval must be >=2 ms')
    if args.usb_peer and not args.usb_device:
        parser.error('--usb-peer requires --usb-device')
    if args.usb_periodic and not args.usb_device:
        parser.error('--usb-periodic requires --usb-device')
    if args.usb_audio and not (args.usb_peer and args.usb_periodic):
        parser.error('--usb-audio requires --usb-peer and --usb-periodic')
    if args.usb_device and args.usb_status_interval_ms:
        parser.error('descriptor IRQs and artificial status injection are separate modes')
    peer_library = None
    if args.usb_peer:
        from v01_usb_peer import build
        peer_library = build()
    root, meta, _, _ = dynamic._fixture(args.candidate)
    reports = []
    with tempfile.TemporaryDirectory(prefix='v01-contention-', dir=root) as temp:
        selected = Path(temp) / 'usb1.snap'
        print('Preparing disposable USB1 snapshot (outside timing window)', flush=True)
        prepare(root, selected, args.usb_device, args.usb_audio)
        prepared_sha = hashlib.sha256(selected.read_bytes()).hexdigest()
        for speed, depth in ((32, 32), (64, 48)):
            print('Native model preflight SPD%d/DEP%d' % (speed, depth), flush=True)
            reports.append(run_profile(root, selected, speed, depth, args.fsys,
                                       args.duration_ms, args.usb_status_interval_ms,
                                       args.usb_device, peer_library, args.profile_execution,
                                       args.usb_periodic, args.usb_audio))
    valid = all(not r['halted'] and r['panel_hook_sequence_valid'] and
                not r['panel']['unfinished'] and
                not r['panel']['unmatched_dispatches'] and
                all(e.get('arrival_to_complete_ms', float('inf')) <= 100
                    for e in r['panel']['events']) and
                r['counters']['machine_id'] == 121 and
                r['counters']['starts'] == 1 and
                r['counters']['effective_changes'] > 1 and
                not r['cycle_model']['decode_mismatches'] and
                not r['native_worker_wait_faked'] and
                (args.usb_device or not args.usb_status_interval_ms or (
                    r['usb_status'] is not None and
                    r['usb_status']['scheduled'] > 1 and
                    r['usb_status']['pending'] == 0 and
                    r['usb_status']['delivered_209'] ==
                    r['usb_status']['scheduled'] and
                    r['counters']['usb_irq_vector134_count'] >=
                    r['usb_status']['scheduled'] and
                    r['counters']['usb_deferred_status_pending'] == 0 and
                    not r['counters']['usb_intc1_source6_pending'])) and
                (not args.usb_device or (
                    r['counters'].get('usb_device', {}).get('primed', 0) > 0 and
                    r['counters'].get('usb_device', {}).get('completed', 0) > 0 and
                    r['counters'].get('usb_209_deliveries', 0) > 0 and
                    not r['counters'].get('usb_device', {}).get('errors'))) and
                (not args.usb_peer or (
                    r['counters'].get('peer', {}).get('allowed') and
                    r['counters'].get('peer', {}).get('starts', 0) >= 1 and
                    r['counters']['gate'] == 1 and
                    r['counters'].get('peer', {}).get('sessions_seen') == 1 and
                    not r['counters']['source_queue_overflow'] and
                    not r['counters']['source_session_fatal'] and
                    r['counters']['native_rx_bridge_calls'] > 0 and
                    set(r['counters'].get('native_rx_interrupt_levels', {})) == {'0'}))
                and (not args.usb_audio or (
                    audio_stream_valid(r['counters'])))
                and (not args.profile_execution or (
                    r['worker_spans']['complete_batches'] > 0 and
                    r['worker_spans']['invalid_spans'] == 0))
                for r in reports)
    report = {
        'task': 'V01 Gate A', 'status': 'emulator-gap' if valid else 'invalid-preflight',
        'schema': 'v01-contention-preflight/2',
        'usb_dispatch': ('native-device-model' if args.usb_device else
                         ('scheduled-status209-intc1-source6' if
                          args.usb_status_interval_ms else 'absent')),
        'hardware_test': False, 'gate_a_verdict': 'NOT SCORED',
        'component_panel_limit_ms': 100,
        'component_verdict': 'within-panel-limit' if valid else 'failed-or-incomplete',
        'candidate_sha256': meta['syx_sha256'],
        'stock_main_sha256': meta['stock_main_sha256'],
        'snapshot_sha256': hashlib.sha256((root / 'assigned.snap').read_bytes()).hexdigest(),
        'prepared_snapshot_sha256': prepared_sha,
        'probe_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'helper_source_sha256': {name: hashlib.sha256((Path(__file__).parent / name).read_bytes()).hexdigest()
                                 for name in ['v01_usb_device_model.py', 'v01_usb_peer.py',
                                              'v01_execution_profile.py', 'v01_worker_spans.py'] if args.usb_device or args.profile_execution},
        'emulator_revision': subprocess.check_output(
            ['git', '-C', str(r03_probe.EMU), 'rev-parse', 'HEAD'], text=True).strip(),
        'emulator_patch_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in sorted((Path(__file__).resolve().parents[1] /
                                                   'patches/digiemu').glob('*.patch'))},
        'profiles': reports,
        'limits': [
            ('Experimental native audio DMA with silent credit-based returns; external DTIM2 clock is absent and audio validity must pass before latency attribution.' if args.usb_audio else
             'Native USB MIDI control is modelled, but audio endpoint traffic/PCM, enumeration and physical host/bus scheduling are absent.'
             if args.usb_device else 'No USB descriptor completion, RX or ACK arrival model; active G transport is not exercised.'),
            ('USB-device setup calls native MIDI configuration outside measurement and seeds configured RUN/UI-IRQ/queue-base and synchronized Overbridge preconditions once; enumeration is not modelled.'
             if args.usb_device else 'Disconnected snapshot; no USB enumeration is driven.'),
            'Source queue peak is sampled at render/worker entries; it is a lower bound, not an exact watermark.',
            'No complete-machine UI runnable/wait attribution or physical Gate A regression verdict is possible here.',
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
