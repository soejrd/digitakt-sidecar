import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from i04_analyze_trace import analyze

def row(values):
    assert len(values)==25
    return 'I04TRACE row '+' '.join(map(str,values))+'\n'
first=[0,9,2,100,1,0,54516,100000,99900,96,99,54464,7,0,3,0,0]+[0]*8
second=[1,9,3,101,1,0,54520,100012,99907,101,100,54496,7,1,0,0,0]+[0]*8
result=analyze(row(first)+row(second))
assert result['rows'][0]['slack_frames']==-20
assert result['rows'][1]['sender_frames']==32
assert result['rows'][1]['rendered_frames']==12
assert result['rows'][1]['requested_frames']==7
assert not any(k in str(result) for k in ['param0','velocity','note','generation'])
try:
    analyze('I04TRACE row 1 2 3\n')
    raise AssertionError('truncation was accepted')
except ValueError:
    pass
print('I04 trace analyzer: signed deadline slack, source/render/request deltas, truncation rejection and private payload omission: PASS')
