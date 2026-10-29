#!/usr/bin/env python3
"""Execute the actual compiled two-lane helper in ColdFire Unicorn.

This is a function-level probe with synthetic buffers, not a full firmware boot,
USB DMA model, installed render hook or hardware performance measurement.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(output):
    from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
    from unicorn.m68k_const import (UC_M68K_REG_A7, UC_M68K_REG_D0,
                                    UC_M68K_REG_PC, UC_M68K_REG_SR)
    build = ROOT / 'build/x01'
    build.mkdir(parents=True, exist_ok=True)
    elf, binary = build / 'return-lanes.elf', build / 'return-lanes.bin'
    subprocess.run(['m68k-elf-gcc', '-mcpu=54455', '-O2', '-Wall', '-Wextra',
                    '-Werror', '-ffreestanding', '-fno-builtin', '-nostdlib',
                    '-fno-pic', '-fno-pie', '-Wl,-Ttext=0x100000',
                    '-Wl,--entry=sc_return_lanes', str(ROOT / 'mods/usb-track/return_lanes.c'),
                    '-o', str(elf)], check=True)
    undefined = subprocess.check_output(['m68k-elf-nm', '-u', str(elf)], text=True)
    assert not undefined.strip(), undefined
    subprocess.run(['m68k-elf-objcopy', '-O', 'binary', '--only-section=.text',
                    str(elf), str(binary)], check=True)
    code = binary.read_bytes()
    results = []
    cases = [(0, 1, (0, 3)), (1, 1, (7, 2)), (0, 0, (0, 3)),
             (1, 0, (0, 3)), (2, 1, (0, 3)), (0, 1, (-1, 3)),
             (0, 1, (0, 0)), (0, 1, (0, 8)), (0, 1, (-2, 3))]
    for selector, ready, mapping in cases:
        cpu = Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN)
        cpu.mem_map(0x100000, 0x10000)
        cpu.mem_map(0x200000, 0x10000)
        cpu.mem_map(0x300000, 0x10000)
        cpu.mem_write(0x100000, code)
        tracks, halves, map_addr, sp, stop = 0x200100, 0x201000, 0x202000, 0x30ff00, 0x10fff0
        original = [1234567 + i for i in range(256)]
        pcm = [(i - 64) * 654321 for i in range(128)]
        cpu.mem_write(tracks-4, b'\xa5'*1032)
        cpu.mem_write(halves-4, b'\x5a'*520)
        cpu.mem_write(tracks, struct.pack('>256i', *original))
        cpu.mem_write(halves, struct.pack('>128i', *pcm))
        cpu.mem_write(map_addr, struct.pack('bb', *mapping))
        cpu.mem_write(sp, struct.pack('>6I', stop, tracks, halves, selector, ready, map_addr))
        cpu.reg_write(UC_M68K_REG_SR, 0x2700)
        cpu.reg_write(UC_M68K_REG_A7, sp)
        instructions = [0]

        def count(*_):
            instructions[0] += 1

        cpu.hook_add(UC_HOOK_CODE, count)
        cpu.emu_start(0x100000, stop, count=10000)
        assert cpu.reg_read(UC_M68K_REG_PC) == stop, 'helper did not return'
        valid = all(-1 <= t <= 7 for t in mapping) and not (
            mapping[0] >= 0 and mapping[0] == mapping[1])
        active = bool(valid and ready and selector < 2)
        want_tracks, want_pcm = original[:], pcm[:]
        if valid:
            for lane, track in enumerate(mapping):
                if track >= 0:
                    want_tracks[track*32:(track+1)*32] = [
                        pcm[selector*64+i*2+lane] if active else 0 for i in range(32)]
        if active:
            want_pcm[selector*64:(selector+1)*64] = [0]*64
        else:
            want_pcm = [0]*128
        assert cpu.reg_read(UC_M68K_REG_D0) == int(active)
        assert bytes(cpu.mem_read(tracks, 1024)) == struct.pack('>256i', *want_tracks)
        assert bytes(cpu.mem_read(halves, 512)) == struct.pack('>128i', *want_pcm)
        for addr, value in ((tracks-4, b'\xa5'*4), (tracks+1024, b'\xa5'*4),
                            (halves-4, b'\x5a'*4), (halves+512, b'\x5a'*4)):
            assert bytes(cpu.mem_read(addr, 4)) == value
        results.append({'selector': selector, 'ready': ready, 'lane_tracks': mapping,
                        'active': active, 'exact_pcm': True, 'bounds_intact': True,
                        'compiled_instructions': instructions[0]})
    report = {'scope': 'actual compiled ColdFire helper; synthetic buffers; no firmware boot/DMA/hardware timing',
              'command': 'PYTHONPATH=private/r03-runtime:deps/digiemu deps/digiemu/.venv/bin/python tools/x01_route_probe.py',
              'source_sha256': hashlib.sha256((ROOT / 'mods/usb-track/return_lanes.c').read_bytes()).hexdigest(),
              'code_sha256': hashlib.sha256(code).hexdigest(), 'code_bytes': len(code),
              'undefined_symbols': [], 'cases': results, 'pass': True}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2)+'\n')
    print(f'X01 compiled ColdFire routing: {len(results)} exact PCM/bounds cases: PASS')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'evidence/x01/coldfire-routing.json')
    run(parser.parse_args().output)
