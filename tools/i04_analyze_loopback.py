#!/usr/bin/env python3
"""Match private 48 kHz/24-bit mono source/return WAVs; emit timing metadata.
Requires NumPy. This measures recorder-relative return path, not trig latency.
"""
import argparse
import hashlib
import json
from pathlib import Path
import wave
import numpy as np


def read_pcm(path):
    with wave.open(str(path), 'rb') as w:
        if (w.getnchannels(), w.getsampwidth(), w.getframerate()) != (1, 3, 48000):
            raise ValueError('expected mono 24-bit 48 kHz PCM')
        raw = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8)
    if len(raw) % 3:
        raise ValueError('truncated 24-bit PCM')
    a = raw.reshape(-1, 3).astype(np.int32)
    return (((a[:, 0] | (a[:, 1] << 8) | (a[:, 2] << 16)) ^ 0x800000)
            - 0x800000).astype(float)


def onsets(samples, expected):
    """Coarse 10 ms windows select templates; not the timing estimator."""
    env = np.sqrt(np.mean(samples[:len(samples)//480*480].reshape(-1, 480)**2, axis=1))
    if not len(env) or max(env) <= 0:
        raise ValueError('silent source')
    high = env > max(env) * .25
    starts = []
    for i in range(1, len(high)):
        if high[i] and not high[i-1] and (not starts or i*480-starts[-1] >= 24000):
            starts.append(i*480)
    if len(starts) != expected:
        raise ValueError(f'expected {expected} isolated source hits, found {len(starts)}')
    return starts


def match(template, returned, max_lag=4800):
    if len(returned) != len(template)+max_lag or not len(template):
        raise ValueError('incomplete correlation window')
    x = template-template.mean()
    if np.dot(x, x) <= 0:
        raise ValueError('silent template')
    n = len(x)
    size = 1 << (len(returned)+n-1).bit_length()
    corr = np.fft.irfft(np.fft.rfft(returned, size) *
                       np.conj(np.fft.rfft(x, size)), size)[:max_lag+1]
    sums = np.concatenate(([0.], np.cumsum(returned)))
    sq = np.concatenate(([0.], np.cumsum(returned*returned)))
    energy = (sq[n:n+max_lag+1]-sq[:max_lag+1] -
              (sums[n:n+max_lag+1]-sums[:max_lag+1])**2/n)
    cc = corr/np.sqrt(np.maximum(energy, 1)*np.dot(x, x))
    lag = int(np.argmax(abs(cc)))
    return lag, float(cc[lag])


def analyze(folder, capture, expected=5):
    recorder = capture['rec']
    if recorder['drops'] or recorder['silence'] or recorder['error']:
        raise ValueError('recording has drops, inserted silence or an error')
    names = ['13_VOICE1.wav', '01_MAIN_L.wav', '02_MAIN_R.wav', '11_IN_L.wav', '12_IN_R.wav']
    source = read_pcm(folder/names[0])
    if len(source) != recorder['frames']:
        raise ValueError('source length differs from recorder count')
    starts = onsets(source, expected)
    channels = {}
    for name in names[1:]:
        samples = read_pcm(folder/name)
        if len(samples) != len(source):
            raise ValueError('recording channels are not aligned in length')
        pairs = []
        for start in starts:
            begin = max(0, start-960)
            lag, cc = match(source[begin:begin+9600], samples[begin:begin+14400])
            pairs.append({'lag_frames': lag, 'correlation': cc})
        ok = all(abs(p['correlation']) >= .9 and 0 < p['lag_frames'] < 4800 for p in pairs)
        entry = {'measurement_available': ok, 'matches': pairs}
        if ok:
            delays = np.array([p['lag_frames'] for p in pairs])/48
            entry.update(min_ms=float(min(delays)), median_ms=float(np.median(delays)),
                         max_ms=float(max(delays)), range_ms=float(max(delays)-min(delays)))
        else:
            entry = {'measurement_available': False, 'peak_abs_correlation': max(abs(p['correlation']) for p in pairs),
                     'reason': 'No consistent >=0.9 waveform match inside the 0..100 ms search window'}
        channels[name] = entry
    return {'task': 'I04', 'hardware_test': True,
            'scope': 'recorder-relative Pi voice source to USB MAIN and analog loopback; excludes trigger/control and scheduler delay',
            'sample_rate_hz': 48000, 'frames': len(source), 'source_hits': len(starts),
            'template_start_reference_frames': starts, 'channels': channels,
            'capture_sha256': {name: hashlib.sha256((folder/name).read_bytes()).hexdigest() for name in names},
            'recorder': {k: recorder[k] for k in ['frames', 'drops', 'silence', 'outShort', 'error']},
            'source_alignment_bound_ms': 112/48,
            'limits': ['OUT reference FIFO may lead by up to 112 frames (2.333 ms); not calibrated USB-bus timestamps',
                       'OUT-side short frames are counted separately; high correlation does not remove alignment uncertainty',
                       'Lag fits 200 ms waveforms at integer-sample resolution; five matches do not establish long-run jitter',
                       'Analog loopback includes DAC/ADC and local processing; it is not a pure output-only delay',
                       'No trigger-to-audio, control-delivery, 2048-frame buffer, or SRC filter delay measurement follows from this take']}


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--folder', type=Path, required=True)
    ap.add_argument('--capture', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    result = analyze(args.folder, json.loads(args.capture.read_text()))
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k: {a: b for a, b in v.items() if a != 'matches'} for k, v in result['channels'].items()}))
