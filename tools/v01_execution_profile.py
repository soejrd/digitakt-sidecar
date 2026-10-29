"""Exclusive block-cost attribution for the pinned OS1.54 timing model.

This partitions estimated executed cycles; it does not add inclusive worker
and render spans. Ready/wait time and physical cache/bus costs are not measured.
"""
import bisect
import collections

from v01_usb_device_model import u32


class ExecutionProfile:
    def __init__(self, session, symbols):
        from unicorn import UC_HOOK_BLOCK
        self.s = session
        self.cycles = collections.Counter()
        self.instructions = collections.Counter()
        self.functions = collections.Counter()
        self.last_cycles = self.last_instructions = 0
        self.ui_task = None
        self.worker_task = int(symbols['worker_task'], 0)
        self.cache = {}
        uc = session.m.uc
        assert bytes(uc.mem_read(0x40000418, 6)) == bytes.fromhex('20794399e798')
        addresses = sorted(set(int(v, 0) for v in symbols.values()
                               if isinstance(v, str) and v.startswith('0x')))
        self.ranges = [(0x40004990, 0x40004c78, 'native_serializer'),
                       (0x40004832, 0x400048c4, 'native_submission'),
                       (0x40004760, 0x40004832, 'native_rx_parser')]
        for name in ['usb_track_tick', 'usbmidi_tick', 'dt1_rx_service',
                     'sc_dev_receive_control', 'sc_decode_control']:
            start = int(symbols[name], 0)
            self.ranges.append((start, addresses[bisect.bisect_right(addresses, start)], name))

        def ui(*_):
            self.ui_task = u32(uc, 0x4399e798)

        session.at(session.profile.mainloop, ui)
        uc.hook_add(UC_HOOK_BLOCK, self._block)

    def _block(self, uc, address, size, data):
        clock = self.s.clock
        # CycleClock stops at the next block before charging it when its
        # budget is exhausted. Ignore that unexecuted boundary callback.
        cost = clock.cycles - self.last_cycles
        count = clock.instructions - self.last_instructions
        self.last_cycles, self.last_instructions = clock.cycles, clock.instructions
        if not count:
            return
        if clock._frames:
            owner = 'irq:%d' % clock._frames[-1][0]
        else:
            owner = 'task:0x%08x' % u32(uc, 0x4399e798)
        self.cycles[owner] += cost
        self.instructions[owner] += count
        name = self.cache.get(address)
        if name is None:
            name = next((n for lo, hi, n in self.ranges if lo <= address < hi), 'other')
            self.cache[address] = name
        self.functions[name] += cost

    def report(self):
        rows = {}
        for owner, cycles in sorted(self.cycles.items()):
            label = owner
            if owner == 'task:0x%08x' % self.worker_task:
                label = 'worker_task'
            elif self.ui_task is not None and owner == 'task:0x%08x' % self.ui_task:
                label = 'ui_task'
            rows[label] = {'estimated_cycles': cycles,
                           'entered_block_instructions': self.instructions[owner],
                           'fraction_of_model_cycles': round(cycles / self.s.clock.cycles, 6)}
        return {'exclusive_execution': rows,
                'selected_pc_range_self_cycles': dict(self.functions),
                'selected_pc_ranges': {n: ['0x%08x' % lo, '0x%08x' % hi]
                                       for lo, hi, n in self.ranges},
                'unattributed_cycles': self.s.clock.cycles - sum(self.cycles.values()),
                'limits': ['Estimated block costs, not measured hardware CPU utilization.',
                           'Branch resolution/exception entry can be charged at an adjacent block boundary.',
                           'Function ranges use linked entry symbols; unselected callees count as other.',
                           'Task state/runnable waiting and memory/cache/bus stalls are not attributed.']}
