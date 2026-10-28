#!/usr/bin/env python3
"""Compare simultaneous native-click and USB1 audible onsets in private WAVs.
An audio-onset reference is not an exact sequencer trigger timestamp. NumPy.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from i04_analyze_loopback import read_pcm, match


def reference_starts(reference):
    peak = float(max(abs(reference)))
    if peak <= 0:
        raise ValueError('silent native reference')
    starts = []
    for i in np.flatnonzero(abs(reference) > peak*.1):
        if not starts or i-starts[-1] > 12000:
            starts.append(int(i))
    if len(starts) < 4 or any(abs((b-a)-24000) > 32 for a,b in zip(starts,starts[1:])):
        raise ValueError('expected isolated native clicks every 500 ms at 120 BPM')
    return starts


def rms(samples):
    q = np.concatenate(([0.], np.cumsum(samples*samples)))
    return np.sqrt(np.maximum(0, (q[48:]-q[:-48])/48))


def onset(env, reference):
    baseline = float(np.median(env[max(0,reference-2400):reference]))
    window = env[reference:reference+12000]
    peak = float(max(window))
    threshold = max(peak*.1,baseline*3)
    if peak <= max(1.,threshold):
        raise ValueError('no distinct post-reference attack')
    hits = np.flatnonzero(window>threshold)
    if not len(hits):
        raise ValueError('no onset above threshold')
    return reference+int(hits[0])+47


def measure(reference, source, analog):
    if len({len(reference),len(source),len(analog)}) != 1:
        raise ValueError('channel length mismatch')
    starts = reference_starts(reference)
    se, ae = rms(source), rms(analog)
    rows = []
    for t in starts:
        if t+14400 >= len(source):
            raise ValueError('incomplete final reference window')
        s,a = onset(se,t),onset(ae,t)
        begin = max(0,s-960)
        lag,cc = match(source[begin:begin+9600],analog[begin:begin+14400])
        if abs(cc)<.9 or not 0<lag<4800:
            raise ValueError('source and analog waveform do not match')
        rows.append({'reference_frame':t,'source_offset_frames':s-t,
                     'analog_offset_frames':a-t,'return_path_lag_frames':lag,'correlation':cc})
    stats = {}
    for key in ['source_offset_frames','analog_offset_frames','return_path_lag_frames']:
        d = np.array([r[key] for r in rows])/48
        stats[key] = {'min_ms':float(min(d)),'median_ms':float(np.median(d)),
                      'max_ms':float(max(d)),'range_ms':float(max(d)-min(d))}
    return rows,stats


def analyze(folder, capture):
    rec = capture['rec']
    if rec['drops'] or rec['silence'] or rec['error']:
        raise ValueError('recording has drops, inserted silence or an error')
    names = ['04_SD.wav','13_VOICE1.wav','11_IN_L.wav']
    signals = [read_pcm(folder/name) for name in names]
    if len(signals[0]) != rec['frames']:
        raise ValueError('recorder length mismatch')
    rows,stats = measure(*signals)
    return {'task':'I04','hardware_test':True,'scope':'120 BPM simultaneous T1 USB1 and T2 native-click relative audio-onset offsets',
            'sample_rate_hz':48000,'reference_hits':len(rows),'stats':stats,'rows':rows,
            'method':{'native_reference':'first sample exceeding 10% absolute peak, >=250 ms refractory',
                      'remote_onsets':'1 ms trailing RMS first crossing max(10% post-reference peak, 3x pre-reference baseline); 50 ms pre-reference baseline, 250 ms search',
                      'return_match':'200 ms normalized waveform correlation, 0..100 ms search, >=0.9 required'},
            'recorder':{k:rec[k] for k in ['frames','drops','silence','outShort','error']},
            'capture_sha256':{name:hashlib.sha256((folder/name).read_bytes()).hexdigest() for name in names},
            'limits':['Native sample onset/leading silence is not calibrated to the sequencer trigger; these are observable relative audio offsets',
                      'RMS threshold and different attack shapes bias onset values; no exact event-to-audio claim',
                      'Analog and native reference are same-clock IN channels; OUT FIFO alignment uncertainty applies to the Pi source reference only (up to112 frames/2.333ms)',
                      'Short 21-hit run does not establish worst-case or long-run jitter',
                      'No manual-key latency or Pi render CPU margin measurement']}


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--folder',type=Path,required=True)
    ap.add_argument('--capture',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();result=analyze(a.folder,json.loads(a.capture.read_text()))
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'reference_hits':result['reference_hits'],'stats':result['stats']}))
