"""The latency probe must not bypass a masked panel IRQ."""
import struct
import sys
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from v01_contention_probe import PanelArrival, UsbStatusArrival, audio_stream_valid
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


@unittest.skipUnless(HAS_RUNTIME, 'run with the pinned emulator Python/Unicorn environment')
class UsbStatusArrivalTest(unittest.TestCase):
    def make(self, schedule):
        uc = Memory()
        uc.mem_write(UsbStatusArrival.STATUS, bytes(4))
        calls = []

        def vector(number, **kwargs):
            calls.append((number, kwargs))
            return True

        session = SimpleNamespace(
            m=SimpleNamespace(uc=uc, raise_vector=vector), _t0=0,
            clock=SimpleNamespace(cycles=0), ips=250_000_000)
        return UsbStatusArrival(session, schedule), uc, calls

    @patch('emu.pit.interrupt_level', return_value=6)
    def test_status_waits_for_ipl_and_clears_pending_after_delivery(self, _level):
        source, uc, calls = self.make([100, 200])
        source.s.clock.cycles = 99
        self.assertFalse(source.service(99))
        self.assertFalse(source.pending)
        source.s.clock.cycles = 100
        self.assertFalse(source.service(100))
        self.assertTrue(source.pending)
        self.assertEqual(uc.mem_read(UsbStatusArrival.STATUS, 4),
                         struct.pack('>I', 4))
        uc.sr = 0x2600  # equal IPL 6 is still masked
        self.assertFalse(source.service(110))
        uc.sr = 0x2500
        self.assertTrue(source.service(120))
        self.assertEqual(calls, [(209, {'level': 6})])
        self.assertEqual(source.report()['delivered_209'], 1)
        self.assertEqual(source.report()['scheduled'], 1)

    @patch('emu.pit.interrupt_level', return_value=None)
    def test_disabled_status_vector_remains_pending(self, _level):
        source, _, calls = self.make([100])
        self.assertFalse(source.service(100))
        self.assertTrue(source.pending)
        self.assertEqual(calls, [])
        self.assertEqual(source.report()['pending'], 1)


class AudioStreamValidityTest(unittest.TestCase):
    def test_initial_queue_only_and_counter_gaps_cannot_pass(self):
        counts = {'render_blocks': 750, 'peer': {'audio': {
            'in_packets': 11, 'returns_scheduled': 11, 'counter_gaps': 0}}}
        self.assertFalse(audio_stream_valid(counts))
        audio = counts['peer']['audio']
        audio.update(in_packets=3432, returns_scheduled=3432)
        self.assertTrue(audio_stream_valid(counts))
        audio['counter_gaps'] = 1
        self.assertFalse(audio_stream_valid(counts))
        audio.update(counter_gaps=0, returns_scheduled=3431)
        self.assertFalse(audio_stream_valid(counts))


if __name__ == '__main__':
    unittest.main()
