#!/usr/bin/env python3
"""Summarize private H01 loopback captures; standard library only."""
import argparse
import array
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys


def latency(values):
    values=sorted(values)
    def percentile(p):
        index=(len(values)-1)*p
        lo=int(index)
        return values[lo]+(values[min(lo+1,len(values)-1)]-values[lo])*(index-lo)
    return {'count':len(values),'min_ms':min(values),'median_ms':statistics.median(values),
            'p95_ms':percentile(.95),'max_ms':max(values),'range_ms':max(values)-min(values)} if values else None


def summarize(metrics, raw=None):
    result=json.loads(metrics.read_text())
    result['host_observed_roundtrip']=latency(result.pop('host_observed_roundtrip_ms',[]))
    if raw:
        data=array.array('i');data.frombytes(raw.read_bytes())
        if sys.byteorder!='little':data.byteswap()
        assert len(data)%12==0
        channels=[data[c::12] for c in range(12)]
        signal=channels[10]
        first=next(i for i,v in enumerate(signal) if abs(v)>100000)
        segment=signal[first+120:first+1500]
        # Hann-windowed coherent projection; grid is a diagnostic, not a
        # calibrated sample clock. 48 kHz is independently checked by cadence.
        window=[v*(.5-.5*math.cos(2*math.pi*i/(len(segment)-1))) for i,v in enumerate(segment)]
        def amplitude(hz):
            a=sum(v*math.cos(2*math.pi*hz*i/48000) for i,v in enumerate(window))
            b=sum(v*math.sin(2*math.pi*hz*i/48000) for i,v in enumerate(window))
            return 4*math.hypot(a,b)/len(window)
        best=max(range(4200,4601),key=lambda f:amplitude(f/10))
        result['capture_analysis']={
            'sha256':hashlib.sha256(raw.read_bytes()).hexdigest(),
            'frames':len(signal),'observed_input_channel_zero_based':10,
            'channel_peaks':[max(map(abs,x)) for x in channels],
            'input_low_8_bits_zero':all(v%256==0 for v in signal),
            'tone_peak_hz_at_48k':best/10,'tone_amplitude_q31':amplitude(440),
            'limits':'Frequency grid assumes 48 kHz; analog volume/gain not calibrated.'}
    return result


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--metrics',type=Path,required=True)
    ap.add_argument('--raw',type=Path)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args()
    result=summarize(args.metrics,args.raw)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print('Transport PASS' if result['pass'] else 'Transport FAIL',result['host_observed_roundtrip'])


if __name__=='__main__':main()
