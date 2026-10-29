#!/usr/bin/env python3
"""Bounded native USB device model for V01 Gate A.

The native OS 1.54 driver submits its own descriptor chains to the ChipIdea
controller at 0xFC0B0000. This model does not replace that driver: it owns the
controller registers the driver polls, takes a primed descriptor from the
driver's queue head, completes it at the endpoint's packet cadence, and raises
the native USB status/deferred-interrupt path. The firmware then runs its own
ISR209 -> INTC1 source6 -> ISR134 -> receive callback chain.

Native dTD next pointers and the queue head's hardware next field are distinct
from the software head/tail used by ISR134. The model follows the former; the
native driver advances the latter and owns callbacks and descriptor release.

Register names and the endpoint queue-head table follow the independently
written Octatrack USB SDK in the local elekloader dependency and the native
OS 1.54 accesses in evidence/r07/handoff-facts.json. No physical USB timing is
claimed: one transaction per endpoint per assumed 125us host poll is a declared
component-model policy. OUT descriptors remain active until a payload arrives.
"""
import collections
import struct

from unicorn import UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE

BASE = 0xFC0B0000
USBCMD = 0x140
USBSTS = 0x144
USBINTR = 0x148
FRINDEX = 0x14C
DEVICEADDR = 0x154
ENDPOINTLISTADDR = 0x158
PORTSC1 = 0x184
OTGSC = 0x1A4
ENDPTSETUPSTAT = 0x1AC
ENDPTPRIME = 0x1B0
ENDPTFLUSH = 0x1B4
ENDPTSTAT = 0x1B8
ENDPTCOMPLETE = 0x1BC
ENDPTCTRL = 0x1C0

# The native driver reads 0xFC0B0178 as its port/connection status and tests
# bit 17 at 0x40004520 before setting its own ready flag 0x40610474.
PORT_STATUS = 0x178
PORT_CONNECTED = 1 << 17

# Native endpoint index -> ChipIdea queue-head offset (0x4017CEBC).
QUEUE_OFFSETS = (0x000, 0x080, 0x100, 0x180, 0x040, 0x0C0, 0x140, 0x1C0)
QUEUE_BASE = 0x80000000
ENDPOINT_BITS = (1, 2, 4, 8, 1 << 16, 1 << 17, 1 << 18, 1 << 19)

# Native descriptor fields, read from the submission and completion routines.
NEXT, TOKEN, STATUS, BUFFER, FRAME = 0x00, 0x04, 0x07, 0x08, 0x0C
LENGTH, CALLBACK, TRANSFER_BUFFER = 0x20, 0x24, 0x30
ACTIVE = 0x80

AUDIO_ENDPOINT = 3              # internal index used by 0x40003222


def u32(uc, address):
    return struct.unpack('>I', bytes(uc.mem_read(address, 4)))[0]


def put_u32(uc, address, value):
    uc.mem_write(address, struct.pack('>I', value & 0xFFFFFFFF))


class Transfer:
    __slots__ = ('endpoint', 'descriptor', 'due', 'rx', 'length')

    def __init__(self, endpoint, descriptor, due, rx=None, length=None):
        self.endpoint = endpoint
        self.descriptor = descriptor
        self.due = due
        self.rx = rx
        self.length = length


class UsbDeviceModel:
    """Controller registers and native descriptor-chain ownership."""

    def __init__(self, machine, fsys, rx_bytes=None, tx_sink=None):
        self.m = machine
        self.uc = machine.uc
        self.ips = fsys
        self.rx_bytes = collections.deque((1, 0, bytes(p)) for p in (rx_bytes or ()))
        self.tx_sink = tx_sink
        self.poll_cycles = max(1, fsys // 8000)  # explicit HS host-poll assumption
        self.now = 0
        self.transfers = []
        self.primed = 0
        self.completed = 0
        self.rx_dispatched = 0
        self.primed_bits = 0
        self.completed_bits = 0
        self.status = 0
        self.stat = 0
        self.setupstat = 0
        self.command = u32(self.uc, BASE + USBCMD)
        self.intr = u32(self.uc, BASE + USBINTR)
        self.portsc = 2 << 26                   # high speed
        self.otgsc = 0
        self.endptctrl = 0xC800C8
        self.frames = 0
        self.queued_descriptors = {}
        self.errors = []
        self._pending_prime = 0
        self._pending_complete_clear = 0
        self._pending_status_clear = 0
        self._write_registers()
        uc = self.uc
        uc.hook_add(UC_HOOK_MEM_WRITE, self._on_write,
                    begin=BASE, end=BASE + 0x200 - 1)
        uc.hook_add(UC_HOOK_MEM_READ, self._on_read,
                    begin=BASE, end=BASE + 0x200 - 1)

    # -- controller registers -------------------------------------------------
    def _write_registers(self):
        put_u32(self.uc, BASE + USBCMD, self.command)
        put_u32(self.uc, BASE + USBINTR, self.intr)
        put_u32(self.uc, BASE + USBSTS, self.status)
        put_u32(self.uc, BASE + FRINDEX, self.frames)
        put_u32(self.uc, BASE + PORT_STATUS, PORT_CONNECTED)
        put_u32(self.uc, BASE + PORTSC1, self.portsc)
        put_u32(self.uc, BASE + OTGSC, self.otgsc)
        put_u32(self.uc, BASE + ENDPTSETUPSTAT, self.setupstat)
        put_u32(self.uc, BASE + ENDPTPRIME, self.primed_bits)
        put_u32(self.uc, BASE + ENDPTSTAT, self.stat)
        put_u32(self.uc, BASE + ENDPTCOMPLETE, self.completed_bits)
        put_u32(self.uc, BASE + ENDPTCTRL + 1 * 4, self.endptctrl)

    def _on_write(self, uc, access, address, size, value, data):
        off = address - BASE
        value = int(value) & ((1 << (8 * size)) - 1)
        if off == ENDPTPRIME:
            self._pending_prime |= value
            self.primed_bits |= value
        elif off == ENDPTCOMPLETE:
            self._pending_complete_clear |= value
        elif off == USBSTS:
            self._pending_status_clear |= value
        elif off == USBCMD:
            self.command = value
        elif off == USBINTR:
            self.intr = value
        elif off == ENDPTFLUSH:
            # Flush is acknowledged immediately in this bounded model.
            self.stat &= ~value
            self.primed_bits &= ~value
            cancelled = [t for t in self.transfers if ENDPOINT_BITS[t.endpoint] & value]
            for t in cancelled:
                self.queued_descriptors.pop(t.descriptor, None)
                self.transfers.remove(t)
        elif off == ENDPTCTRL + 1 * 4:
            self.endptctrl = value

    def _on_read(self, uc, access, address, size, value, data):
        # The guest's load happens after this hook. Present the model's state
        # so write-one-to-clear and hardware-owned bits do not leak stale bytes.
        off = address - BASE
        self._clear_acks()
        if off == USBSTS:
            put_u32(uc, address, self.status)
        elif off == ENDPTPRIME:
            put_u32(uc, address, self.primed_bits)
        elif off == ENDPTSTAT:
            put_u32(uc, address, self.stat)
        elif off == ENDPTCOMPLETE:
            put_u32(uc, address, self.completed_bits)
        elif off == ENDPTSETUPSTAT:
            put_u32(uc, address, self.setupstat)
        elif off == ENDPTFLUSH:
            put_u32(uc, address, 0)
        elif off == USBINTR:
            put_u32(uc, address, self.intr)
        elif off == FRINDEX:
            put_u32(uc, address, self.frames)
        elif off == PORT_STATUS:
            put_u32(uc, address, PORT_CONNECTED)
        elif off == PORTSC1:
            put_u32(uc, address, self.portsc)
        elif off == OTGSC:
            put_u32(uc, address, self.otgsc)
        elif off == ENDPTCTRL + 1 * 4:
            put_u32(uc, address, self.endptctrl)

    def _queue_head(self, endpoint):
        base = u32(self.uc, BASE + ENDPOINTLISTADDR) & ~0x7ff
        return base + QUEUE_OFFSETS[endpoint]

    def configure_component(self):
        """Explicit configured-controller precondition; not enumeration."""
        self.command |= 1
        self.intr |= 1
        put_u32(self.uc, BASE + ENDPOINTLISTADDR, QUEUE_BASE)
        self._write_registers()

    def _descriptor_for(self, endpoint):
        qh = self._queue_head(endpoint)
        desc = u32(self.uc, qh + 8)
        if desc == 0 or desc & 1 or desc in self.queued_descriptors:
            return None
        token = u32(self.uc, desc + TOKEN)
        if not token & ACTIVE:
            return None
        return desc

    def _period_cycles(self, endpoint):
        # A capacity of seven audio frames is not a packet cadence. This
        # component models one explicitly assumed host poll per microframe.
        return self.poll_cycles

    def queue_rx(self, endpoint, payload, due):
        if not 0 <= endpoint < 4:
            raise ValueError('host OUT payload requires an OUT endpoint')
        self.rx_bytes.append((endpoint, due, bytes(payload)))

    def adopt_configured_queues(self):
        """Resume active native MIDI dTDs in a disposable configured snapshot.

        This is not enumeration: setup ran the native configuration routine.
        The original checkpoint has no USB controller state to restore.
        """
        for endpoint in (1, 5):
            desc = self._descriptor_for(endpoint)
            if desc is not None:
                self._pending_prime |= ENDPOINT_BITS[endpoint]

    def _take_primed(self, done):
        for endpoint in range(8):
            bit = ENDPOINT_BITS[endpoint]
            if not self._pending_prime & bit:
                continue
            self._pending_prime &= ~bit
            self.primed_bits &= ~bit
            desc = self._descriptor_for(endpoint)
            if desc is None:
                self.errors.append({'event': 'prime-without-descriptor',
                                    'endpoint': endpoint})
                continue
            self.primed += 1
            self.stat |= bit
            self.queued_descriptors[desc] = endpoint
            length = u32(self.uc, desc + LENGTH)
            due = done + self._period_cycles(endpoint)
            self.transfers.append(Transfer(endpoint, desc, due, None, length))

    def _complete(self, transfer):
        uc = self.uc
        desc = transfer.descriptor
        token = u32(uc, desc + TOKEN)
        requested = u32(uc, desc + LENGTH)
        length = transfer.length if transfer.length is not None else requested
        if transfer.rx is not None:
            buffer = u32(uc, desc + TRANSFER_BUFFER) or u32(uc, desc + BUFFER)
            if len(transfer.rx) > 0x10000:
                raise RuntimeError('RX packet too large for bounded model')
            if len(transfer.rx) > requested:
                raise RuntimeError('RX packet exceeds native descriptor capacity')
            self.m.ensure(buffer)
            self.m.ensure(buffer + max(1, len(transfer.rx)) - 1)
            uc.mem_write(buffer, transfer.rx)
            self.rx_dispatched += 1
        # TOKEN+0 carries residual bytes, not bytes transferred. Native
        # ISR134 reads word +4 and calls back with LENGTH minus residual.
        put_u32(uc, desc + TOKEN, ((requested - length) << 16) & 0xFFFF0000)
        uc.mem_write(desc + STATUS, b'\x00')
        put_u32(uc, desc + FRAME, self.frames & 0x7FF)
        self.queued_descriptors.pop(desc, None)
        self.stat &= ~ENDPOINT_BITS[transfer.endpoint]
        self.completed_bits |= ENDPOINT_BITS[transfer.endpoint]
        self.completed += 1
        if token & 0x8000:  # IOC, set by the native submitter for callbacks
            self.status |= 1
        if transfer.endpoint >= 4 and self.tx_sink is not None:
            buffer = u32(uc, desc + TRANSFER_BUFFER) or u32(uc, desc + BUFFER)
            self.tx_sink(transfer.endpoint, bytes(uc.mem_read(buffer, length)), self.now)
        # Follow the native hardware dTD next pointer. Native ISR134 later
        # advances its separate software queue head (+0x30) and calls back.
        nxt = u32(uc, desc + NEXT)
        if not nxt & 1:
            if nxt == desc or nxt in self.queued_descriptors:
                self.errors.append({'event': 'descriptor-cycle', 'endpoint': transfer.endpoint})
            elif u32(uc, nxt + TOKEN) & ACTIVE:
                self.queued_descriptors[nxt] = transfer.endpoint
                self.stat |= ENDPOINT_BITS[transfer.endpoint]
                self.transfers.append(Transfer(transfer.endpoint, nxt,
                                              self.now + self.poll_cycles, None,
                                              u32(uc, nxt + LENGTH)))
        qh = self._queue_head(transfer.endpoint)
        put_u32(uc, qh + 4, desc)
        put_u32(uc, qh + 8, nxt)

    # -- emulator time source -------------------------------------------------
    def step(self, done, remaining=None):
        if not self.command & 1:
            return None
        if self._pending_prime:
            return 1
        if self.transfers:
            return max(1, min(t.due for t in self.transfers) - done)
        return None

    def _clear_acks(self):
        changed = bool(self._pending_complete_clear or self._pending_status_clear)
        self.completed_bits &= ~self._pending_complete_clear
        self._pending_complete_clear = 0
        self.status &= ~self._pending_status_clear
        self._pending_status_clear = 0
        return changed

    def service(self, done):
        self.now = done
        self.frames = (done * 8000 // self.ips) & 0x3fff
        changed = self._clear_acks()
        if not self.command & 1:
            return changed
        if self._pending_prime:
            self._take_primed(done)
            changed = True
        due = [t for t in self.transfers if t.due <= done]
        for transfer in due:
            if transfer.endpoint < 4:
                packet = next((p for p in self.rx_bytes if p[0] == transfer.endpoint
                               and p[1] <= done), None)
                if packet is None:
                    transfer.due = done + self.poll_cycles
                    continue  # no host packet: never invent an OUT completion
                self.rx_bytes.remove(packet)
                transfer.rx = packet[2]
                transfer.length = len(packet[2])
            self.completed_bits &= ~ENDPOINT_BITS[transfer.endpoint]  # hardware sets below
            self._complete(transfer)
            self.transfers.remove(transfer)
            changed = True
        if changed:
            self._write_registers()
        return changed

    def report(self):
        return {
            'schema': 'v01-usb-device-model/2',
            'primed': self.primed,
            'completed': self.completed,
            'rx_dispatched': self.rx_dispatched,
            'pending_transfers': len(self.transfers),
            'queued_descriptors': len(self.queued_descriptors),
            'status': self.status,
            'run_stop': bool(self.command & 1),
            'interrupt_enable': self.intr,
            'endptstat': self.stat,
            'endptcomplete': self.completed_bits,
            'prime_register': self.primed_bits,
            'host_poll_assumption_us': self.poll_cycles * 1e6 / self.ips,
            'errors': list(self.errors),
            'limits': [
                'Native descriptor next pointers are followed; one transaction per endpoint per assumed host poll.',
                'No host-side USB scheduling or bus contention is modelled.',
                'RX bytes are supplied at explicit arrival times, not a live host stream.',
                '125us host polling is an explicit model assumption, not measured device completion latency.',
                'No ACK policy, session persistence or physical USB claim is made.',
            ],
        }


class UsbStatusIrq:
    """Deliver INTC2 vector 209 while the model holds USB status bit 0."""

    def __init__(self, model):
        self.model = model
        self.m = model.m
        self.deliveries = 0
        self.blocked_by_mask = 0
        self.blocked_by_ipl = 0

    def step(self, done, remaining=None):
        from emu.pit import PENDING_STEP
        if self.model.status & 1:
            return min(PENDING_STEP, remaining) if remaining is not None else PENDING_STEP
        return None

    def service(self, done):
        from emu.pit import interrupt_level
        from unicorn.m68k_const import UC_M68K_REG_SR
        if not self.model.status & 1:
            return False
        if not self.model.intr & 1:
            self.blocked_by_mask += 1
            return False
        level = interrupt_level(self.m, 209)
        if level is None:
            self.blocked_by_mask += 1
            return False
        if ((self.m.uc.reg_read(UC_M68K_REG_SR) >> 8) & 7) >= level:
            self.blocked_by_ipl += 1
            return False
        if self.m.raise_vector(209, level=level):
            self.deliveries += 1
            return True
        return False
