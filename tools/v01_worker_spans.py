"""Observe native worker brackets and the render time nested within them.

The firmware counters remain unchanged. End-render state is sampled at the
native end-timer load, before a later interrupt could contaminate the span.
Non-render time includes other preemption and is an upper bound on worker CPU.
"""
import struct

from v01_usb_device_model import u32


class WorkerSpans:
    def __init__(self, session, symbols):
        from unicorn import UC_HOOK_MEM_WRITE
        self.s = session
        self.start = int(symbols['v01_batch_started'], 0)
        self.work = int(symbols['v01_work_ticks'], 0)
        self.render = int(symbols['v01_render_ticks'], 0)
        entry = int(symbols['usb-track:entry'], 0)
        uc = session.m.uc
        # Native inline: read DTCN0; subtract batch start; add to work total.
        pattern = bytes.fromhex('2039fc07000c90b9') + struct.pack('>I', self.start)
        pattern += bytes.fromhex('d1b9') + struct.pack('>I', self.work)
        code = bytes(uc.mem_read(entry, 1024))
        if code.count(pattern) != 1:
            raise RuntimeError('native worker end-timer sequence not uniquely verified')
        self.end_pc = entry + code.index(pattern)
        self.open_render = self.end_render = None
        self.rows = []
        self.errors = 0
        session.at(self.end_pc, self._end_timer)
        uc.hook_add(UC_HOOK_MEM_WRITE, self._write,
                    begin=self.start, end=self.start + 3)
        uc.hook_add(UC_HOOK_MEM_WRITE, self._write,
                    begin=self.work, end=self.work + 3)

    def _end_timer(self, *_):
        self.end_render = u32(self.s.m.uc, self.render)

    def _write(self, uc, access, address, size, value, data):
        if size != 4:
            self.errors += 1
            return
        if address == self.start:
            self.open_render = u32(uc, self.render)
            self.end_render = None
        elif self.open_render is not None and self.end_render is not None:
            elapsed = (value - u32(uc, self.work)) & 0xffffffff
            overlap = (self.end_render - self.open_render) & 0xffffffff
            if overlap > elapsed:
                self.errors += 1
            else:
                self.rows.append((elapsed, overlap))
            self.open_render = self.end_render = None
        else:
            self.errors += 1

    def report(self):
        rows = self.rows
        elapsed = sum(e for e, _ in rows)
        overlap = sum(r for _, r in rows)
        return {'complete_batches': len(rows), 'invalid_spans': self.errors,
                'unfinished': self.open_render is not None,
                'batches_with_render_overlap': sum(r > 0 for _, r in rows),
                'elapsed_us_total': round(elapsed / 132, 6),
                'render_overlap_us_total': round(overlap / 132, 6),
                'nonrender_us_total': round((elapsed - overlap) / 132, 6),
                'elapsed_us_max': round(max((e for e, _ in rows), default=0) / 132, 6),
                'nonrender_us_max': round(max((e - r for e, r in rows), default=0) / 132, 6),
                'end_timer_pc': '0x%08x' % self.end_pc,
                'limits': 'Assumed132MHz timer. Non-render includes other IRQ/task preemption; not pure CPU.'}
