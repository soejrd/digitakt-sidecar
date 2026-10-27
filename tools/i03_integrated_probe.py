#!/usr/bin/env python3
"""Compiled I03 device + native MIDI serializer + portable Circle peer.
Only USB descriptor submission/completion and received PCM are modelled.
No physical USB, audio output or firmware flashing.
"""
import argparse
import ctypes
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import i03_dynamic_probe as dynamic
import r03_probe as r03

ROOT=Path(__file__).resolve().parents[1]

def run(candidate, output):
    from emu.session import Session
    from emu.harness import call
    from unicorn import UC_HOOK_CODE
    from unicorn.m68k_const import UC_M68K_REG_A7,UC_M68K_REG_PC,UC_M68K_REG_D0,UC_M68K_REG_SR
    candidate,meta,symbols,firmware=dynamic._fixture(candidate)
    dynamic._configure(firmware)
    dylib=ROOT/'build/i03/emulator_peer.dylib'
    dylib.parent.mkdir(exist_ok=True,parents=True)
    subprocess.run([os.environ.get('CC','cc'),'-dynamiclib','-O2','-std=c99','-Icontrol',
                    'tests/i03_emulator_peer.c','control/link.c','control/host_link.c',
                    'control/circle_link_adapter.c','-o',str(dylib)],cwd=ROOT,check=True)
    peer=ctypes.CDLL(str(dylib))
    peer.peer_frame.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint32]
    peer.peer_reply.argtypes=[ctypes.c_void_p]
    peer.peer_init()
    state={'up':True,'drop':False,'tx':0,'rx':0,'silent':0,'audible':0,'sends_masked':True}
    pending=bytearray()
    cin=(0,0,2,3,3,1,2,3,3,3,3,3,2,2,3,1)
    def sym(n): return int(symbols[n],0)
    def u32(u,a): return struct.unpack('>I',u.mem_read(a,4))[0]
    def put(u,a,n): u.mem_write(a,struct.pack('>I',n))
    def hook(u,addr,size,data):
        if addr==0x40004832: # boundary of real native serializer / USB DMA
            sp=u.reg_read(UC_M68K_REG_A7)
            ret,n,p,wait=struct.unpack('>4I',u.mem_read(sp,16))
            assert wait==0 and n%4==0 and n<=132
            state['sends_masked'] &= u.reg_read(UC_M68K_REG_SR)&0x700==0x700
            raw=bytes(u.mem_read(p,n))
            for i in range(0,n,4):
                assert raw[i]>>4==0
                for b in raw[i+1:i+1+cin[raw[i]&15]]:
                    if b==0xf0: pending.clear()
                    pending.append(b)
                    if b==0xf7:
                        f=bytes(pending); pending.clear()
                        peer.peer_frame(f,len(f),u32(u,sym('usb_track_clock')))
                        state['tx']+=1
            put(u,0x40610478,u32(u,0x40610478)+n)
            u.reg_write(UC_M68K_REG_D0,0) # actual native API has no return contract
            u.reg_write(UC_M68K_REG_A7,sp+4); u.reg_write(UC_M68K_REG_PC,ret)
        elif addr==0x40077fba:
            put(u,0x40610474,int(state['up']))
            put(u,0xfc0b01c4,0xc800c8 if state['up'] else 0)
            put(u,0x406104f4,2); put(u,0x4025c294,int(state['up']))
            put(u,0x40610478,0) # synthetic USB completion between render blocks
            s=u32(u,0x4199f134)
            assert s<2
            u.mem_write(0x80001f60+256*s,struct.pack('>ii',123456,-123456)*32)
        elif addr==0x40077fc0 and u.mem_read(sym('fw_render_machine'),1)[0]==121:
            pcm=bytes(u.mem_read(0x80001a18,128))
            if not u32(u,sym('usb_track_gate')):
                assert pcm==bytes(128),'PCM escaped before activation'
                state['silent']+=1
            elif pcm!=bytes(128): state['audible']+=1
    def machine(s):
        for a in (0x40004832,0x40077fba,0x40077fc0):
            s.m.uc.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
    s=Session(str(candidate/'assigned.snap'),audio=True,on_machine=machine)
    reply=ctypes.create_string_buffer(64)
    def step(ms):
        for _ in range((ms+4)//5):
            s.run_ms(5)
            assert not s.halted,s.halted
            while (n:=peer.peer_reply(reply)):
                if state['drop'] or not state['up']: continue
                u=s.m.uc; slot=u32(u,0x4061046c)
                assert 0x47bc0000<=slot<0x48000000,'native RX bridge not installed'
                ctx=u.context_save()
                try:
                    s.m.ensure(0x30000000); u.mem_write(0x30000000,reply.raw[:n])
                    call(s.m,slot,[0x30000000,n],limit=100000)
                    state['rx']+=1
                finally: u.context_restore(ctx)
    try:
        step(100)
        dynamic._select_usb1(s,symbols)
        s.tap(24,hold_ms=30,after_ms=50)
        step(1000)
        assert u32(s.m.uc,sym('usb_track_gate')) and peer.peer_allowed(),state
        assert state['audible']>0 and state['silent']>0,state
        before=peer.peer_param(0)
        r03.rotate(s,1,16); step(500)
        assert peer.peer_param(0)!=before,'panel SRC did not reach host'
        expected=struct.unpack('>h',s.m.uc.mem_read(sym('fw_voice_params'),2))[0]
        assert peer.peer_param(0)==expected
        starts=peer.peer_starts()
        for _ in range(3): s.tap(24,hold_ms=30,after_ms=30); step(100)
        step(500); assert peer.peer_starts()>=starts+3
        state['drop']=True; step(450)
        assert not u32(s.m.uc,sym('usb_track_gate')),'lost ACKs left PCM active'
        state['drop']=False; step(1500)
        assert u32(s.m.uc,sym('usb_track_gate')),'ACK recovery failed'
        state['up']=False; peer.peer_lost(); step(150)
        assert not u32(s.m.uc,sym('usb_track_gate'))
        state['up']=True; step(1500)
        assert u32(s.m.uc,sym('usb_track_gate')) and peer.peer_allowed()
        assert state['sends_masked']
        report={'status':'verified','level':'compiled OS 1.54 + native serializer; USB DMA/PCM modelled',
                'hardware_test':False,'candidate':dynamic._candidate_summary(meta),
                'checks':['native serializer and two-argument compiled RX bridge',
                          'silence before ACTIVATE; fresh PCM after stale-half flush',
                          'panel SRC reaches host final 8.8 value','repeated native starts reach host',
                          'ACK loss closes gate; fresh session recovers','link loss/reconnect recovers',
                          'preflight and sender run in one native critical section'],
                'counters':state,
                'limits':['No physical USB/DMA, Circle scheduling or hardware timing measured.',
                          'USB descriptor submission/completion and already-decoded PCM are modelled.']}
        output.parent.mkdir(exist_ok=True,parents=True)
        output.write_text(json.dumps(report,indent=2)+'\n')
        print('I03 compiled-device/portable-Circle exchange: PASS',state,flush=True)
    finally: s.close()

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.candidate,a.output)
