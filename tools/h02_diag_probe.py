#!/usr/bin/env python3
"""Execute the H02D SRC formatters in its compiled OS image; no hardware."""
import argparse
import json
from pathlib import Path
import struct

import i03_dynamic_probe as dynamic


def run(candidate, output):
    from emu.session import Session
    from emu.harness import call
    from unicorn.m68k_const import UC_M68K_REG_PC
    candidate, meta, symbols, firmware = dynamic._fixture(candidate)
    assert meta['firmware_label'] == 'H02D'
    dynamic._configure(firmware)
    s = Session(str(candidate/'assigned.snap'), audio=True)
    def addr(name): return int(symbols[name], 0)
    def u32(a): return struct.unpack('>I', s.m.uc.mem_read(a, 4))[0]
    def put(a, n): s.m.uc.mem_write(a, struct.pack('>I', n))
    try:
        s.run_ms(100)
        dynamic._select_usb1(s, symbols)
        ui = u32(addr('usb_track_machine')+28)
        assert s.m.uc.mem_read(ui+616, 1)[0] == 2
        pages = u32(ui+620)
        assert bytes(s.m.uc.mem_read(pages+16+4, 8)) == bytes([8,9,10,11,12,13,14,0])
        s.m.ensure(0x30000000)
        def invoke(fn, args):
            context = s.m.uc.context_save()
            try:
                s.m._fast_stepper_obj.left = 100000
                call(s.m, fn, args, limit=100000)
                assert s.m.uc.reg_read(UC_M68K_REG_PC) == 0xdeadbee0
            finally:
                s.m.uc.context_restore(context)
        put(addr('usb_track_clock'), 12345)
        put(addr('usb_track_worker_ticks'), 5678)
        put(addr('usb_track_route'), 1)
        put(addr('usb_track_gate'), 0)
        s.m.uc.mem_write(addr('fw_render_machine'), bytes([121]))
        put(0x406104f4, 2); put(0x4199f134, 1); put(0x4025c294, 1)
        watched = [addr(n) for n in ('usb_track_clock', 'usb_track_worker_ticks',
                                     'usb_track_route', 'usb_track_gate')]
        before = [u32(a) for a in watched]
        values = {}
        for k, label in enumerate(('RCLK','TICK','STATE','SYNC','MIDI','TX','RX')):
            knob = ui+336+40*k
            assert bytes(s.m.uc.mem_read(u32(knob), len(label)+1)) == label.encode()+b'\0'
            s.m.uc.mem_write(0x30000000, bytes(32))
            invoke(u32(knob+32), [0x30000000, 0, 0, 121])
            text = bytes(s.m.uc.mem_read(0x30000000, 32)).split(b'\0')[0].decode()
            assert text.isdecimal() and 0 <= int(text) < 10000
            values[label] = int(text)
        assert [values[k] for k in ('RCLK','TICK','STATE','SYNC')] == [2345,5678,29,1]
        assert [u32(a) for a in watched] == before, 'diagnostics changed worker/render state'
        report = {'task':'H02','pass':True,'scope':'compiled emulator diagnostics only; seeded globals, no physical USB',
                  'candidate_sha256':meta['syx_sha256'],'label':meta['firmware_label'],
                  'diagnostic_page_labels':list(values),'formatter_values':values,
                  'read_only_observed_state':True}
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2)+'\n')
        print('H02D compiled diagnostic page: PASS')
    finally:
        s.close()


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    run(a.candidate, a.output)
