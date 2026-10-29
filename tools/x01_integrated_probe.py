#!/usr/bin/env python3
"""Booted X01E firmware/native serializer/RX callback + real dual-engine host.

USB descriptor submission/completion and already-received PCM are modelled.
No physical USB, Circle scheduling/timing or hardware flash.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

import i03_dynamic_probe as dynamic
import r03_probe as r03

ROOT = Path(__file__).resolve().parents[1]


def run(candidate, output, lfo_probe=False, locks_probe=False):
    from emu.session import Session
    from emu.harness import call
    from unicorn import UC_HOOK_CODE
    from unicorn.m68k_const import UC_M68K_REG_A7, UC_M68K_REG_PC, UC_M68K_REG_D0, UC_M68K_REG_SR
    candidate, meta, symbols, firmware = dynamic._fixture(candidate)
    assert meta.get('emulator_only') and meta['firmware_label'] == 'X01E'
    dynamic._configure(firmware)
    library = ROOT/'build/x01/emulator-peer.dylib'
    library.parent.mkdir(parents=True, exist_ok=True)
    sources = ['tests/x01_emulator_peer.c', 'control/link.c', 'control/track_frame.c',
               'control/track_session.c', 'control/track_host.c', 'audio/track_playout.c',
               'audio/trx_voice.c', 'audio/src_44148.c', 'deps/mdkit/src/trx/b2/mdkit_trx_b2.c']
    subprocess.run([os.environ.get('CC', 'cc'), '-dynamiclib', '-O2', '-std=c99',
                    '-Icontrol', '-Iaudio', '-Ideps/mdkit/include', '-Ideps/mdkit/src/trx/b2',
                    *sources, '-lm', '-o', str(library)], cwd=ROOT, check=True)
    peer = ctypes.CDLL(str(library))
    peer.peer_frame.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint32]
    peer.peer_reply.argtypes = [ctypes.c_void_p]
    peer.peer_render.argtypes = [ctypes.c_void_p]
    peer.peer_init()
    state = {'up': True, 'drop': False, 'stall': False, 'tx': 0, 'rx': 0,
             'silent': [0, 0], 'audible': [0, 0], 'halves': [0, 0],
             'other_track_checks': 0, 'sends_masked': True, 'worker_entries': 0}
    pending = bytearray()
    pcm = (ctypes.c_int32*64)()
    others = None
    native_lfo = [set(), set()]
    observe_lfo = False
    observe_locks = False
    native_starts = [[], []]
    cin = (0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1)

    def sym(name): return int(symbols[name], 0)
    def get(cpu, at): return struct.unpack('>I', cpu.mem_read(at, 4))[0]
    def put(cpu, at, value): cpu.mem_write(at, struct.pack('>I', value))

    def hook(cpu, address, size, data):
        nonlocal others
        if address == sym('usb_track_tick'):
            assert (cpu.reg_read(UC_M68K_REG_SR)>>8)&7 == 0
            state['worker_entries'] += 1
        elif address == 0x40004832:
            sp = cpu.reg_read(UC_M68K_REG_A7)
            ret, count, pointer, wait = struct.unpack('>4I', cpu.mem_read(sp, 16))
            assert wait == 0 and count%4 == 0 and count <= 132
            state['sends_masked'] &= cpu.reg_read(UC_M68K_REG_SR)&0x700 == 0x700
            raw = bytes(cpu.mem_read(pointer, count))
            for i in range(0, count, 4):
                assert raw[i]>>4 == 0
                for byte in raw[i+1:i+1+cin[raw[i]&15]]:
                    if byte == 0xf0: pending.clear()
                    pending.append(byte)
                    if byte == 0xf7:
                        wire = bytes(pending); pending.clear()
                        peer.peer_frame(wire, len(wire), get(cpu, sym('usb_track_clock')))
                        state['tx'] += 1
            put(cpu, 0x40610478, get(cpu, 0x40610478)+count)
            cpu.reg_write(UC_M68K_REG_D0, 0)
            cpu.reg_write(UC_M68K_REG_A7, sp+4)
            cpu.reg_write(UC_M68K_REG_PC, ret)
        elif address == 0x40077fba:
            if observe_locks:
                mask=get(cpu,sym('fw_voice_start'))
                for t in range(2):
                    if mask & (1<<t):
                        native_starts[t].append(struct.unpack('>h',cpu.mem_read(sym('fw_voice_params')+106*t,2))[0])
            if observe_lfo:
                for t in range(2):
                    native_lfo[t].add(struct.unpack('>h', cpu.mem_read(sym('fw_voice_params')+106*t,2))[0])
            put(cpu, 0x40610474, int(state['up']))
            put(cpu, 0xfc0b01c4, 0xc800c8 if state['up'] else 0)
            put(cpu, 0x406104f4, 6); put(cpu, 0x4025c294, int(state['up']))
            put(cpu, 0x40610478, 0xffb-1 if state['stall'] else 0)
            selector = get(cpu, 0x4199f134)
            assert selector < 2
            peer.peer_render(pcm)
            cpu.mem_write(0x80001f60+256*selector, struct.pack('>64i', *pcm))
            others = bytes(cpu.mem_read(0x80001a18+256, 768))
        elif address == 0x40077fc0:
            assert others == bytes(cpu.mem_read(0x80001a18+256, 768))
            state['other_track_checks'] += 1
            selector = get(cpu, 0x4199f134)
            state['halves'][selector] += 1
            for track in range(2):
                if cpu.mem_read(sym('fw_render_machine')+track, 1)[0] != 121: continue
                block = bytes(cpu.mem_read(0x80001a18+128*track, 128))
                if not get(cpu, sym('usb_track_gate')):
                    assert block == bytes(128), 'PCM escaped before negotiated activation'
                    state['silent'][track] += 1
                elif block != bytes(128): state['audible'][track] += 1

    def machine(session):
        from emu import intfrc
        sem = sym('worker_sem')
        assert sem in session.ev['unblock_skip'], 'worker wait must not be faked'
        irq = intfrc.ForcedInterrupts(session.m, sources=(13,), base=0xfc050000, first_vector=192)
        if get(session.m.uc, 0xfc050014)&(1<<13): irq.asserted.add(13)
        session.sources += (irq,)
        for address in (0x40004832, 0x40077fba, 0x40077fc0, sym('usb_track_tick')):
            session.m.uc.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)

    session = Session(str(candidate/'assigned.snap'), audio=True, on_machine=machine)
    reply = ctypes.create_string_buffer(64)

    def step(ms):
        for _ in range((ms+4)//5):
            session.run_ms(5)
            assert not session.halted, session.halted
            peer.peer_tick(get(session.m.uc, sym('usb_track_clock')))
            while (n := peer.peer_reply(reply)):
                if state['drop'] or not state['up']: continue
                cpu = session.m.uc; slot = get(cpu, 0x4061046c)
                assert 0x47bc0000 <= slot < 0x48000000
                context = cpu.context_save()
                try:
                    session.m.ensure(0x30000000); cpu.mem_write(0x30000000, reply.raw[:n])
                    session.m._fast_stepper_obj.left = 100000
                    call(session.m, slot, [0x30000000, n], limit=100000)
                    assert cpu.reg_read(UC_M68K_REG_PC) == 0xdeadbee0
                    state['rx'] += 1
                finally:
                    cpu.context_restore(context)

    try:
        step(100)
        dynamic._select_usb1(session, symbols, 0)
        dynamic._select_usb1(session, symbols, 1)
        if lfo_probe:
            # Match the known-good single-track fixture: seed before the first
            # voice start. Late seeding did not affect native render values.
            kit=get(session.m.uc,sym('fw_kit'))
            slots=dynamic._sound_slots(session.m.uc,kit,1)
            seeds={1:0x5000,4:17<<8,5:2<<8,6:0x4000,7:1<<8,8:0x4800,17:60<<8}
            for slot,value in seeds.items(): slots[slot]=value
            dynamic._write_sound_slots(session.m.uc,kit,1,slots)
            assert dynamic._sound_slots(session.m.uc,kit,1)==slots
            observe_lfo=True
            session.press(24); session.press(25); step(50)
            session.release(24); session.release(25)
            values=[set(),set()]
            for _ in range(600):
                step(5)
                if peer.peer_allowed():
                    for t in range(2): values[t].add(peer.peer_param(t,0))
            diagnostic={
                'scope':'booted OS1.54 native two-track LFO; modelled USB/host timing; no hardware',
                'pass':len(values[1])>=2 and values[0]=={60<<8} and len(native_lfo[1])>=3 and
                       values[1].issubset({(v//256)*256 for v in native_lfo[1]}) and
                       bool(peer.peer_allowed()) and peer.peer_fault()==0,
                'track0_values_8_8':sorted(values[0]),'track1_values_8_8':sorted(values[1]),
                'native_values_8_8':[sorted(v) for v in native_lfo],
                'seed_readback_matches':dynamic._sound_slots(session.m.uc,kit,1)==slots,
                'observation_ms':3000,'host_allowed':bool(peer.peer_allowed()),
                'host_fault':peer.peer_fault()}
            diagnostic['candidate']=dynamic._candidate_summary(meta)
            diagnostic['probe_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
            diagnostic['peer_source_sha256']={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in sources}
            (ROOT/'evidence/x01/lfo-fixture.json').write_text(json.dumps(diagnostic,indent=2)+'\n')
            assert diagnostic['pass'],diagnostic
            print(json.dumps(diagnostic,indent=2))
            return
        session.press(24); session.press(25); step(30)
        session.release(24); session.release(25)
        step(1200)
        assert get(session.m.uc, sym('usb_track_gate')) and peer.peer_allowed(), (state, peer.peer_reason(), peer.peer_fault())
        assert get(session.m.uc, sym('x01_route_lanes')) == 0x100
        generation = peer.peer_session()
        if locks_probe:
            uc=session.m.uc
            pattern=get(uc,dynamic.PATTERN_POINTER)
            assert 0x40000000<=pattern<0x48000000
            base=[dynamic._sound_slots(uc,get(uc,sym('fw_kit')),t) for t in range(2)]
            for t in range(2):
                uc.mem_write(pattern+dynamic.TRACK_STRIDE*t,
                             struct.pack('>64H',*([0x0881]*4+[0]*60)))
                locks=pattern+0x38f0+6965*t
                for index in range(4):
                    dynamic._write_i16s(uc,locks+108*index,[-1]*53+[0])
                dynamic._write_i16s(uc,locks+216+2*17,[(72+12*t)<<8])
            assert base==[dynamic._sound_slots(uc,get(uc,sym('fw_kit')),t) for t in range(2)]
            observe_locks=True
            seen=[[],[]]
            counts=[peer.peer_starts(t) for t in range(2)]
            session.press(session.code('PLAY')); step(40); session.release(session.code('PLAY'))
            for _ in range(600):
                step(5)
                for t in range(2):
                    count=peer.peer_starts(t)
                    if count!=counts[t]:
                        seen[t].append(peer.peer_param(t,0)); counts[t]=count
            session.press(session.code('STOP')); step(40); session.release(session.code('STOP')); step(100)
            expected=[[60<<8,(72+12*t)<<8,60<<8] for t in range(2)]
            diagnostic={'scope':'booted OS1.54 native two-track stored PTCH locks and real host engines; modelled USB; no hardware',
                'native_start_values_8_8':native_starts,'host_start_values_8_8':seen,
                'expected_consecutive_starts':expected,'same_session':peer.peer_session()==generation,
                'host_allowed':bool(peer.peer_allowed()),'host_fault':peer.peer_fault(),
                'base_sounds_preserved':base==[dynamic._sound_slots(uc,get(uc,sym('fw_kit')),t) for t in range(2)]}
            diagnostic['pass']=all(any(v[i:i+3]==expected[t] for i in range(len(v)-2)) for t in range(2) for v in (native_starts[t],seen[t])) and diagnostic['same_session'] and diagnostic['host_allowed'] and diagnostic['host_fault']==0 and diagnostic['base_sounds_preserved']
            diagnostic['candidate']=dynamic._candidate_summary(meta)
            diagnostic['probe_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
            diagnostic['peer_source_sha256']={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in sources}
            (ROOT/'evidence/x01/locks-154.json').write_text(json.dumps(diagnostic,indent=2)+'\n')
            assert diagnostic['pass'],diagnostic
            print(json.dumps(diagnostic,indent=2)); return
        before = [peer.peer_starts(t) for t in range(2)]
        for _ in range(3):
            session.press(24); session.press(25); step(30)
            session.release(24); session.release(25); step(100)
        assert all(peer.peer_starts(t) >= before[t]+3 for t in range(2))
        controls = [peer.peer_param(t, 0) for t in range(2)]
        kit=get(session.m.uc,sym('fw_kit'))
        slots_before=dynamic._sound_slots(session.m.uc,kit,1)
        r03.tap(session,20)  # Explicitly reopen SRC after both machine choosers.
        r03.rotate(session,1,32)
        session.press(25); step(30); session.release(25); step(300)
        changed = [peer.peer_param(t, 0) for t in range(2)]
        if changed[1] == controls[1]:
            from emu.panel import write_png
            if session.frames: write_png(session.frames[-1][1],str(candidate/'x01-src-check.png'))
        assert changed[1] != controls[1] and changed[0] == controls[0], {
            'before': controls, 'after': changed, 'active_track': get(session.m.uc, sym('fw_active_track')),
            'native_ptch': [struct.unpack('>h', session.m.uc.mem_read(sym('fw_voice_params')+106*t, 2))[0] for t in range(2)],
            'changed_sound_slots': [(i,a,b) for i,(a,b) in enumerate(zip(slots_before,dynamic._sound_slots(session.m.uc,kit,1))) if a!=b],
            'reason': peer.peer_reason(), 'fault': peer.peer_fault()}
        assert peer.peer_session() == generation
        assert all(state['audible']) and all(state['silent']) and all(state['halves'])
        state['drop'] = True; step(650)
        assert not get(session.m.uc, sym('usb_track_gate'))
        state['drop'] = False; step(1500)
        assert get(session.m.uc, sym('usb_track_gate')) and peer.peer_allowed()
        state['up'] = False; peer.peer_lost(); step(150)
        assert not get(session.m.uc, sym('usb_track_gate'))
        state['up'] = True; step(1500)
        assert get(session.m.uc, sym('usb_track_gate')) and peer.peer_allowed()
        state['stall'] = True; step(900)
        assert not get(session.m.uc, sym('usb_track_gate'))
        state['stall'] = False; step(1500)
        assert get(session.m.uc, sym('usb_track_gate')) and peer.peer_allowed()
        assert get(session.m.uc, sym('usb_track_worker_guard_faults')) == 0
        assert session.ev.get('satisfied_by_sem', {}).get(sym('worker_sem'), 0) == 0
        assert state['sends_masked'] and state['worker_entries'] > 0
        report = {'scope': 'booted X01E OS1.54, native RTOS worker/serializer/RX bridge and dual real-engine peer; USB DMA/PCM modelled',
                  'hardware_test': False, 'candidate': {**dynamic._candidate_summary(meta),
                      'emulator_only': True, 'manifest_sha256': meta['manifest_sha256']},
                  'peer_source_sha256': {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in sources},
                  'checks': ['two USB1 machines selected through native UI', 'matching v5/v4 two-lane session',
                             'native simultaneous manual triggers consumed on both engines',
                             'panel SRC edit changes only the selected track', 'both returned engine lanes reach their track buffers',
                             'other six source buffers unchanged', 'ACK loss/recovery', 'disconnect/reconnect',
                             'stalled transmit queue/recovery', 'native worker at task IPL0, real semaphore wait, intact guard counters'],
                  'counters': state, 'pass': True}
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2)+'\n')
        print('X01 full compiled-device/native-serializer/dual-engine exchange: PASS', state, flush=True)
    finally:
        session.close()


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--output', type=Path, default=ROOT/'evidence/x01/integrated-154.json')
    p.add_argument('--lfo-probe',action='store_true',help='separate native first-trigger LFO scenario; writes lfo-fixture.json')
    p.add_argument('--locks-probe',action='store_true',help='separate two-track native lock/restore scenario')
    a = p.parse_args()
    if a.lfo_probe and a.locks_probe: p.error('select one scenario')
    run(a.candidate,a.output,a.lfo_probe,a.locks_probe)
