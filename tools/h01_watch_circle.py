#!/usr/bin/env python3
"""Read Circle status/log for a bounded stability run. No commands sent."""
import argparse
import json
from pathlib import Path
import re
import time
import urllib.request

PATTERN=re.compile(r'(\d+)ms IN \d+/s (\d+) gap (\d+) lost (\d+) max \d+ odd (\d+) sh (\d+) bad (\d+) err (\d+).*?OUT \d+/s (\d+) err (\d+).*?drop (\d+).*?sf (\d+)/(\d+)')
KEYS=['stream_ms','in_blocks','gaps','lost','odd_gaps','short','bad_headers','in_errors','out_blocks','out_errors','credit_drops','in_submit_fail','out_submit_fail']
ERRORS=[x for x in KEYS if x not in ('stream_ms','in_blocks','out_blocks')]


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--url',required=True)
    ap.add_argument('--seconds',type=float,default=240)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--log',type=Path,required=True)
    args=ap.parse_args()
    def get(path):
        with urllib.request.urlopen(args.url.rstrip('/')+path,timeout=5) as r:return r.read().decode()
    start=time.monotonic();cursor=0;stats=[];statuses=[];network_errors=[]
    args.log.parent.mkdir(parents=True,exist_ok=True)
    with args.log.open('w') as log:
        while True:
            elapsed=time.monotonic()-start
            try:
                status=json.loads(get('/status'))
                statuses.append({'elapsed':elapsed,**{k:status.get(k) for k in ('uptime','streaming','link','out','underruns','temp','version','commit')}})
                chunk=get('/log?since='+str(cursor))
                header,body=chunk.split('\n',1)
                first,last=map(int,header.split())
                if cursor and first>cursor:network_errors.append('log cursor overrun')
                cursor=last
                log.write(body);log.flush()
                for match in PATTERN.finditer(body):stats.append(dict(zip(KEYS,map(int,match.groups()))))
                print(round(elapsed),statuses[-1],stats[-1] if stats else {},flush=True)
            except Exception as e:
                network_errors.append(type(e).__name__+': '+str(e))
                print('read failed',type(e).__name__,flush=True)
            if elapsed>=args.seconds:break
            time.sleep(min(5,args.seconds-elapsed))
    # Only the first matching line near this observation's start is baseline;
    # an initial log fetch can include the complete earlier boot history.
    if stats:
        baseline=next((x for x in stats if x['stream_ms']>=stats[-1]['stream_ms']-args.seconds*1000-2000),stats[0])
        final=stats[-1]
    else:baseline=final={}
    delta={k:final[k]-baseline[k] for k in KEYS} if stats else {}
    under=[s['underruns'] for s in statuses if s['underruns'] is not None]
    assertions={
        'network_reads_complete':not network_errors,
        'streaming_throughout':bool(statuses) and all(s['streaming'] for s in statuses),
        'usb_link_and_main':bool(statuses) and all(s['link']=='usb' and s['out']=='main' for s in statuses),
        'four_minute_device_window':bool(stats) and delta['stream_ms']>=args.seconds*1000-2000,
        'in_and_out_progress':bool(stats) and delta['in_blocks']>args.seconds*6000 and delta['out_blocks']>args.seconds*6000,
        'no_new_usb_errors_or_gaps':bool(stats) and all(delta[k]==0 for k in ERRORS),
        'no_new_engine_underruns':bool(under) and max(under)==min(under),
    }
    report={'platform':'Pi 4B Circle Sidecar','hardware_test':True,'observation_seconds':time.monotonic()-start,
        'status_samples':len(statuses),'transport_samples':len(stats),'baseline':baseline,'final':final,'delta':delta,
        'initial_underruns':under[0] if under else None,'final_underruns':under[-1] if under else None,
        'max_temperature_c':max((s['temp'] or 0 for s in statuses),default=0),
        'network_errors':network_errors,'assertions':assertions,'pass':all(assertions.values()),
        'limits':['Silence stream; audio-return loopback/timing was measured separately with the Linux diagnostic.']}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('Circle PASS' if report['pass'] else 'Circle FAIL',assertions,flush=True)
    if not report['pass']:raise SystemExit(1)


if __name__=='__main__':main()
