"""Exercise the real HTTP watcher against a local deterministic Pi substitute."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
import unittest
import urllib.parse

spec = importlib.util.spec_from_file_location('capture', Path(__file__).resolve().parents[1]/'tools/v01_timing_capture.py')
watch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(watch)


class CaptureTest(unittest.TestCase):
    def run_capture(self, statuses, declared_rows=0):
        audit, messages = [], ['I04TRACE row OLD-DUMP\n']
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args): pass
            def do_POST(self):
                data = self.rfile.read(int(self.headers['Content-Length'])).decode()
                command = urllib.parse.parse_qs(data)['c'][0]
                audit.append(command)
                if command == 'trxstatus':
                    arm, faults = statuses.pop(0) if len(statuses)>1 else statuses[0]
                    messages.append(f'I04 arm {arm} queue 0 peak 1 faults {faults} applied 9 starts 1 consumed 1 frame 100\n')
                if command == 'trxtrace':
                    messages.append(f'I04TRACE count {declared_rows} frozen 1\n')
                self.send_response(200); self.end_headers(); self.wfile.write(b'ok\n')
            def do_GET(self):
                if self.path.startswith('/status'):
                    body = json.dumps({'uptime':1, 'underruns':8, 'streaming':True})
                else:
                    cursor = int(urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query)['since'][0])
                    log = ''.join(messages)
                    body = f'{cursor} {len(log)}\n'+log[cursor:]
                self.send_response(200); self.end_headers(); self.wfile.write(body.encode())
        server = HTTPServer(('127.0.0.1',0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        error = None
        try:
            with tempfile.TemporaryDirectory() as directory, contextlib.redirect_stdout(io.StringIO()):
                output = Path(directory)/'trial'
                try: watch.capture('http://127.0.0.1:'+str(server.server_port), output, 5)
                except RuntimeError as exc: error = str(exc)
                result = json.loads((output/'summary.json').read_text())
                self.assertNotIn('OLD-DUMP',(output/'trace.txt').read_text())
        finally:
            server.shutdown(); server.server_close(); thread.join()
        self.assertNotIn('trx=on',audit)
        self.assertEqual(audit[-3:], ['trx=off','trxtrace=off','trxtrace'])
        return result, error

    def test_historical_fault_does_not_stop_until_new_fault(self):
        result, error = self.run_capture([(0,2),(0,2),(0,3)])
        self.assertIsNone(error)
        self.assertEqual(result['reason'],'new playback fault')
        self.assertEqual(result['first']['faults'],2)
        self.assertEqual(result['last']['faults'],3)
        self.assertTrue(result['trace_dump_after_measurement'])

    def test_unexpected_arming_stops_and_mutes(self):
        result, error = self.run_capture([(0,0),(1,0)])
        self.assertIsNone(error)
        self.assertEqual(result['reason'],'unexpected armed voice')

    def test_truncated_trace_is_not_a_successful_collection(self):
        result, error = self.run_capture([(0,0),(0,1)],declared_rows=1)
        self.assertIn('Incomplete post-trial trace',error)
        self.assertFalse(result['trace_dump_after_measurement'])

    def test_missing_fresh_status_is_rejected(self):
        with self.assertRaises(ValueError): watch.voice_status('stale non-status log')


if __name__ == '__main__': unittest.main()
