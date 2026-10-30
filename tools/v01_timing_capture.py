#!/usr/bin/env python3
"""Muted I04R timing capture. Dump the bounded trace only after measurement.

Use a new private output directory. A stop file ends the capture. An optional
action.json in that directory labels samples, so reconnect/load/edits can be
kept separate. No firmware, SD or project changes; never enables audio.
"""
import argparse
import json
from pathlib import Path
import re
import time
import urllib.parse
import urllib.request

VOICE = re.compile(r'I04 arm (\d+) queue (\d+) peak (\d+) faults (\d+) applied (\d+) starts (\d+) consumed (\d+) frame (\d+)')
FIELDS = ('armed', 'queue', 'peak', 'faults', 'applied', 'starts', 'consumed', 'frame')


def voice_status(text):
    rows = VOICE.findall(text)
    if not rows:
        raise ValueError('No fresh voice status in incremental log')
    return dict(zip(FIELDS, map(int, rows[-1])))


def capture(url, output, seconds):
    output.mkdir(parents=True, exist_ok=False)
    cursor = 0
    def get(path):
        return urllib.request.urlopen(url.rstrip('/')+path, timeout=5).read().decode()
    def command(value):
        request = urllib.request.Request(url.rstrip('/')+'/cmd',
            data=urllib.parse.urlencode({'c': value}).encode())
        if urllib.request.urlopen(request, timeout=5).read().strip() != b'ok':
            raise RuntimeError('Command refused: '+value)
    def log():
        nonlocal cursor
        header, _, body = get('/log?since='+str(cursor)).partition('\n')
        first, end = map(int, header.split())
        if cursor and first > cursor:
            raise RuntimeError('Incremental log lost bytes')
        cursor = end
        with (output/'raw-log.txt').open('a') as stream:
            stream.write(body)
        return body
    rows, error, reason = [], None, 'timeout'
    started = time.monotonic()
    try:
        command('trx=off')
        command('trxtrace=off')
        command('trxstatus'); time.sleep(.1)
        baseline = voice_status(log())
        if baseline['armed']:
            raise RuntimeError('Voice is armed')
        command('trxtrace=on')
        while True:
            command('trxstatus'); time.sleep(.1)
            body = log()
            status = voice_status(body)
            system = json.loads(get('/status'))
            mark = output/'action.json'
            action = json.loads(mark.read_text()) if mark.exists() else {'phase': 'idle'}
            row = {**status, 'elapsed_s': round(time.monotonic()-started, 3),
                   'uptime_s': system['uptime'], 'underrun_frames': system['underruns'],
                   'streaming': system['streaming'], 'action': action}
            faults = [line for line in body.splitlines() if 'I04 fault code ' in line]
            if faults: row['fault_line'] = faults[-1]
            rows.append(row)
            (output/'latest.json').write_text(json.dumps(row, indent=2)+'\n')
            with (output/'samples.jsonl').open('a') as stream:
                stream.write(json.dumps(row)+'\n')
            if status['armed']:
                reason = 'unexpected armed voice'; break
            if status['faults'] > baseline['faults']:
                reason = 'new playback fault'; break
            if not system['streaming']:
                reason = 'USB stream lost'; break
            if (output/'stop').exists():
                reason = 'operator stop'; break
            if time.monotonic()-started >= seconds:
                break
            time.sleep(1)
    except Exception as exc:
        error = str(exc)
        reason = 'capture error'
    finally:
        # End measurement before any trace printing, which can disturb USB.
        ended = time.monotonic()
        dumped, trace_count, frozen = False, None, None
        try:
            command('trx=off')
            command('trxtrace=off')
            log()
            command('trxtrace'); time.sleep(.5)
            body = log()
            (output/'trace.txt').write_text(body)
            headers = re.findall(r'I04TRACE count (\d+) frozen (\d+)', body)
            if not headers: raise ValueError('Missing post-trial trace header')
            trace_count, frozen = map(int, headers[-1])
            actual = len(re.findall(r'I04TRACE row ', body))
            if actual != trace_count:
                raise ValueError('Incomplete post-trial trace: expected %d, got %d' % (trace_count, actual))
            dumped = True
        except Exception as exc:
            error = (error+'; ' if error else '')+'collection: '+str(exc)
        summary = {'scope': 'muted hardware diagnostic, not automatic qualification',
                   'reason': reason, 'error': error, 'samples': len(rows),
                   'measurement_s': round(ended-started, 3),
                   'first': rows[0] if rows else None, 'last': rows[-1] if rows else None,
                   'trace_dump_after_measurement': dumped, 'trace_rows': trace_count, 'trace_frozen': frozen,
                   'limits': ['Action marks bound human actions; they are not exact panel-event timestamps.',
                              'Historical fault counts are baselined, not reset.',
                              'Counter growth alone does not attribute producer starvation.']}
        (output/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
        print(json.dumps(summary), flush=True)
    if error:
        raise RuntimeError(error)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=float, default=180)
    args = parser.parse_args()
    if not 0 < args.seconds <= 600:
        parser.error('--seconds must be in (0,600]')
    capture(args.url, args.output, args.seconds)
