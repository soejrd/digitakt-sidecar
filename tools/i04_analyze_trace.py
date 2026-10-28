#!/usr/bin/env python3
"""Analyze private I04TRACE rows; export only compact timing metadata.
Full control frames/logs remain private. Does not infer a root cause from a gap.
"""
import argparse
import json
import re
from pathlib import Path

FIELDS = ['index', 'session', 'sequence', 'block', 'generation', 'flags', 'clock',
          'rendered', 'requested', 'fifo', 'previous_block', 'previous_due',
          'queued', 'accepted', 'reason', 'note', 'velocity']
FIELDS += [f'param{i}' for i in range(8)]
MASK = 0xffffffff

def delta(a, b):
    return (a-b) & MASK

def signed(value):
    return ((value+0x80000000) & MASK)-0x80000000

def analyze(text, delay=2048):
    rows=[]
    for match in re.finditer(r'I04TRACE row ([^\r\n]+)', text):
        numbers=list(map(int, match[1].split()))
        if len(numbers)!=len(FIELDS):
            raise ValueError('truncated trace row')
        row=dict(zip(FIELDS,numbers))
        if rows and row['index']!=rows[-1]['index']+1:
            raise ValueError('non-contiguous trace; use a single complete dump')
        rows.append(row)
    if not rows: raise ValueError('no trace rows')
    summary=[]
    previous=None
    for row in rows:
        due = (row['clock']+delay) & MASK if row['sequence']==1 else (
            row['previous_due']+delta(row['block'],row['previous_block'])*32) & MASK
        entry={'index':row['index'], 'accepted':row['accepted'], 'reason':row['reason'],
               'slack_frames':signed(due-row['clock']), 'fifo_frames':row['fifo'],
               'queued_events':row['queued']}
        if previous is not None and row['session']==previous['session']:
            entry.update(sender_frames=delta(row['block'],previous['block'])*32,
                         rendered_frames=delta(row['rendered'],previous['rendered']),
                         requested_frames=delta(row['requested'],previous['requested']))
        summary.append(entry); previous=row
    return {'scope':'timing metadata only; raw snapshots remain private',
            'retained_rows':len(rows), 'minimum_slack_frames':min(r['slack_frames'] for r in summary),
            'queue_peak_observed':max(r['queued_events'] for r in summary),
            'rows':summary, 'limits':['Generated/requested clocks wrap at 32 bits; interval analysis assumes <2^32 frames between rows',
                                    'A late row alone does not establish worker stall versus clock drift or host pacing cause']}

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--delay',type=int,default=2048)
    args=ap.parse_args()
    result=analyze(args.log.read_text(),args.delay)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(f"Analyzed {result['retained_rows']} private trace rows; minimum slack {result['minimum_slack_frames']} frames")
