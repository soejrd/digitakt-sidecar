#!/usr/bin/env python3
"""Validate native RTOS task sleep, storage/stack and signal wake in emulator.
Adds the missing INTC2 reschedule source through the existing generic model.
No hardware or calibrated CPU/USB timing claim.
"""
import argparse,json,struct
from pathlib import Path
from i03_dynamic_probe import _fixture,_configure


def run(candidate,output):
    from emu.session import Session
    from emu.harness import call
    from emu import intfrc
    from unicorn.m68k_const import UC_M68K_REG_PC
    root,meta,symbols,fw=_fixture(candidate);_configure(fw)
    sym=lambda k:int(symbols[k],0)
    def u32(s,a):return struct.unpack('>I',bytes(s.m.uc.mem_read(a,4)))[0]
    state={}
    def machine(s):
        sem=sym('worker_sem')
        assert sem in s.ev['unblock_skip'],'emulator would fake the worker wait'
        state['natural_real_wait']=True
        irq=intfrc.ForcedInterrupts(s.m,sources=(13,),base=0xfc050000,first_vector=192)
        if u32(s,0xfc050014)&(1<<13):irq.asserted.add(13)
        s.sources+=(irq,);state['irq']=irq
    s=Session(str(root/'assigned.snap'),audio=True,on_machine=machine)
    try:
        assert all(sym(k)%16==0 for k in ['worker_sem','worker_task','worker_stack'])
        before=u32(s,sym('usb_track_worker_runs'));s.run_ms(300)
        idle=u32(s,sym('usb_track_worker_runs'))-before
        assert 1<=idle<=40 and not s.halted,(idle,s.halted)
        # A producer signal must wake a genuinely sleeping native task.
        before=u32(s,sym('usb_track_worker_runs'))
        ctx=s.m.uc.context_save()
        try:
            s.m._fast_stepper_obj.left=100000
            call(s.m,sym('usb_track_control_wake'),[],limit=100000)
            assert s.m.uc.reg_read(UC_M68K_REG_PC)==0xdeadbee0
        finally:s.m.uc.context_restore(ctx)
        s.run_ms(5)
        wake=u32(s,sym('usb_track_worker_runs'))-before
        assert 1<=wake<=3 and not s.halted,(wake,s.halted)
        sem=sym('worker_sem');task=sym('worker_task');stack=sym('worker_stack')
        assert u32(s,sem+4)==task,'worker is not blocked on its own semaphore'
        sp=u32(s,task+0x48)
        assert sp%4==0 and stack+32<=sp<stack+32+8192
        guards=struct.pack('>8I',*([0x574f524b]*8))
        assert bytes(s.m.uc.mem_read(stack,32))==guards
        assert bytes(s.m.uc.mem_read(stack+32+8192,32))==guards
        assert u32(s,sym('usb_track_worker_guard_faults'))==0
        fake=s.ev.get('satisfied_by_sem',{}).get(sem,0);assert fake==0
        words=struct.unpack('>2048I',bytes(s.m.uc.mem_read(stack+32,8192)))
        unused=next((i*4 for i,x in enumerate(words) if x!=0xa5a5a5a5),8192)
        report={'task':'I04','scope':'compiled native RTOS emulator; no hardware/USB latency proof',
                'candidate_sha256':meta['syx_sha256'],'status':'verified',
                'checks':{'task_sem_stack_16byte_aligned':True,'stack_pointer_4byte_aligned':True,
                          'idle_runs_in300ms':idle,'signal_wakes_in5ms':wake,
                          'blocked_on_own_semaphore':True,'artificial_wait_satisfactions':fake,
                          'stack_guards_intact':True,'idle_path_observed_stack_used_bytes':8192-unused,
                          'modelled_intc2_reschedules':state['irq'].fired[13]},
                'limits':['Existing emulator models INTC0 but not INTC2 reschedules by default; probe explicitly adds INTC2 source13 using its generic interrupt model',
                          'Stack watermark covers idle/signalling paths only; compiled exchange exercises transport separately',
                          'Instruction/USB/DMA model does not qualify hardware worst-case latency or CPU headroom']}
        output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report['checks']))
    finally:s.close()

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.candidate,a.output)
