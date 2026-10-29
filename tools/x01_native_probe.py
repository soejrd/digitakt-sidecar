#!/usr/bin/env python3
"""Full OS 1.54 render with the compiled candidate helper injected in RAM.

Own private I02 stock fixture required. Synthetic USB PCM, no USB DMA/control
link and no machine selection/lifecycle claim; no firmware file is modified.
"""
import argparse
import array
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys

import i02_probe as i02
import r03_probe as r03
from r07_handoff_map import MAIN_SHA, load_main

ROOT = Path(__file__).resolve().parents[1]
CODE = 0x50000000


def build_gateway():
    folder = ROOT / 'build/x01'
    folder.mkdir(parents=True, exist_ok=True)
    elf, binary = folder/'native-route.elf', folder/'native-route.bin'
    subprocess.run(['m68k-elf-gcc', '-mcpu=54455', '-O2', '-ffreestanding',
                    '-fno-builtin', '-fno-pic', '-fno-pie', '-nostdlib',
                    '-Wl,-Ttext=0x50000000', '-Wl,--entry=x01_gateway',
                    str(ROOT/'tools/x01_route_gateway.s'),
                    str(ROOT/'mods/usb-track/return_lanes.c'), '-o', str(elf)], check=True)
    assert not subprocess.check_output(['m68k-elf-nm', '-u', str(elf)], text=True).strip()
    symbols = subprocess.check_output(['m68k-elf-nm', str(elf)], text=True)
    assert any(line.startswith('50000000 ') and line.endswith(' x01_gateway')
               for line in symbols.splitlines())
    subprocess.run(['m68k-elf-objcopy', '-O', 'binary', str(elf), str(binary)], check=True)
    return binary.read_bytes()


def amplitudes(pcm):
    values = array.array('h', pcm)
    if sys.byteorder != 'little':
        values.byteswap()
    mono = [(values[i]+values[i+1])/2 for i in range(0, len(values), 2)]
    result = {}
    for hz in (750, 3000):
        real = sum(v*math.cos(2*math.pi*hz*i/48000) for i, v in enumerate(mono))
        imag = sum(v*math.sin(2*math.pi*hz*i/48000) for i, v in enumerate(mono))
        result[str(hz)] = 2*math.hypot(real, imag)/max(1, len(mono))
    return result


def capture(fixture, code, name, muted=None, closed=None):
    from emu.session import Session
    from unicorn import UC_HOOK_CODE
    expected = None
    phase = 0
    checks = {'exact_blocks': 0, 'halves': [0, 0], 'dry_suppression': 0}

    def on_machine(session):
        cpu = session.m.uc
        assert bytes(cpu.mem_read(i02.PRE, 6)) == bytes.fromhex('49f94199f444')
        cpu.mem_map(CODE, (len(code)+4095)//4096*4096)
        cpu.mem_write(CODE, code)
        cpu.mem_write(i02.PRE, bytes.fromhex('4eb9')+struct.pack('>I', CODE))

        def hook(uc, address, size, data):
            nonlocal phase, expected
            selector = struct.unpack('>I', uc.mem_read(i02.SELECTOR, 4))[0]
            assert selector <= 1
            if address == i02.PRE:
                # Model already received PCM at the verified current-half boundary.
                lanes = [[round(math.sin(2*math.pi*hz*(phase+i)/48000)*0.05*(2**31-1))
                          for i in range(32)] for hz in (750, 3000)]
                phase += 32
                pair = b''.join(struct.pack('>ii', lanes[0][i], lanes[1][i]) for i in range(32))
                uc.mem_write(i02.USB+256*selector, pair)
                expected = (selector, lanes, bytes(uc.mem_read(i02.TRACK+256, 768)),
                            bytes(uc.mem_read(i02.USB+256*(selector^1), 256)))
            elif address == i02.PRE+6:
                assert expected is not None
                selected, lanes, others, other_half = expected
                assert selected == selector
                for t in range(2):
                    assert bytes(uc.mem_read(i02.TRACK+128*t, 128)) == struct.pack('>32i', *lanes[t])
                assert bytes(uc.mem_read(i02.TRACK+256, 768)) == others
                assert bytes(uc.mem_read(i02.USB+256*(selector^1), 256)) == other_half
                assert bytes(uc.mem_read(i02.USB+256*selector, 256)) == bytes(256)
                checks['exact_blocks'] += 1
                checks['halves'][selector] += 1
            elif address == 0x40072194:
                assert bytes(uc.mem_read(i02.USB+256*selector, 256)) == bytes(256)
                checks['dry_suppression'] += 1

        for address in (i02.PRE, i02.PRE+6, 0x40072194):
            cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)

    session = Session(str(fixture/'assigned.snap'), audio=True, on_machine=on_machine)
    try:
        session.run_ms(100)
        if muted is not None:
            r03.select_track(session, muted)
            r03.tap(session, 22)
            r03.rotate(session, 8, -127)
        if closed is not None:
            r03.select_track(session, closed)
            r03.tap(session, 21)
            r03.rotate(session, 5, -127)
        session.press(24)
        session.press(25)
        session.run_ms(50)
        session.release(24)
        session.release(25)
        session.pcm.clear()
        session.run_ms(400)
        assert not session.halted, session.halted
        assert checks['exact_blocks'] > 100 and all(checks['halves'])
        return {'name': name, 'muted_track': muted, 'closed_filter_track': closed,
                'tone_amplitudes': amplitudes(session.pcm), 'checks': checks,
                'halted': session.halted}
    finally:
        session.close()


def run(fixture, output):
    load_main(fixture/'sections')
    i02.configure(fixture)
    r03.ACTIVE_TRACK = 0x4197C6B4  # OS 1.54, as in I02's pinned probe.
    code = build_gateway()
    results = []
    for name, muted, closed in [('both', None, None), ('amp0', 0, None),
                                ('amp1', 1, None), ('filter0', None, 0)]:
        result = capture(fixture, code, name, muted, closed)
        results.append(result)
        print(json.dumps(result), flush=True)
    baseline = results[0]['tone_amplitudes']
    assert baseline['750'] > 1 and baseline['3000'] > 1
    for row, own, other in [(results[1], '750', '3000'), (results[2], '3000', '750')]:
        assert row['tone_amplitudes'][own] < baseline[own]*0.05
        assert row['tone_amplitudes'][other] > baseline[other]*0.5
    assert results[3]['tone_amplitudes']['750'] < baseline['750']*0.1
    assert results[3]['tone_amplitudes']['3000'] > baseline['3000']*0.5
    report = {'scope': 'full stock OS1.54 render; actual compiled helper via RAM-only gateway; synthetic USB PCM; HLE enabled',
              'not_proven': ['USB DMA/transport', 'track-aware firmware capture/worker/handshake',
                             'native multitrack machine selection/persistence', 'hardware timing'],
              'main_sha256': MAIN_SHA, 'gateway_sha256': hashlib.sha256(code).hexdigest(),
              'cases': results, 'pass': True}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2)+'\n')
    print('X01 native render: two compiled return lanes, independent AMP/filter, dry suppression: PASS')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', type=Path, default=ROOT/'private/i02-fixture')
    parser.add_argument('--output', type=Path, default=ROOT/'evidence/x01/native-routing-154.json')
    args = parser.parse_args()
    run(args.fixture, args.output)
