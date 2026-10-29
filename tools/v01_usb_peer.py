"""Portable protocol peer behind scheduled native USB transactions.

Host protocol processing is instantaneous in emulated device time. Replies
arrive on the next assumed host poll through native OUT DMA/IRQ/job dispatch;
no receive callback is context-injected. This is not Circle scheduling/audio.
"""
import ctypes
import os
from pathlib import Path
import subprocess
import tempfile

from v01_usb_device_model import u32

ROOT = Path(__file__).resolve().parents[1]
CIN_LENGTH = (0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1)


def build():
    output = ROOT / 'build/v01/emulator_peer.dylib'
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=output.parent, suffix='.dylib', delete=False) as f:
        temporary = Path(f.name)
    try:
        subprocess.run([os.environ.get('CC', 'cc'), '-dynamiclib', '-O2', '-std=c99',
                        '-Icontrol', 'tests/i03_emulator_peer.c', 'control/link.c',
                        'control/host_link.c', 'control/circle_link_adapter.c',
                        '-o', str(temporary)], cwd=ROOT, check=True)
        os.replace(temporary, output)  # concurrent probes never dlopen a partial file
    finally:
        temporary.unlink(missing_ok=True)
    return output


class UsbPeer:
    def __init__(self, session, model, clock_address, library):
        self.s, self.model, self.clock_address = session, model, clock_address
        self.peer = ctypes.CDLL(str(library))
        self.peer.peer_frame.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint32]
        self.peer.peer_reply.argtypes = [ctypes.c_void_p]
        self.peer.peer_init()
        self.pending = bytearray()
        self.frames = self.replies = self.tx_bytes = 0
        self.snapshots = self.age_max = 0
        self.sessions = set()
        self.last_block = None
        self.reply = ctypes.create_string_buffer(64)
        model.tx_sink = self.transmitted

    def transmitted(self, endpoint, payload, done):
        if endpoint != 5:
            return
        if len(payload) % 4:
            raise RuntimeError('native MIDI transfer is not packet aligned')
        self.tx_bytes += len(payload)
        block = u32(self.s.m.uc, self.clock_address)
        for i in range(0, len(payload), 4):
            header = payload[i]
            if header >> 4:
                raise RuntimeError('unexpected MIDI cable')
            for byte in payload[i + 1:i + 1 + CIN_LENGTH[header & 15]]:
                if byte == 0xf0:
                    self.pending.clear()
                self.pending.append(byte)
                if byte == 0xf7:
                    frame = bytes(self.pending)
                    self.pending.clear()
                    self.peer.peer_frame(frame, len(frame), block)
                    self.frames += 1
                    session = self.peer.peer_session()
                    if session:
                        self.sessions.add(session)
                    if len(frame) in (45, 61) and frame[6] in (1, 3):
                        source_block = sum(v << (7 * k) for k, v in enumerate(frame[17:22]))
                        self.age_max = max(self.age_max, (block - source_block) & 0xffffffff)
                        self.snapshots += 1
        self._replies(done)

    def _replies(self, done):
        while (n := self.peer.peer_reply(self.reply)):
            raw = self.reply.raw[:n]
            packets = bytearray()
            for start in range(0, n, 3):
                part = raw[start:start + 3]
                cin = 4 if start + 3 < n else 4 + len(part)
                packets.extend(bytes([cin]) + part.ljust(3, b'\0'))
            self.model.queue_rx(1, packets, done + self.model.poll_cycles)
            self.replies += 1

    def step(self, done, remaining=None):
        return None

    def service(self, done):
        block = u32(self.s.m.uc, self.clock_address)
        if block != self.last_block:
            self.peer.peer_advance(block)
            self.last_block = block
            self._replies(done)
        return False

    def report(self):
        return {'frames_received': self.frames, 'replies_scheduled': self.replies,
                'tx_bytes': self.tx_bytes, 'installs': self.peer.peer_installs(),
                'starts': self.peer.peer_starts(), 'allowed': self.peer.peer_allowed(),
                'session': self.peer.peer_session(), 'sessions_seen': len(self.sessions),
                'snapshots_received': self.snapshots,
                'source_to_host_age_blocks_max': self.age_max}
