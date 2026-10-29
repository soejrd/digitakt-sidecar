#!/usr/bin/env python3
"""Private OS 1.54 fixture and full-render routing probe. No hardware I/O."""
import argparse
import json
import math
import os
from pathlib import Path
import shutil
import struct
import wave
import r03_probe as r03
from r07_handoff_map import MAIN_SHA, SYX_SHA, load_main

ROOT = Path(__file__).resolve().parents[1]
PRE = 0x40077FBA
SELECTOR = 0x4199F134
USB = 0x80001F60
TRACK = 0x80001A18


def configure(root):
    from emu.bootstrap import FirmwarePaths
    p = FirmwarePaths(str(root.resolve()), 'Digitakt_OS1.54.syx', str(r03.EMU/'devices'))
    os.environ.update(p.env())
    return p


def prepare(args):
    from emu.bootstrap import first_run
    from emu.release import identify_release, write_device_overlay
    from emu import ekfsformat as ek
    from emu.session import Session
    root = args.root.resolve()
    root.mkdir(exist_ok=False)
    assert r03.sha(args.syx) == SYX_SHA, 'wrong stock container'
    shutil.copyfile(args.syx, root/'Digitakt_OS1.54.syx')
    p = configure(root)
    release = identify_release(p.syx, p.devices_dir)
    write_device_overlay(release, p.overlay, p.devices_dir)
    os.environ.update(p.env())
    progress = lambda e: print(e.step, e.kind, flush=True) if e.kind in ('start', 'done') else None
    first_run(p, progress=progress)
    load_main(p.sections)
    tone = root/'fixture.wav'
    with wave.open(str(tone), 'wb') as out:
        out.setparams((1, 2, 48000, 0, 'NONE', 'not compressed'))
        out.writeframes(b''.join(struct.pack('<h', round(6000*math.sin(2*math.pi*440*i/48000)))
                                  for i in range(48000)))
    fs = ek.Ekfs(p.card, write=True)
    try:
        fs.add_sample(ek.ROOT_INODE, tone.name, tone.read_bytes())
    finally:
        fs.close()
    first_run(p, progress=progress)
    assign_project(p, root)
    (root/'fixture.json').write_text(json.dumps({'main_sha256': MAIN_SHA,
        'syx_sha256': SYX_SHA, 'acceptance': 'upstream untested-release overlay; card checked'}, indent=2)+'\n')
    print('PRIVATE 1.54 FIXTURE READY', flush=True)


def assign_project(p, root):
    from emu.session import Session
    r03.ACTIVE_TRACK = 0x4197C6B4  # pinned core/mod.json 1.54 export
    s = Session(p.gui, audio=False)
    try:
        s.run_ms(700)
        for code in (6, 15, 12, 15, 15, 17, 12, 15, 15, 12, 12):
            r03.tap(s, code, after=400)
        for _ in range(4):
            r03.tap(s, 13)
        for track in (0, 1):
            r03.select_track(s, track)
            r03.tap(s, 20)
            s.turn(4, 1, after_ms=150)
            for _ in range(130):
                r03.tap(s, 14, after=70)
            r03.tap(s, 15)
            r03.tap(s, 12)
            for _ in range(3):
                r03.tap(s, 13)  # close sample confirmation/browser before changing tracks
        r03.save_session(s, root/'assigned.snap')
    finally:
        s.close()


def candidate_prepare(args):
    import subprocess
    from emu.fwcheck import prepare as firmware_prepare
    from emu.bootstrap import first_run
    root=args.root.resolve()
    root.mkdir(exist_ok=False)
    assert r03.sha(args.stock)==SYX_SHA,'wrong stock container'
    load_main(args.fixture/'sections')
    artifacts=root/'artifacts'
    artifacts.mkdir()
    env=dict(os.environ,PYTHONPATH=str(ROOT/'deps/elekloader'),
             ELEKLOADER_CROSS=os.environ.get('ELEKLOADER_CROSS','m68k-elf-'))
    with (root/'build.log').open('w') as log:
        def run(*cmd):
            subprocess.run([__import__('sys').executable,*cmd],env=env,cwd=ROOT,
                           stdout=log,stderr=subprocess.STDOUT,check=True)
        track_mod = 'mods/usb-track'
        midi_mod = 'mods/usb-midi'
        if args.diagnostic:
            overlay = root/'source'
            (overlay/'mods/usb-track').mkdir(parents=True)
            (overlay/'control').symlink_to(ROOT/'control', target_is_directory=True)
            if getattr(args,'remote_diagnostic',False):
                midi_path=overlay/'mods/usb-midi'
                midi_path.mkdir()
                for source in (ROOT/'mods/usb-midi').iterdir():
                    if source.suffix in ('.c','.h','.s'):
                        shutil.copyfile(source,midi_path/source.name)
                midi_manifest=json.loads((ROOT/'mods/usb-midi/mod.json').read_text())
                midi_manifest['cflags']=midi_manifest.get('cflags',[])+['-DV01_REMOTE_DIAGNOSTIC']
                (midi_path/'mod.json').write_text(json.dumps(midi_manifest,indent=2)+'\n')
                midi_mod=str(midi_path)
            else:
                (overlay/'mods/usb-midi').symlink_to(ROOT/'mods/usb-midi',target_is_directory=True)
            for source in (ROOT/'mods/usb-track').iterdir():
                if source.suffix in ('.c', '.h', '.s'):
                    shutil.copyfile(source, overlay/'mods/usb-track'/source.name)
            manifest = json.loads((ROOT/'mods/usb-track/mod.json').read_text())
            cflags = ['-DUSB_TRACK_DIAGNOSTIC']
            if getattr(args, 'remote_diagnostic', False):
                cflags.append('-DV01_REMOTE_DIAGNOSTIC')
                manifest['subscribe']=manifest.get('subscribe',[])+[
                    {'event':'ev_render_in','fn':'usb_track_diag_render_in','order':10},
                    {'event':'ev_render_out','fn':'usb_track_diag_render_out','order':90}]
            if getattr(args, 'compact_snapshots', False):
                cflags.append('-DV01_COMPACT_SNAPSHOTS')
            if getattr(args, 'no_self_wake', False):
                cflags.append('-DV01_NO_SELF_WAKE')
            if getattr(args, 'worker_priority', None) is not None:
                cflags.append(f'-DV01_WORKER_PRIORITY={args.worker_priority}')
            manifest['cflags'] = manifest.get('cflags', []) + cflags
            (overlay/'mods/usb-track/mod.json').write_text(json.dumps(manifest, indent=2)+'\n')
            track_mod = str(overlay/'mods/usb-track')
        for mod in ('deps/elekloader/mods/core','deps/elekloader/mods/machine-pages',midi_mod,track_mod):
            run('-m','elekloader.sdk.build',mod,'--stock',str(args.stock.resolve()),'--out',str(artifacts))
        version=json.loads((ROOT/'mods/usb-track/mod.json').read_text())['version']
        mods=[artifacts/name for name in ('core-4.1-os1.54.elemod',
            'machine-pages-1.5-os1.54.elemod','usb-midi-0.1.elemod',f'usb-track-{version}.elemod')]
        run('-m','elekloader.lint',str(mods[-1]),'--stock',str(args.stock.resolve()),
            '--with',str(mods[0]),'--with',str(mods[1]),'--with',str(mods[2]),'--json')
        run('-m','elekloader.patch','--stock',str(args.stock.resolve()),
            *[v for m in mods for v in ('--mod',str(m))],
            '--out',str(artifacts/'I02.syx'),'--version',
            getattr(args,'label',None) or ('H02F' if args.diagnostic else ('I02A' if version=='0.1' else 'I03P')))
    p,_=firmware_prepare(str(artifacts/'I02.syx'),str(root/'emulator'))
    shutil.copyfile(args.fixture/'plusdrive.img',p.card)
    os.environ.update(p.env())
    first_run(p,progress=lambda e:print(e.step,e.kind,flush=True) if e.kind in ('start','done') else None)
    assign_project(p,root)
    write_candidate_metadata(root,p,version,args.diagnostic,getattr(args,'label',None),getattr(args,'no_self_wake',False),getattr(args,'worker_priority',None),getattr(args,'remote_diagnostic',False),getattr(args,'compact_snapshots',False))
    print('CANDIDATE BOOTED AND ASSIGNED',flush=True)


def write_candidate_metadata(root,p,version,diagnostic=False,label=None,no_self_wake=False,worker_priority=None,remote_diagnostic=False,compact_snapshots=False):
    artifacts=root/'artifacts'
    (root/'fixture.json').write_text(json.dumps({
        'firmware_root':str(Path(p.root).relative_to(root)),
        'map':'artifacts/I02.syx.map.json',
        'syx_sha256':r03.sha(artifacts/'I02.syx'),
        'stock_main_sha256':MAIN_SHA,
        'usb_track_version':version,
        'firmware_label':label or ('H02F' if diagnostic else ('I02A' if version=='0.1' else 'I03P')),
        'compiler_defines':(['USB_TRACK_DIAGNOSTIC'] if diagnostic else []) +
            (['V01_NO_SELF_WAKE'] if no_self_wake else []) +
            (['V01_REMOTE_DIAGNOSTIC'] if remote_diagnostic else []) +
            (['V01_COMPACT_SNAPSHOTS'] if compact_snapshots else []) +
            ([f'V01_WORKER_PRIORITY={worker_priority}'] if worker_priority is not None else []),
        'source_hashes':{str(f.relative_to(ROOT)):r03.sha(f) for f in
            sorted(set([p for folder in ('mods/usb-track','mods/usb-midi')
                        for p in (ROOT/folder).glob('*')
                        if p.suffix in ('.c','.h','.s','.json')] +
                       [ROOT/'control'/name for name in
                        ('link.c','link.h','device_link.c','device_link.h',
                         'sc_memory.h','v01_diag.h','dt1_transport.c','dt1_transport.h',
                         'usbmidi.c','usbmidi.h','sysex_split.c','sysex_split.h')]))}},indent=2)+'\n')


def capture(root, name, *, route=True, signal=True, cutoff=None, amp_zero=False, track=0, candidate=None, send=False, bypass=False):
    from emu.session import Session
    from unicorn import UC_HOOK_CODE
    if candidate:
        from emu.bootstrap import FirmwarePaths
        meta=json.loads((candidate/'fixture.json').read_text())
        folder=candidate/meta['firmware_root']
        p = FirmwarePaths(str(folder), 'I02.syx', str(r03.EMU/'devices'))
        os.environ.update(p.env())
        symbols = json.loads((candidate/meta['map']).read_text())
        fn = int(symbols['usb_track_render'],0)
        fn_end = int(symbols['usb_track_machine'],0)
    else:
        p = configure(root)
        load_main(p.sections)
        fn = fn_end = 0
    r03.ACTIVE_TRACK = 0x4197C6B4
    counts = {'blocks': 0, 'halves': [0, 0], 'main': 0, 'toggle': 0}
    phase = 0
    stages = []
    first_order = None
    instruction_counts = []
    current_instructions = 0
    started = False
    playing = False
    other_tracks = None
    other_track_checks = 0
    active_blocks = 0
    def on_machine(s):
        if bypass:
            s.m.uc.mem_write(fn,bytes.fromhex('4e75'))  # diagnostic: return before routing
        def hook(u, address, size, data):
            nonlocal phase, first_order, stages, current_instructions, started, other_tracks, other_track_checks, active_blocks
            if candidate and fn <= address < fn_end:
                if address == fn:
                    current_instructions = 0
                    started = True
                if started:
                    current_instructions += 1
                return
            if address == PRE+6:
                if other_tracks is not None:
                    assert bytes(u.mem_read(TRACK+128,896))==other_tracks, 'other track source changed'
                    other_track_checks += 1
                if started:
                    instruction_counts.append(current_instructions)
                    started = False
                return
            selector = struct.unpack('>I', u.mem_read(SELECTOR, 4))[0]
            if address == 0x40077420:
                if stages and first_order is None:
                    first_order = stages[:]
                stages = ['render']
                if candidate:
                    u.mem_write(0x406104F4, struct.pack('>I',6))
                    u.mem_write(0x4025C294, struct.pack('>I',1))
                samples = [round(math.sin(2*math.pi*3000*(phase+i)/48000)*0.1*(2**31-1))
                           if signal and (not candidate or not route or u.mem_read(int(symbols['fw_render_machine'],0),1)[0] == 121) else 0 for i in range(32)]
                phase += 32
                # Synthetic already-received current half, not USB DMA emulation.
                u.mem_write(USB+256*selector, b''.join(struct.pack('>ii',v,-v//2) for v in samples))
            elif address == PRE:
                stages.append('source')
                other_tracks=bytes(u.mem_read(TRACK+128,896))
                if candidate and u.mem_read(int(symbols['fw_render_machine'],0),1)[0]==121:
                    active_blocks += 1
                counts['blocks'] += 1
                counts['halves'][selector] += 1
                if route and not candidate:
                    pair = bytes(u.mem_read(USB+256*selector,256))
                    u.mem_write(TRACK,b''.join(pair[i:i+4] for i in range(0,256,8)))
                    u.mem_write(USB+256*selector,bytes(256))
            elif address == 0x40072194:
                stages.append('main')
                counts['main'] += 1
                if route and not bypass and (not candidate or (playing and u.mem_read(int(symbols['fw_render_machine'],0),1)[0] == 121)):
                    assert bytes(u.mem_read(USB+256*selector,256)) == bytes(256)
            elif address == 0x40078352:
                stages.append('toggle')
                counts['toggle'] += 1
        if candidate:
            s.m.uc.hook_add(UC_HOOK_CODE, hook, begin=fn, end=fn_end-1)
        for address in (0x40077420, PRE, PRE+6, 0x40072194, 0x40078352):
            s.m.uc.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
    s = Session(str((candidate or root)/'assigned.snap'),audio=True,on_machine=on_machine)
    try:
        s.run_ms(100)
        r03.select_track(s,track)
        if candidate and route:
            r03.select_track(s,0)
            s.press(s.code('FUNC'))
            s.run_ms(80)
            r03.tap(s,20)  # SRC machine chooser
            s.release(s.code('FUNC'))
            for _ in range(4):
                r03.tap(s,15)
            r03.tap(s,12)
            r03.tap(s,12)  # confirm changing this sound's machine
            kit=struct.unpack('>I',s.m.uc.mem_read(int(symbols['fw_kit'],0),4))[0]
            assert s.m.uc.mem_read(kit+0x20+126,1)[0] == 121, 'USB1 not selected'
            playing = True
            if track != 0:
                s.tap(24,hold_ms=50,after_ms=100)  # activate USB1 before the other-track regression
            r03.select_track(s,track)
        if cutoff:
            r03.tap(s,21)
            r03.rotate(s,5,127 if cutoff=='open' else -127)
        if amp_zero:
            r03.tap(s,22)
            r03.rotate(s,8,-127)
        if send:
            r03.tap(s,22)
            r03.rotate(s,6,127)  # AMP reverb send
        s.pcm.clear()
        s.tap(24+track,hold_ms=50,after_ms=900)
        raw=bytes(s.pcm)
        result=r03.metrics(raw)
        if name in ('open','send'):
            signal=False
            s.run_ms(100)
            s.pcm.clear()
            s.run_ms(300)
            result['tail']=r03.metrics(s.pcm)
        result.update(name=name,counts=counts,order=first_order,halted=s.halted,
                      compiled_instructions_max=max(instruction_counts,default=0),
                      other_track_checks=other_track_checks,active_blocks=active_blocks)
        assert not s.halted,s.halted
        print(json.dumps(result),flush=True)
        return result
    finally:
        s.close()


def lifecycle(candidate, symbols):
    from emu.bootstrap import FirmwarePaths
    from emu.session import Session
    from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
    from unicorn.m68k_const import UC_M68K_REG_A7, UC_M68K_REG_PC, UC_M68K_REG_SR
    meta=json.loads((candidate/'fixture.json').read_text())
    folder=candidate/meta['firmware_root']
    p=FirmwarePaths(str(folder.resolve()),'I02.syx',str(r03.EMU/'devices'))
    os.environ.update(p.env())
    fn=int(symbols['usb_track_render'],0)
    end=int(symbols['usb_track_machine'],0)
    armed=int(symbols['usb-track:armed'],0)
    machine=int(symbols['fw_render_machine'],0)
    s=Session(str(candidate/'assigned.snap'),audio=False)
    try:
        code=bytes(s.m.uc.mem_read(fn,end-fn))
    finally:
        s.close()
    u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN)
    for address,size in [(0x47BC0000,0x10000),(0x4C000000,0x10000),
        (0x40000000,0x1000),(0x4025C000,0x1000),(0x40610000,0x1000),
        (0x4199F000,0x1000),(0x80001000,0x2000)]:
        u.mem_map(address,size)
    u.mem_write(fn,code)
    def put(address,value):u.mem_write(address,struct.pack('>I',value))
    stop=0x40000300
    count=0
    def hook(cpu,address,size,data):
        nonlocal count
        if address==stop:
            cpu.emu_stop()
        else:
            assert fn <= address < end,'unexpected call outside bounded candidate'
            count+=1
    u.hook_add(UC_HOOK_CODE,hook)
    results=[]
    for name,selector,mode,sync,was_armed,enabled,kind in [
        ('half_0_extrema',0,6,1,1,True,'copy'),
        ('half_1_extrema',1,6,1,1,True,'copy'),
        ('startup_flush',0,6,1,0,True,'flush'),
        ('disconnect_flush',1,6,0,1,True,'flush'),
        ('audio_mode_flush',0,5,1,1,True,'flush'),
        ('midi_only_mode_flush',0,2,1,1,True,'flush'),
        ('board_mode_flush',0,4,1,1,True,'flush'),
        ('invalid_selector',2,6,1,1,True,'flush'),
        ('disabled_passthrough',0,6,1,1,False,'disabled'),
        ('reenter_flush',1,6,1,0,True,'flush')]:
        initial_tracks=b'\x55'*1024
        u.mem_write(TRACK-4,b'\x5a'*4+initial_tracks+b'\x5a'*4)
        samples=[0x7fffffff,-0x80000000,0,-1]*8
        pair=b''.join(struct.pack('>ii',v,0x12345678) for v in samples)
        initial_usb=pair+bytes([0x33])*256 if selector==0 else bytes([0x33])*256+pair
        u.mem_write(USB-4,b'\x5a'*4+initial_usb+b'\x5a'*4)
        put(SELECTOR,selector);put(0x406104F4,mode);put(0x4025C294,sync);put(armed,was_armed)
        u.mem_write(machine,bytes([121 if enabled else 0]))
        sp=0x4C008000;put(sp,stop);put(sp+4,TRACK)
        u.reg_write(UC_M68K_REG_SR,0x2700);u.reg_write(UC_M68K_REG_A7,sp)
        count=0
        u.emu_start(fn,stop,count=2000)
        assert u.reg_read(UC_M68K_REG_PC)==stop,'candidate did not return'
        assert bytes(u.mem_read(TRACK+128,896))==initial_tracks[128:]
        for guard in (TRACK-4,TRACK+1024,USB-4,USB+512):
            assert bytes(u.mem_read(guard,4))==b'\x5a'*4,'guard changed'
        if kind=='copy':
            assert bytes(u.mem_read(TRACK,128))==struct.pack('>32i',*samples)
            assert bytes(u.mem_read(USB+selector*256,256))==bytes(256)
            other=(selector^1)*256
            assert bytes(u.mem_read(USB+other,256))==initial_usb[other:other+256]
        elif kind=='flush':
            assert bytes(u.mem_read(TRACK,128))==bytes(128)
            assert bytes(u.mem_read(USB,512))==bytes(512)
        else:
            assert bytes(u.mem_read(TRACK,1024))==initial_tracks
            assert bytes(u.mem_read(USB,512))==initial_usb
        expected_armed=int(enabled and selector<=1 and mode==6 and sync==1)
        assert struct.unpack('>I',u.mem_read(armed,4))[0]==expected_armed
        results.append({'case':name,'pass':True,'instructions':count})
    return {'cases':results,'max_instructions':max(x['instructions'] for x in results),
            'scope':'compiled function, synthetic state; no USB DMA or timing claim'}

def check(args):
    candidate=args.candidate.resolve()
    meta=json.loads((candidate/'fixture.json').read_text())
    for name,want in meta['source_hashes'].items():
        assert r03.sha(ROOT/name)==want,'source changed since candidate build: '+name
    symbols=json.loads((candidate/meta['map']).read_text())
    firmware=candidate/meta['firmware_root']/'I02.syx'
    assert r03.sha(firmware)==meta['syx_sha256'],'candidate changed since boot'
    results={}
    for name,kwargs in [
        ('stock',{'route':False,'signal':False}),
        ('open',{'cutoff':'open'}),
        ('closed',{'cutoff':'closed'}),
        ('amp_zero',{'cutoff':'open','amp_zero':True}),
        ('zero',{'signal':False}),
        ('send',{'cutoff':'open','send':True}),
        ('second_bypass',{'signal':False,'track':1,'bypass':True}),
        ('second_routed',{'signal':False,'track':1}),
    ]:
        results[name]=capture(args.root.resolve(),name,candidate=candidate,**kwargs)
    life=lifecycle(candidate,symbols)
    r=results
    assertions={
        'stock_sample_audible':r['stock']['peak']>100,
        'compiled_machine_active':r['open']['active_blocks']>500,
        'both_halves_exercised':min(r['open']['counts']['halves'])>500,
        'full_cycle_order':all(x['order']==['render','source','main','toggle'] for x in r.values()),
        'tone_reaches_track':r['open']['amplitude_3000_hz']>1000,
        'filter_attenuation_over_70_db':r['closed']['rms']<r['open']['rms']/3162,
        'amp_and_dry_return_silent':r['amp_zero']['peak']==0,
        'zero_replaces_stock_source':r['zero']['peak']==0,
        'reverb_send_tail':r['send']['tail']['rms']>100*r['open']['tail']['rms'],
        'other_source_blocks_unchanged':all(x['other_track_checks']>500 for x in r.values()),
        'other_track_audible_during_routing':r['second_routed']['peak']>100 and r['second_routed']['active_blocks']>500,
        'other_track_level_within_3_percent':abs(r['second_routed']['rms']/r['second_bypass']['rms']-1)<0.03,
        'bounded_lifecycle':all(x['pass'] for x in life['cases']) and life['max_instructions']<=515,
    }
    report={
        'firmware':'Digitakt mk1 OS 1.54 + core 4.1 + machine-pages 1.5 + usb-track '+meta.get('usb_track_version','0.1'),
        'stock_main_sha256':MAIN_SHA,'candidate_sha256':meta['syx_sha256'],
        'source_sha256':r03.sha(ROOT/'mods/usb-track/usb_track.c'),
        'hardware_test':False,'candidate_booted':True,'results':results,
        'lifecycle':life,'assertions':assertions,'pass':all(assertions.values()),
        'limits':[
            'Synthetic consumed USB PCM, mode and sync state; no USB DMA or physical stream.',
            'USB is silent until the render machine activates; control handshake remains I03.',
            'Lifecycle flags are synthetic; physical disconnect detection/reconnect freshness unqualified.',
            'Instruction counts bound this function, not the complete render hardware timing budget.',
            'Other source buffers are exact before/after; full-output regression uses a 3% level bound, not byte identity.',
            'Emulator stock-release overlay checks the card; OS 1.54 first-boot RAM acceptance is not mapped.'
        ]}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('I02 PASS' if report['pass'] else 'I02 FAIL',assertions,flush=True)
    if not report['pass']:
        raise SystemExit(1)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest='command', required=True)
    p = sub.add_parser('prepare')
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--syx', type=Path, required=True)
    p = sub.add_parser('check')
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--candidate', type=Path)
    p = sub.add_parser('candidate')
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--stock', type=Path, required=True)
    p.add_argument('--fixture', type=Path, required=True)
    p.add_argument('--diagnostic', action='store_true', help='H02F: corrected native mode with SRC diagnostics')
    p.add_argument('--remote-diagnostic', action='store_true', help='V01 raw-timer and one-Hz remote telemetry (requires --diagnostic)')
    p.add_argument('--compact-snapshots', action='store_true', help='V01 exact integer SRC full snapshots (paired Pi required)')
    p.add_argument('--no-self-wake', action='store_true', help='V01 diagnostic: disable worker self-reschedule')
    p.add_argument('--worker-priority', type=int, help='V01 diagnostic: override USB worker priority')
    p.add_argument('--label', help='explicit private candidate four-character firmware label')
    args = ap.parse_args()
    if getattr(args,'compact_snapshots',False) and not getattr(args,'diagnostic',False):
        ap.error('--compact-snapshots requires --diagnostic')
    if getattr(args,'remote_diagnostic',False) and not getattr(args,'diagnostic',False):
        ap.error('--remote-diagnostic requires --diagnostic')
    {'prepare':prepare,'candidate':candidate_prepare,'check':check}[args.command](args)


if __name__ == '__main__':
    main()
