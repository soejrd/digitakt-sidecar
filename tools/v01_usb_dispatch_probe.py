#!/usr/bin/env python3
"""Exercise the native 209 -> INTC1 source6 -> 134 USB interrupt handoff.

One scheduled status event tests routing only. It does not emulate USB DMA,
packet completion, host timing, ACK traffic or the physical contention failure.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

import i03_dynamic_probe as dynamic
from v01_contention_probe import add_reschedule, u32


class OneUsbStatus:
    def __init__(self, session):
        self.s = session
        self.due = session._t0 + session.ips // 1000
        self.fired = False

    def step(self, done, remaining=None):
        if not self.fired:
            return max(1, self.due - done)
        return None

    def service(self, done):
        from emu.pit import interrupt_level
        from unicorn.m68k_const import UC_M68K_REG_SR
        if self.fired or done < self.due:
            return False
        # Deliberate ISR-entry component test, not a controller IRQ model.
        # Use the native programmed priority; defer while CPU IPL masks it.
        level = interrupt_level(self.s.m, 209, respect_mask=False)
        assert level == 6
        if ((self.s.m.uc.reg_read(UC_M68K_REG_SR) >> 8) & 7) >= level:
            return False
        self.s.m.uc.mem_write(0xfc0b0144, struct.pack('>I', 4))
        self.fired = self.s.m.raise_vector(209, level=level)
        return self.fired


def case(candidate, model_deferred):
    from emu.session import Session
    from emu import intfrc
    root, meta, _, firmware = dynamic._fixture(candidate)
    dynamic._configure(firmware)
    state = {'usb_isr_entries': 0, 'deferred_isr_entries': 0}

    def machine(s):
        add_reschedule(s)
        uc = s.m.uc
        # Verify the exact native handoff/ack instructions before interpreting
        # the result: ISR209 sets bit6; ISR134 clears it on entry.
        assert bytes(uc.mem_read(0x400060e6, 8)) == bytes.fromhex('704081b9fc04c014')
        assert bytes(uc.mem_read(0x4000704c, 8)) == bytes.fromhex('70bfc1b9fc04c014')
        uc.mem_write(0x406104d8, bytes(4))
        uc.mem_write(0xfc0b01a4, bytes(4))  # no port-change branch in this test
        if model_deferred:
            irq = intfrc.ForcedInterrupts(s.m, sources=(6,),
                                         base=0xfc04c000, first_vector=128)
            s.sources += (irq,)
        source = OneUsbStatus(s)
        s.sources += (source,)

        def first(*_):
            state['usb_isr_entries'] += 1

        def deferred(*_):
            state['deferred_isr_entries'] += 1

        s.at(0x4000605e, first)
        s.at(0x40007044, deferred)

    s = Session(str(root / 'assigned.snap'), audio=True, hle=False,
                timing={'fsys': 250_000_000}, on_machine=machine)
    try:
        s.run_ms(10)
        state.update({
            'deferred_source_modelled': model_deferred,
            'pending_status': u32(s.m.uc, 0x406104d8),
            'intc1_source6_pending': bool(u32(s.m.uc, 0xfc04c014) & 64),
            'halted': s.halted,
            'decode_mismatches': s.clock.decode_mismatches,
            'artificial_waits': sum(s.ev['satisfied_by_sem'].values()),
            'cycle_model': s.clock.report(),
        })
        return state
    finally:
        s.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    _, meta, _, _ = dynamic._fixture(args.candidate)
    missing = case(args.candidate, False)
    modelled = case(args.candidate, True)
    assert missing['usb_isr_entries'] == modelled['usb_isr_entries'] == 1
    assert missing['deferred_isr_entries'] == 0
    assert missing['intc1_source6_pending'] and missing['pending_status'] == 4
    assert modelled['deferred_isr_entries'] == 1
    assert not modelled['intc1_source6_pending'] and modelled['pending_status'] == 0
    assert all(not c['halted'] and not c['decode_mismatches'] for c in (missing, modelled))
    result = {
        'schema': 'v01-usb-dispatch/1', 'status': 'verified-emulator-gap',
        'hardware_test': False, 'gate_a_verdict': 'NOT SCORED',
        'candidate_sha256': meta['syx_sha256'],
        'probe_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'without_intc1_source6': missing, 'with_intc1_source6': modelled,
        'finding': 'Native USB ISR209 forces INTC1 source6. Without that source, native deferred USB ISR134 never runs and status remains pending. Existing generic ForcedInterrupts can model this handoff.',
        'limits': ['One scheduled status4 ISR-entry test; not USB controller or DMA emulation.',
                   'The ISR-entry test bypasses the peripheral mask and respects native priority/CPU IPL.',
                   'Status4 exercises a bounded native status-clear branch, not RX/ACK or descriptor completion.',
                   'No physical UI-lag cause or firmware fix established.'],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('Native USB handoff: missing source retains status; modelled source dispatches/clears: PASS')


if __name__ == '__main__':
    main()
