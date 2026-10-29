#!/usr/bin/env python3
"""Poll the paired Pi diagnostic receiver; never arm audio or change device settings.
Raw timer fields are ticks, NOT microseconds. Calibrate before interpreting.
"""
import argparse,json,re,time,urllib.parse,urllib.request
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--url',required=True);p.add_argument('--seconds',type=float,default=20)
    p.add_argument('--stop-file',type=Path)
    p.add_argument('--quiet',action='store_true')
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.seconds<0 or a.seconds>3600:p.error('--seconds must be between 0 and 3600')
    def get(path):
        with urllib.request.urlopen(a.url.rstrip('/')+path,timeout=5) as r:return r.read()
    header=re.compile(r'V01REMOTE rx (\d+) bad (\d+) seq (\d+) block (\d+) timer (\d+) mode (\d+) ui (\d+) work (\d+) ugap (\d+) wgap (\d+) batch (\d+) guards (\d+)')
    total=re.compile(r'V01TOTAL seq (\d+) worker (\d+) render (\d+) version (\d+)')
    raw=re.compile(r'V01RAW seq (\d+) sends (\d+) total (\d+) max (\d+) ui_max (\d+) batch_max (\d+)')
    fields=('received','bad','sequence','block','timer_ticks','timer_mode','ui','worker','ui_gap_ms','worker_gap_ms','batch_ms','guards')
    rawfields=('sequence','send_calls','send_ticks_total','send_ticks_max','ui_ticks_max','batch_ticks_max')
    start=time.monotonic();samples=[];errors=[]
    a.output.parent.mkdir(parents=True,exist_ok=True)
    def save():
        temp=a.output.with_name(a.output.name+'.tmp')
        temp.write_text(json.dumps({'scope':'remote diagnostic samples; not an automatic acceptance verdict','samples':samples,'errors':errors,'limits':['Raw tick rate not assumed; modulo-32-bit differences required','A repeated sequence is stale data, not zero-cost work','No audio arming or knob changes performed']},indent=2)+'\n')
        temp.replace(a.output)
    try:
        while True:
            for command in ('v01status','trxstatus'):
                req=urllib.request.Request(a.url.rstrip('/')+'/cmd',data=urllib.parse.urlencode({'c':command}).encode())
                with urllib.request.urlopen(req,timeout=5) as r:
                    if r.read()!=b'ok\n':raise RuntimeError('command not acknowledged')
            time.sleep(.15)
            log=get('/log').decode();heads=header.findall(log);raws=raw.findall(log)
            if not heads or not raws:raise RuntimeError('receiver absent or no v01status response')
            row=dict(zip(fields,map(int,heads[-1])));rr=dict(zip(rawfields,map(int,raws[-1])))
            if row['sequence']!=rr['sequence']:raise RuntimeError('incomplete snapshot log pair')
            voice=re.findall(r'I04 arm (\d+) queue (\d+) peak (\d+) faults (\d+) applied (\d+) starts (\d+) consumed (\d+) frame (\d+)',log)
            if not voice:raise RuntimeError('Pi voice status missing')
            row.update(dict(zip(('armed','queue','peak','faults','applied','starts','consumed','frame'),map(int,voice[-1]))))
            totals=total.findall(log)
            if totals and int(totals[-1][0])==row['sequence']:
                row['protocol_version']=int(totals[-1][3])
                if row['protocol_version']==2:
                    row['worker_ticks_total']=int(totals[-1][1])
                    row['render_ticks_total']=int(totals[-1][2])
            row.update(rr);row['elapsed_s']=round(time.monotonic()-start,3)
            s=json.loads(get('/status'));row['fifo_underruns']=s['underruns'];row['pi_uptime']=s['uptime']
            samples.append(row);save()
            if not a.quiet:print(json.dumps(row),flush=True)
            if row['armed']:raise RuntimeError('voice armed; muted diagnostic trial required')
            if row['faults']:raise RuntimeError('Pi playout fault; stop trial')
            if row['fifo_underruns']!=samples[0]['fifo_underruns']:raise RuntimeError('new FIFO underrun; stop trial')
            row['stale']=len(samples)>1 and row['sequence']==samples[-2]['sequence']
            if row['received']==0:raise RuntimeError('no device snapshots received; not a responsive-device proof')
            save()
            if (a.stop_file and a.stop_file.exists()) or time.monotonic()-start>=a.seconds:break
            time.sleep(min(2,max(0,a.seconds-(time.monotonic()-start))))
    except Exception as e:errors.append(type(e).__name__+': '+str(e))
    finally:
        save()
        print('Remote capture saved:',len(samples),'samples;',len(errors),'errors',flush=True)
    if errors:raise SystemExit(errors[-1])
if __name__=='__main__':main()
