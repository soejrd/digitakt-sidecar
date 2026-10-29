"""The latency probe must not bypass a masked panel IRQ."""
import struct
import sys
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from v01_contention_probe import PanelArrival
HAS_RUNTIME = importlib.util.find_spec('unicorn') is not None
if HAS_RUNTIME:
    from emu import panelin
    from unicorn.m68k_const import UC_M68K_REG_SR


class Memory:
    def __init__(self):
        self.data = {}
        self.sr = 0x2700

    def mem_write(self, address, data):
        self.data.update({address + i: byte for i, byte in enumerate(data)})

    def mem_read(self, address, size):
        return bytes(self.data.get(address + i, 0) for i in range(size))

    def reg_read(self, register):
        assert register == UC_M68K_REG_SR
        return self.sr


@unittest.skipUnless(HAS_RUNTIME, 'run with the pinned emulator Python/Unicorn environment')
class PanelArrivalTest(unittest.TestCase):
    def make(self, events):
        uc = Memory()
        uc.mem_write(0x100, struct.pack('>I', 0x2000))
        uc.mem_write(panelin.TCD34_DADDR, struct.pack('>I', 0x23ff))
        calls = []

        def vector(number, **kwargs):
            calls.append((number, kwargs))
            return True

        session = SimpleNamespace(
            m=SimpleNamespace(uc=uc, raise_vector=vector),
            profile=SimpleNamespace(uart8_ring_ptr=0x100), _t0=0,
            clock=SimpleNamespace(cycles=0),
            held=SimpleNamespace(press=lambda code: (3, 1),
                                 release=lambda code: (3, 0)))
        return PanelArrival(session, events), uc, calls

    @patch('emu.pit.interrupt_level', return_value=4)
    def test_masked_arrival_waits_and_wraps_dma_ring(self, _level):
        source, uc, calls = self.make([(100, 24, True)])
        source.s.clock.cycles = 100
        self.assertFalse(source.service(99))
        self.assertEqual(source.arrivals, [])
        self.assertFalse(source.service(100))
        self.assertEqual(calls, [])
        self.assertTrue(source.pending)
        self.assertEqual(uc.mem_read(0x23ff, 1), b'\x23')
        self.assertEqual(uc.mem_read(0x2000, 1), b'\x01')
        self.assertEqual(uc.mem_read(panelin.TCD34_DADDR, 4), struct.pack('>I', 0x2001))
        uc.sr = 0x2400  # equal priority is still masked
        self.assertFalse(source.service(110))
        uc.sr = 0x2300
        self.assertTrue(source.service(120))
        self.assertEqual(calls, [(154, {'level': 4})])
        self.assertFalse(source.service(130))
        self.assertEqual(len(calls), 1)

    @patch('emu.pit.interrupt_level', return_value=None)
    def test_disabled_interrupt_does_not_force_delivery(self, _level):
        source, uc, calls = self.make([(100, 24, True)])
        uc.sr = 0x2000
        self.assertFalse(source.service(100))
        self.assertTrue(source.pending)
        self.assertEqual(calls, [])


if __name__ == '__main__':
    unittest.main()
