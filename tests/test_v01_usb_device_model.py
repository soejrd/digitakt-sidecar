"""Native USB controller model: prime, completion and RX ownership."""
import importlib.util
import struct
import sys
import unittest
from unittest.mock import patch
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
HAS_RUNTIME = importlib.util.find_spec('unicorn') is not None
if HAS_RUNTIME:
    from v01_usb_device_model import (AUDIO_ENDPOINT, BASE, ENDPTCOMPLETE,
                                  ENDPTPRIME, ENDPTSTAT, USBSTS, USBINTR, USBCMD, QUEUE_BASE,
                                  QUEUE_OFFSETS, TOKEN, ACTIVE, LENGTH,
                                  TRANSFER_BUFFER, ENDPOINT_BITS, UsbDeviceModel, UsbStatusIrq, put_u32,
                                  u32)


@unittest.skipUnless(HAS_RUNTIME, 'run with the pinned emulator environment')
class UsbDeviceModelTest(unittest.TestCase):
    def make(self, rx=None):
        from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN
        uc = Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN)
        uc.mem_map(0x40000000, 0x100000)
        uc.mem_map(0x80000000, 0x100000)
        uc.mem_map(BASE, 0x4000)
        machine = type('Machine', (), {'uc': uc, 'ensure': lambda self, a, n=1: None})()
        model = UsbDeviceModel(machine, fsys=250_000_000, rx_bytes=rx)
        model.configure_component()
        return model, uc

    def prime(self, uc, endpoint, descriptor, length):
        qh = QUEUE_BASE + QUEUE_OFFSETS[endpoint]
        put_u32(uc, qh + 0x30, descriptor)
        put_u32(uc, qh + 8, descriptor)
        put_u32(uc, descriptor, 1)
        put_u32(uc, descriptor + 8, 0x80003000)
        put_u32(uc, descriptor + TOKEN, ACTIVE | 0x8000 | (length << 16))
        put_u32(uc, descriptor + LENGTH, length)

    def write_guest(self, uc, address, value):
        from unicorn.m68k_const import UC_M68K_REG_D0, UC_M68K_REG_PC
        uc.mem_write(0x40001000, b'\x23\xc0' + struct.pack('>I', address))
        uc.reg_write(UC_M68K_REG_D0, value)
        uc.reg_write(UC_M68K_REG_PC, 0x40001000)
        uc.emu_start(0x40001000, 0x40001006)

    def test_audio_prime_completes_one_descriptor_and_sets_status(self):
        model, uc = self.make()
        desc = 0x80001000
        self.prime(uc, AUDIO_ENDPOINT, desc, 56)
        self.write_guest(uc, BASE + ENDPTPRIME, 1 << AUDIO_ENDPOINT)
        self.assertTrue(model.service(0))
        self.assertEqual(model.primed, 1)
        self.assertEqual(u32(uc, BASE + ENDPTSTAT), 1 << AUDIO_ENDPOINT)
        self.assertEqual(len(model.transfers), 1)
        self.assertFalse(model.service(1))
        self.assertEqual(model.completed, 0)
        model.queue_rx(AUDIO_ENDPOINT, bytes(56), 2)
        self.assertTrue(model.service(100_000))
        self.assertEqual(model.completed, 1)
        self.assertEqual(u32(uc, desc + TOKEN) >> 16, 0)
        self.assertEqual(u32(uc, desc + TOKEN) & 0x8000, 0x8000)
        self.assertEqual(u32(uc, BASE + ENDPTCOMPLETE), 1 << AUDIO_ENDPOINT)
        self.assertEqual(model.status & 1, 1)

    def test_completion_records_frame_not_microframe_and_preserves_page(self):
        from v01_usb_device_model import FRAME
        model, uc = self.make()
        desc = 0x80001000
        self.prime(uc, 5, desc, 60)
        put_u32(uc, desc + FRAME, 0x80004000)
        self.write_guest(uc, BASE + ENDPTPRIME, ENDPOINT_BITS[5])
        model.service(0)
        model.service(343_750)  # microframe11, frame1 at assumed250MHz
        self.assertEqual(u32(uc, desc + FRAME), 0x80004001)

    def test_rx_descriptor_receives_bytes_and_is_not_reprimed(self):
        model, uc = self.make()
        desc = 0x80002000
        buffer = 0x80003000
        self.prime(uc, 2, desc, 64)
        put_u32(uc, desc + TRANSFER_BUFFER, buffer)
        self.write_guest(uc, BASE + ENDPTPRIME, 1 << 2)
        model.service(0)
        model.service(100_000)
        self.assertEqual(model.completed, 0)  # no unsolicited OUT completion
        model.queue_rx(2, b'\x01\x02\x03\x04', 100_001)
        model.service(200_000)
        self.assertEqual(model.rx_dispatched, 1)
        self.assertEqual(bytes(uc.mem_read(buffer, 4)), b'\x01\x02\x03\x04')
        self.assertEqual(model.completed, 1)
        self.assertEqual(u32(uc, desc + TOKEN) >> 16, 60)
        self.assertEqual(u32(uc, desc + LENGTH), 64)

    def test_write_one_to_clear_does_not_leave_stale_completion(self):
        model, uc = self.make()
        desc = 0x80001000
        self.prime(uc, AUDIO_ENDPOINT, desc, 56)
        model.queue_rx(AUDIO_ENDPOINT, bytes(56), 0)
        self.write_guest(uc, BASE + ENDPTPRIME, 1 << AUDIO_ENDPOINT)
        model.service(0)
        model.service(100_000)
        self.assertTrue(model.completed_bits)
        self.write_guest(uc, BASE + ENDPTCOMPLETE, 1 << AUDIO_ENDPOINT)
        model.service(100_001)
        self.assertEqual(model.completed_bits, 0)
        self.assertEqual(u32(uc, BASE + ENDPTCOMPLETE), 0)

    def test_native_in_bit_and_chained_descriptors(self):
        model, uc = self.make()
        first, second = 0x80001000, 0x80001100
        self.prime(uc, 5, first, 60)
        put_u32(uc, first, second)
        put_u32(uc, second, 1)
        put_u32(uc, second + TOKEN, (24 << 16) | ACTIVE | 0x8000)
        put_u32(uc, second + LENGTH, 24)
        self.write_guest(uc, BASE + ENDPTPRIME, ENDPOINT_BITS[5])
        model.service(0)
        model.service(100_000)
        self.assertEqual(model.completed, 1)
        self.assertEqual(model.stat, ENDPOINT_BITS[5])
        model.service(200_000)
        self.assertEqual(model.completed, 2)
        self.assertEqual(model.stat, 0)
        self.assertEqual(model.completed_bits, 1 << 17)

    def test_ack_cannot_clear_a_later_completion_in_same_service(self):
        model, uc = self.make()
        first, second = 0x80001000, 0x80001100
        self.prime(uc, 5, first, 60)
        put_u32(uc, first, second)
        put_u32(uc, second, 1)
        put_u32(uc, second + TOKEN, (24 << 16) | ACTIVE | 0x8000)
        put_u32(uc, second + LENGTH, 24)
        self.write_guest(uc, BASE + ENDPTPRIME, ENDPOINT_BITS[5])
        model.service(0)
        model.service(100_000)
        self.assertEqual(model.status, 1)
        self.write_guest(uc, BASE + USBSTS, 1)
        self.write_guest(uc, BASE + ENDPTCOMPLETE, ENDPOINT_BITS[5])
        model.service(200_000)
        self.assertEqual(model.status, 1)
        self.assertEqual(model.completed_bits, ENDPOINT_BITS[5])

    @patch('emu.pit.interrupt_level', return_value=6)
    def test_controller_irq_mask_blocks_delivery(self, _level):
        from unicorn.m68k_const import UC_M68K_REG_SR
        model, uc = self.make()
        calls = []
        model.m.raise_vector = lambda *a, **k: calls.append((a, k)) or True
        uc.reg_write(UC_M68K_REG_SR, 0x2000)
        irq = UsbStatusIrq(model)
        model.status = 1
        self.write_guest(uc, BASE + USBINTR, 0)
        self.assertFalse(irq.service(0))
        self.assertEqual(calls, [])
        self.write_guest(uc, BASE + USBINTR, 1)
        self.assertTrue(irq.service(1))
        self.assertEqual(calls, [((209,), {'level': 6})])

    def test_native_10ms_timer_and_sof_latch_independently_of_irq_mask(self):
        from v01_usb_device_model import GPTLD, GPTCTL
        model, uc = self.make()
        model.periodic = True
        self.write_guest(uc, BASE + GPTLD, 9999)
        self.write_guest(uc, BASE + GPTCTL, 0x81000000)
        self.write_guest(uc, BASE + USBINTR, 0)
        model.service(0)
        model.service(2_500_000)
        self.assertEqual(model.timer_ticks, 1)
        self.assertEqual(model.sof_ticks, 80)
        self.assertEqual(model.status, (1 << 24) | 0x80)
        self.write_guest(uc, BASE + USBSTS, 1 << 24)
        model.service(2_500_001)
        self.assertEqual(model.status, 0x80)
        model.service(5_000_000)
        self.assertEqual(model.timer_ticks, 2)
        self.assertTrue(model.status & (1 << 24))
        self.write_guest(uc, BASE + GPTCTL, 0)
        model.service(7_500_000)
        self.assertEqual(model.timer_ticks, 2)

    @patch('emu.pit.interrupt_level', return_value=6)
    def test_timer_irq_can_deliver_without_completion_bit(self, _level):
        from unicorn.m68k_const import UC_M68K_REG_SR
        model, uc = self.make()
        calls = []
        model.m.raise_vector = lambda *a, **k: calls.append((a, k)) or True
        uc.reg_write(UC_M68K_REG_SR, 0x2000)
        model.status = 1 << 24
        irq = UsbStatusIrq(model)
        self.assertFalse(irq.service(0))
        self.write_guest(uc, BASE + USBINTR, 1 << 24)
        self.assertTrue(irq.service(1))
        self.assertEqual(calls, [((209,), {'level': 6})])

    def test_stopped_controller_does_not_complete_queued_transfer(self):
        model, uc = self.make()
        self.prime(uc, 5, 0x80001000, 60)
        self.write_guest(uc, BASE + USBCMD, 0)
        self.write_guest(uc, BASE + ENDPTPRIME, ENDPOINT_BITS[5])
        model.service(0)
        model.service(100_000)
        self.assertEqual(model.completed, 0)
        self.write_guest(uc, BASE + USBCMD, 1)
        model.service(100_001)
        model.service(200_000)
        self.assertEqual(model.completed, 1)


if __name__ == '__main__':
    unittest.main()
