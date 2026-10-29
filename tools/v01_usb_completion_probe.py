#!/usr/bin/env python3
"""Native USB submit -> controller -> ISR209/134 -> TX callback regression.

Submission is isolated fixture setup; IRQs and callbacks execute natively in
Session. No active voice, ACK stream or hardware timing claim.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

import i03_dynamic_probe as dynamic
from v01_contention_probe import add_reschedule, add_usb_dispatch
from v01_usb_device_model import UsbDeviceModel, UsbStatusIrq, put_u32, u32


def case(candidate, endpoint, lengths=(60,), rx_payload_len=None):
    from emu.session import Session
    from emu.harness import call
    from unicorn.m68k_const import UC_M68K_REG_PC
    root, meta, _, fw = dynamic._fixture(candidate)
    dynamic._configure(fw)
    state = {'callback_entries': 0}

    def setup(s):
        add_reschedule(s)
        add_usb_dispatch(s)
        model = UsbDeviceModel(s.m, 250_000_000)
        model.configure_component()
        irq = UsbStatusIrq(model)
        s.sources += (model, irq)
        s.usb_model, s.usb_irq = model, irq
        if endpoint < 4:
            model.queue_rx(endpoint, bytes(lengths[0] if rx_payload_len is None else rx_payload_len), 0)
        s.m.ensure(0x30000000)
        desc = 0x30001000
        qh = 0x80000000 + (0x180 if endpoint == 3 else 0xc0)
        s.m.uc.mem_write(qh, bytes(64))
        for i, length in enumerate(lengths):
            dtd = desc + 64 * i
            s.m.uc.mem_write(dtd, bytes(64))
            for offset, value in [(0, 1), (4, (length << 16) | 128),
                                  (8, 0x30002000), (0x20, length),
                                  (0x24, 0x40004404)]:
                put_u32(s.m.uc, dtd + offset, value)
        put_u32(s.m.uc, 0x40610478, sum(lengths))
        put_u32(s.m.uc, 0x406104d8, 0)
        put_u32(s.m.uc, 0xfc0b01a4, 0)
        # Fixture-only call outside the run window. No RX/ACK callback is
        # injected; native IRQ134 must dispatch the TX callback in run_ms.
        ctx = s.m.uc.context_save()
        try:
            for i in range(len(lengths)):
                call(s.m, 0x40005cd4, [endpoint, desc + 64 * i], limit=100000)
                assert s.m.uc.reg_read(UC_M68K_REG_PC) == 0xdeadbee0
        finally:
            s.m.uc.context_restore(ctx)

        def callback(*_):
            state['callback_entries'] += 1

        s.at(0x40004404, callback)

    s = Session(str(root / 'assigned.snap'), hle=False,
                timing={'fsys': 250_000_000}, on_machine=setup)
    try:
        s.run_ms(5)
        state.update(endpoint=endpoint, lengths=list(lengths), queued_after=u32(s.m.uc, 0x40610478),
                     expected_queued_after=0 if rx_payload_len is None else lengths[0] - rx_payload_len,
                     halted=s.halted, model=s.usb_model.report(),
                     irq209_deliveries=s.usb_irq.deliveries,
                     irq134_runs=s.clock.report()['vectors'].get(134, {}).get('count', 0))
        return state
    finally:
        s.close()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    cases = ([case(args.candidate, ep) for ep in (3, 5)] +
             [case(args.candidate, 5, (60, 24)), case(args.candidate, 3, (64,), 4)])
    passed = all(c['queued_after'] == c['expected_queued_after'] and c['callback_entries'] == len(c['lengths']) and
                 c['irq134_runs'] >= 1 and not c['halted'] for c in cases)
    report = {'schema': 'v01-usb-completion/1', 'status': 'verified' if passed else 'failed',
              'hardware_test': False, 'cases': cases,
              'candidate_sha256': dynamic._fixture(args.candidate)[1]['syx_sha256'],
              'probe_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'model_sha256': hashlib.sha256((Path(__file__).parent/'v01_usb_device_model.py').read_bytes()).hexdigest()}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
