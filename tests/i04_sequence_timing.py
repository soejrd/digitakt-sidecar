"""Known aligned-grid lag and malformed-reference rejection, no hardware."""
import sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from i04_analyze_sequence_timing import measure,reference_starts
ref=np.zeros(200000);src=ref.copy();ret=ref.copy()
rng=np.random.default_rng(104)
burst=rng.choice([-10000.,10000.],6000)
for t in [24000,48000,72000,96000,120000]:
 ref[t:t+20]=10000
 src[t+3000:t+9000]=burst
 ret[t+3240:t+9240]=burst*.3
ret+=ref*.001 # weak native-click bleed must not become the returned onset
rows,stats=measure(ref,src,ret)
assert len(rows)==5
for r in rows:
 assert r['source_offset_frames']==3000 and r['analog_offset_frames']==3240
 assert r['return_path_lag_frames']==240 and r['correlation']>.999
bad=ref.copy();bad[72000:72020]=0
try:reference_starts(bad)
except ValueError:pass
else:raise AssertionError('missing middle reference accepted')
try:measure(ref,src,np.zeros_like(ret))
except ValueError:pass
else:raise AssertionError('silent analog accepted')
print('I04 sequence timing: known source/analog offsets, reference cadence and silent-return rejection: PASS')
