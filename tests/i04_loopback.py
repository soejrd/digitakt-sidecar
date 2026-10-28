"""Synthetic lag, polarity/gain/DC invariance and false-match rejection."""
import importlib.util
from pathlib import Path
import numpy as np
spec=importlib.util.spec_from_file_location('loopback',Path(__file__).resolve().parents[1]/'tools/i04_analyze_loopback.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
rng=np.random.default_rng(104)
x=rng.normal(size=9600)*10000
for lag,gain in [(149,.4),(241,-.04),(4799,.7)]:
 y=rng.normal(size=14400)*.001+37
 y[lag:lag+len(x)]+=gain*x
 found,cc=m.match(x,y)
 assert found==lag and abs(cc)>.999,(found,cc)
_,cc=m.match(x,rng.normal(size=14400))
assert abs(cc)<.1
try:m.match(np.zeros(9600),np.zeros(14400))
except ValueError:pass
else:raise AssertionError('silent template accepted')
y=np.zeros(8*48000)
for i in range(1,6):y[i*48000:i*48000+9600]=x
assert len(m.onsets(y,5))==5
try:m.onsets(y,6)
except ValueError:pass
else:raise AssertionError('missing hit accepted')
try:m.analyze(Path('/nonexistent'),{'rec':{'drops':1,'silence':0,'error':''}})
except ValueError:pass
else:raise AssertionError('dropped recording accepted')
print('I04 loopback: known sample lags, polarity/gain/DC, weak/silent rejection and expected hit count: PASS')
