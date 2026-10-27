#!/usr/bin/env python3
"""Hash-pinned OS 1.54 USB-MIDI anchors; no hardware I/O or firmware output."""
import argparse
import json
from pathlib import Path
from r07_handoff_map import MAIN_SHA, load_main
from dt2.coldfire import disasm

def check_ui(candidate):
    import os
    import struct
    import r03_probe as r03
    from emu.bootstrap import FirmwarePaths
    from emu.session import Session
    meta=json.loads((candidate/'fixture.json').read_text())
    paths=FirmwarePaths(str((candidate/meta['firmware_root']).resolve()),
                        'I02.syx',str(r03.EMU/'devices'))
    os.environ.update(paths.env())
    symbols=json.loads((candidate/meta['map']).read_text())
    r03.ACTIVE_TRACK=0x4197C6B4
    s=Session(str(candidate/'assigned.snap'), audio=True)
    try:
        s.run_ms(100)
        r03.select_track(s,0)
        s.press(s.code('FUNC')); s.run_ms(80); r03.tap(s,20)
        s.release(s.code('FUNC'))
        for _ in range(4): r03.tap(s,15)
        r03.tap(s,12); r03.tap(s,12)
        def read(addr,n): return bytes(s.m.uc.mem_read(addr,n))
        def u32(addr): return struct.unpack('>I',read(addr,4))[0]
        kit=u32(int(symbols['fw_kit'],0)); sound=kit+0x20
        assert read(sound+126,1)==bytes([121]), 'USB1 selection failed'
        machine=int(symbols['usb_track_machine'],0)
        ui=u32(machine+28)
        names=['PTCH','DEC','RAMP','HOLD','TICK','NOIS','DIRT','DIST']
        defaults=[60,65,41,62,91,44,56,58]
        for k,name in enumerate(names):
            knob=ui+16+40*k
            assert read(u32(knob),len(name)+1)==name.encode()+b'\0'
            assert struct.unpack('>iii',read(knob+20,12))==(0,127<<8,defaults[k]<<8)
        r03.tap(s,20)
        r03.tap(s,24,after=250)
        params=int(symbols['fw_voice_params'],0)
        initial=list(struct.unpack('>8h',read(params,16)))
        assert initial==[d<<8 for d in defaults], ('defaults',initial)
        changed=[]
        values=[]
        for k in range(8):
            before=read(params+2*k,2)
            r03.rotate(s,k+1,16)
            r03.tap(s,24,after=250)
            after=read(params+2*k,2)
            assert before!=after, ('knob did not change',k,before,after)
            assert read(sound+126,1)==bytes([121])
            changed.append(names[k])
            values.append([int.from_bytes(before,"big",signed=True),
                           int.from_bytes(after,"big",signed=True)])
        return {'candidate_sha256': meta['syx_sha256'],
                'level':'compiled OS 1.54 emulator; no physical USB',
                'booted':True, 'labels_ranges_defaults':True,
                'panel_knobs_changed':changed, 'final_8_8_before_after':values,
                'native_defaults_applied':True,
                'transport_qualified':False}
    finally:
        s.close()


p = argparse.ArgumentParser(__doc__)
p.add_argument('--sections', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--candidate', type=Path)
a = p.parse_args()
b = load_main(a.sections)
# Each range begins at a separately identified function boundary.
rows = {}
for lo, hi in ((0x400045A6, 0x40004726), (0x40004760, 0x40004832),
               (0x40004990, 0x40004A1A)):
    rows.update({pc: (mn, op) for pc, _, mn, op in disasm(b, 0x40000400, lo, hi)})
checks = {
    'callback_registration': (0x400045AA, 'move.l', '$4061046c.l'),
    'endpoint_1_control': (0x400045CA, 'clr.l', '$fc0b01c4.l'),
    'receive_callback_load': (0x400047FA, 'movea.l', '$4061046c.l'),
    'receive_callback_call': (0x40004808, 'jsr', '(a0)'),
    'send_wrap_wait': (0x400049D8, 'bge.b', '$400049d0'),
    'send_capacity_wait': (0x40004A0C, 'bgt.b', '$400049fe'),
}
verified = {}
for name, (pc, mn, operand) in checks.items():
    actual = rows.get(pc)
    assert actual and actual[0] == mn and operand in actual[1], (name, actual)
    verified[name] = hex(pc)
a.output.parent.mkdir(parents=True, exist_ok=True)
report = {
    'main_sha256': MAIN_SHA, 'level': 'static instruction anchors only',
    'anchors': verified,
    'candidate_raw_midi_sender': '0x40004990',
    'candidate_receive_callback_slot': '0x4061046c',
    'transport_qualified': False,
    'qualification_scope': 'anchors/UI only; compiled exchange is evidence/i03/integrated-154.json',
    'missing': ['physical USB DMA/completion cadence, disconnect detection and hardware timing'],
}
if a.candidate:
    report['ui_emulator'] = check_ui(a.candidate)
a.output.write_text(json.dumps(report, indent=2)+'\n')
print('I03 anchors/UI: PASS (compiled transport exchange is a separate probe)')
