#!/usr/bin/env python3
"""Measure captured source-to-Digitakt-MAIN pulse delays; no hardware access."""
import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import statistics
import wave


def read(path):
    with wave.open(str(path), 'rb') as w:
        assert (w.getnchannels(), w.getsampwidth(), w.getframerate()) == (1, 3, 48000)
        raw = w.readframes(w.getnframes())
    return array('i', (int.from_bytes(raw[i:i+3], 'little', signed=True)
                       for i in range(0, len(raw), 3)))


def energy(samples, window=16):
    """One 3 kHz period, advanced one sample at a time; suppress carrier phase."""
    value = sum(x*x for x in samples[:window])
    result = array('d', [value])
    for i in range(window, len(samples)):
        value += samples[i]*samples[i] - samples[i-window]*samples[i-window]
        result.append(value)
    return result


def falls(power):
    threshold = max(power)*0.5
    assert threshold > 0, 'capture is silent'
    hits = []
    for i in range(24000, len(power)):  # planned recorder warm-up, before the first pulse
        if power[i-1] >= threshold > power[i] and (not hits or i-hits[-1] > 4800):
            hits.append(i)
    return hits


def analyze(folder, capture):
    names = ('13_VOICE1.wav', '01_MAIN_L.wav', '02_MAIN_R.wav')
    samples = {name: read(folder/name) for name in names}
    assert len({len(x) for x in samples.values()}) == 1
    source = falls(energy(samples[names[0]]))
    assert len(source) == 12, source
    channels = {}
    for name in names[1:]:
        returned = falls(energy(samples[name]))
        if len(returned) != len(source):
            channels[name] = {'falling_edge_frames':returned, 'pulse_count':len(returned),
                              'measurement_available':False,
                              'peak_after_warmup':max(map(abs,samples[name][24000:])),
                              'reason':'No matching returned pulse train in digital USB MAIN; latency cannot be measured from this take.'}
            continue
        delays = [b-a for a,b in zip(source, returned)]
        assert all(0 < x < 4800 for x in delays), delays
        ms = [x/48 for x in delays]
        channels[name] = {'falling_edge_frames':returned,'delay_frames':delays,
                          'delay_ms':ms,'min_ms':min(ms),'median_ms':statistics.median(ms),
                          'max_ms':max(ms),'range_ms':max(ms)-min(ms),
                          'peak_dbfs':20*math.log10(max(map(abs,samples[name]))/8388608)}
    recorder = json.loads(capture.read_text())['recorder']
    assert recorder['drops'] == 0 and not recorder['error']
    return {'task':'H02','hardware_test':True,'pass':all('delay_frames' in v for v in channels.values()),
            'scope':'attempted recorder-relative Pi source -> USB -> Digitakt track -> USB MAIN return; dry steady amp, filter off, sends/overdrive off',
            'sample_rate_hz':48000,'frames':len(samples[names[0]]),'pulse_count':12,
            'ignored_initial_frames':24000,'source_falling_edge_frames':source,'channels':channels,
            'capture_sha256':{name:hashlib.sha256((folder/name).read_bytes()).hexdigest() for name in names},
            'recorder':{k:recorder[k] for k in ('frames','drops','outShort','error')},
            'source_alignment_bound_ms':112/48,
            'missing_reference_frames_total':recorder['outShort'],
            'limits':['OUT voice reference uses the recorder side FIFO; documented possible 16-packet lead is 112 frames (2.333 ms). Values are recorder-relative, not a calibrated USB-bus timestamp.',
                      'Recorder inserted zero reference samples for outShort frames; no card-ring drops. This is separate from engine FIFO underruns.',
                      'Threshold is half of one-period power, at one-sample steps; up to one-sample edge quantization per channel.',
                      'Measures returned PCM path, not note-to-synthesis latency, ColdFire render headroom or analog-output latency.']}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--folder', type=Path, required=True)
    p.add_argument('--capture', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    result = analyze(a.folder, a.capture)
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k:{f:v.get(f) for f in ('measurement_available','pulse_count','min_ms','median_ms','max_ms','range_ms')} for k,v in result['channels'].items()}))
