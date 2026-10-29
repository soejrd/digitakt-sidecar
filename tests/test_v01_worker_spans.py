"""Worker overlap accounting must use the native end-timer boundary."""
import importlib.util
import sys
from pathlib import Path
from types import SimpleNamespace
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
HAS_RUNTIME = importlib.util.find_spec('unicorn') is not None
if HAS_RUNTIME:
    from v01_worker_spans import WorkerSpans


@unittest.skipUnless(HAS_RUNTIME, 'run with the pinned emulator environment')
class WorkerSpansTest(unittest.TestCase):
    def make(self):
        spans = WorkerSpans.__new__(WorkerSpans)
        spans.start, spans.work, spans.render = 1, 5, 9
        values = {5: 0xfffffff0, 9: 0xfffffff0}
        uc = SimpleNamespace(mem_read=lambda address, size: values[address].to_bytes(size, 'big'))
        spans.s = SimpleNamespace(m=SimpleNamespace(uc=uc))
        spans.open_render = spans.end_render = None
        spans.rows, spans.errors, spans.end_pc = [], 0, 123
        return spans, uc, values

    def test_wrap_and_interrupt_after_end_read_do_not_corrupt_overlap(self):
        s, uc, v = self.make()
        s._write(uc, None, s.start, 4, 0, None)
        v[9] = 0x10  #32 render ticks, across wrap
        s._end_timer()
        v[9] = 0x1000  # later render must not count inside the finished bracket
        s._write(uc, None, s.work, 4, 0x40, None)  #80 elapsed ticks, across wrap
        self.assertEqual(s.rows, [(80, 32)])
        self.assertEqual(s.report()['batches_with_render_overlap'], 1)
        self.assertEqual(s.errors, 0)

    def test_invalid_overlap_is_rejected_instead_of_clamped(self):
        s, uc, v = self.make()
        s._write(uc, None, s.start, 4, 0, None)
        v[9] = 0x1000
        s._end_timer()
        s._write(uc, None, s.work, 4, 0, None)
        self.assertEqual(s.rows, [])
        self.assertEqual(s.errors, 1)


if __name__ == '__main__':
    unittest.main()
