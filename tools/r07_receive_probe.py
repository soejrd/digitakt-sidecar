#!/usr/bin/env python3
"""Execute pinned OS 1.54 receive/mix functions with synthetic PCM, without USB hardware.

Requires the R03 patched Unicorn selected on PYTHONPATH; may require execution
outside the restrictive sandbox. USB DMA completion/queueing and libc copy/zero
are test doubles. This is a function-level probe, not a full boot or USB model.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from r07_handoff_map import BASE, MAIN_SHA, load_main


def put(u, address, value):
    u.mem_write(address, struct.pack('>I', value & 0xFFFFFFFF))


def get(u, address):
    return struct.unpack('>I', u.mem_read(address, 4))[0]


def run(sections):
    from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
    from unicorn.m68k_const import (UC_M68K_REG_A3, UC_M68K_REG_A6,
        UC_M68K_REG_A7, UC_M68K_REG_D0, UC_M68K_REG_D2,
        UC_M68K_REG_D3, UC_M68K_REG_D4, UC_M68K_REG_PC, UC_M68K_REG_SR)
    image = load_main(sections)

    def create():
        u = Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN)
        for address, size in [(0x40000000, 0x400000), (0x43990000, 0x10000),
                              (0x4C000000, 0x10000), (0x80000000, 0x20000)]:
            u.mem_map(address, size)
        u.mem_write(BASE, image)
        u.reg_write(UC_M68K_REG_SR, 0x2700)
        return u

    receive = []
    for packets, calls in [(0, 1), (2, 1), (5, 1), (10, 2)]:
        u = create()
        sp, out, stop = 0x4C008000, 0x80001F60, 0x40000300
        put(u, 0x4025C294, 1)  # Already synchronized/streaming: skip startup alignment.
        expected = b''
        for i in range(packets):
            desc, payload = 0x80008800 + i*64, 0x8000BA00 + i*88
            u.mem_write(payload, b'\x07\xff')  # Seven frames, no metadata event.
            put(u, desc + 0x30, payload)
            put(u, desc + 0x34, desc + 64 if i+1 < packets else 0)
            for j in range(7):
                k = i*7+j+1
                sample = struct.pack('>ii', k*123456, -k*98765)
                u.mem_write(payload + 32 + j*8, sample)
                expected += sample
        if packets:
            put(u, 0x4025C264, 0x80008800)
        requeues = []

        def hook(cpu, address, size, data):
            if address == stop:
                cpu.emu_stop()
                return
            if address not in (0x400E8514, 0x400E8AE8, 0x4000315C):
                return
            stack = cpu.reg_read(UC_M68K_REG_A7)
            if address == 0x400E8514:
                cpu.mem_write(get(cpu, stack+4), bytes(get(cpu, stack+8)))
            elif address == 0x400E8AE8:
                cpu.mem_write(get(cpu, stack+4), bytes(cpu.mem_read(
                    get(cpu, stack+8), get(cpu, stack+12))))
            else:
                requeues.append(get(cpu, stack+4))
            cpu.reg_write(UC_M68K_REG_PC, get(cpu, stack))
            cpu.reg_write(UC_M68K_REG_A7, stack+4)

        u.hook_add(UC_HOOK_CODE, hook)
        for call in range(calls):
            u.mem_write(out-4, b'\x5a'*264)
            put(u, sp, stop)
            for i, value in enumerate([out, 0, 0, 32]):
                put(u, sp+4+4*i, value)
            u.reg_write(UC_M68K_REG_A7, sp)
            u.emu_start(0x400035F4, stop, count=100000)
            assert u.reg_read(UC_M68K_REG_PC) == stop, 'receive did not return'
            want = expected[call*256:(call+1)*256].ljust(256, b'\0')
            assert bytes(u.mem_read(out, 256)) == want, 'PCM/underflow mismatch'
            assert bytes(u.mem_read(out-4, 4)) == b'\x5a'*4
            assert bytes(u.mem_read(out+256, 4)) == b'\x5a'*4
        cursor = get(u, 0x4025C018)
        assert cursor == ((32*calls) % 7 if packets*7 >= calls*32 else 0)
        assert len(requeues) == min(packets, calls*32//7)
        assert all(value == 0 for value in requeues)
        receive.append({'packets': packets, 'calls': calls, 'pcm_exact': True,
                        'bounds_intact': True, 'packet_cursor': cursor,
                        'requeues': len(requeues)})

    def mix(clear_usb, internal):
        u = create()
        frame, output, usb = 0x4C008000, 0x80001800, 0x80001F60
        source = struct.pack('>ii', 0x1000000, -0x800000)*32
        u.mem_write(usb, source)
        # Proposed I02 operation on the half ABOUT TO BE CONSUMED: preserve
        # left Q31 words in track 0, then erase both dry stereo channels.
        if clear_usb:
            u.mem_write(0x80001A18, b''.join(source[i:i+4] for i in range(0, 256, 8)))
            u.mem_write(usb, bytes(256))
        if internal:
            u.mem_write(0x8000EA70, struct.pack('>ii', 0x2000000, -0x1000000)*32)
        # INT TO MAIN=2 (enabled), or 0 (disabled). Exercise the stock branch.
        u.mem_write(0x80001A0D, bytes([2 if internal else 0]))
        put(u, frame+8, output)
        for reg, value in [(UC_M68K_REG_A6, frame), (UC_M68K_REG_A7, frame-0x200),
                           (UC_M68K_REG_A3, 0x8000E000), (UC_M68K_REG_D3, 0),
                           (UC_M68K_REG_D4, 16), (UC_M68K_REG_D2, 16),
                           (UC_M68K_REG_D0, 0)]:
            u.reg_write(reg, value)
        # The render ISR's fractional EMAC mode, then jump to the stock gain
        # setup and mix. Private synthetic harness instructions, not a mod.
        u.mem_write(0x40000300, bytes.fromhex('a93c000000204ef9400721b4'))
        u.emu_start(0x40000300, 0x40072220, count=10000)
        assert u.reg_read(UC_M68K_REG_PC) == 0x40072220
        pcm = struct.unpack('>64i', u.mem_read(output, 256))
        if clear_usb:
            assert bytes(u.mem_read(0x80001A18, 128)) == b''.join(
                source[i:i+4] for i in range(0, 256, 8))
        return pcm

    dry, silent = mix(False, False), mix(True, False)
    combined, retained_internal = mix(False, True), mix(True, True)
    assert dry == (-65536, 32768)*32
    assert silent == (0,)*64
    assert retained_internal == (-131072, 65536)*32
    assert combined == tuple(a+b for a, b in zip(dry, retained_internal))
    return {'firmware': 'Digitakt mk1 OS 1.54', 'main_sha256': MAIN_SHA,
            'pass': True, 'hardware_test': False, 'full_boot': False,
            'receive': receive,
            'mix': {'dry_first_pair': list(dry[:2]), 'cleared_peak': max(map(abs, silent)),
                    'internal_first_pair_after_clear': list(retained_internal[:2]),
                    'internal_preserved': True, 'copied_track_source_preserved': True},
            'limits': ['USB controller/DMA completions and libc are test doubles.',
                       'Already synchronized descriptor queues; startup/reconnect not exercised.',
                       'No live USB, hardware path selection, timing or full track pipeline test.',
                       'Gain setup uses controlled state; UI dB mapping not qualified.']}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sections', required=True)
    ap.add_argument('--json')
    args = ap.parse_args()
    result = run(args.sections)
    text = json.dumps(result, indent=2) + '\n'
    if args.json:
        Path(args.json).parent.mkdir(parents=True, exist_ok=True)
        Path(args.json).write_text(text)
    print(text, end='')


if __name__ == '__main__':
    main()
