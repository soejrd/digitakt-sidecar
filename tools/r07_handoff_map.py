#!/usr/bin/env python3
"""Hash-pinned OS 1.54 static USB receive anchors; never infer topology from absent literals.

Use --sections with already extracted, private section_3_MAIN_OS.bin and
section_8_SECTION.bin. Requires Capstone and pinned deps/digiemu for the
ColdFire-aware decoder. Does not extract, download or distribute firmware.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

BASE = 0x40000400
MAIN_SHA = '5c58bf9e3949ef09977c5fc007a61e8d026931f67f1621238379dfb8ee4d31a2'
S8_SHA = '6943b2f27773f40f1e7857ea6fa7a121269e930bd403098e6b6c00002327dc20'
SYX_SHA = 'f78ba80fa7b1da5fb0e1ff61ad61e9e71aafe79f4364fc49679f3651353e3cf6'
ROOT = Path(__file__).resolve().parents[1]


def load_main(sections):
    image = (Path(sections) / 'section_3_MAIN_OS.bin').read_bytes()
    if hashlib.sha256(image).hexdigest() != MAIN_SHA:
        raise ValueError('MAIN image is not the pinned OS 1.54')
    return image


def facts(sections, syx=None):
    sys.path.insert(0, str(ROOT / 'deps/digiemu'))
    from dt2.coldfire import disasm
    image = load_main(sections)
    controller = (Path(sections) / 'section_8_SECTION.bin').read_bytes()
    if hashlib.sha256(controller).hexdigest() != S8_SHA:
        raise ValueError('section 8 hash mismatch')
    if syx and hashlib.sha256(Path(syx).read_bytes()).hexdigest() != SYX_SHA:
        raise ValueError('container hash mismatch')
    # Small, known-boundary windows. No linear sweep through embedded tables.
    windows = {
        'boot_usb_dispatch': (0x400692A0, 0x40069324),
        'usb_mode_store': (0x40006E0C, 0x40006E46),
        'out_descriptor_and_submit': (0x4000315C, 0x40003230),
        'receive_copy': (0x400036EE, 0x40003704),
        'receive_release_and_underflow': (0x400037F8, 0x40003850),
        'render_receive_call': (0x40077546, 0x40077598),
        'main_gain_and_usb_half': (0x40072194, 0x400721F8),
        'half_toggle': (0x40078350, 0x40078358),
        'usb_prime': (0x40005E04, 0x40005E22),
    }
    anchors = {}
    for name, (start, end) in windows.items():
        anchors[name] = [
            f'0x{a:08X}: {mn} {ops}'.strip()
            for a, _, mn, ops in disasm(image, BASE, start, end)
        ]
    # Independently audited immediate/absolute stores; retain the assignment
    # and write sites so the SSI1 D1 value is reproducible, not guessed.
    clocks = []
    for module, name, assign, write, expected in [
        (17, 'eDMA', 0x40000500, 0x40000500, '10bc0011'),
        (47, 'SSI0', 0x40000F78, 0x40000F80, '13c1fc04002d'),
        (50, 'SSI1', 0x40000F90, 0x40000FA0, '13c1fc04002d'),
        (23, 'DSPI0', 0x40068E54, 0x40068E56, '13c0fc04002d'),
        (15, 'DSPI1', 0x40068F2A, 0x40068F2C, '13c0fc04002d'),
        (44, 'USB OTG', 0x40006E74, 0x40006E76, '13c1fc04002d'),
    ]:
        assert image[write-BASE:write-BASE+len(expected)//2].hex() == expected
        clocks.append({'module': module, 'name': name,
                       'assignment': f'0x{assign:08X}', 'write': f'0x{write:08X}'})
    return {
        'firmware': 'Digitakt mk1 OS 1.54', 'hardware_test': False,
        'main_sha256': MAIN_SHA, 'section8_sha256': S8_SHA,
        'syx_sha256': SYX_SHA if syx else None,
        'native_usb': {
            'register_base': '0xFC0B0000', 'out_endpoint': '0x03',
            'submit': '0x40005CD4', 'out_internal_index': 3,
            'descriptor_pool': '0x80008800', 'descriptor_count': 16,
            'descriptor_stride': 64, 'payload_pool': '0x8000BA00',
            'packet_bytes': 88, 'header_bytes': 32, 'max_stereo_frames': 7,
            'consumer': '0x400035F4', 'pcm_halves': ['0x80001F60', '0x80002060'],
            'frames_per_half': 32, 'frame_bytes': 8,
            'half_selector': '0x4199F134', 'consumer_packet': '0x4025C264',
            'packet_frame_cursor': '0x4025C018',
            'main_mix': '0x400721F8..0x4007221E',
            'endpoint_queue_offsets': [f'0x{v:X}' for v in struct.unpack_from(
                '>8I', image, 0x4017CEBC-BASE)],
        },
        'clock_enable_requests': clocks, 'anchors': anchors,
        'limitations': [
            'Boot USB dispatch traced statically; not executed on the physical unit.',
            'Section 8 usage/hardware-variant selection is not established.',
            'No clock rate, transport latency or live USB synchronization measured.',
            'Absent address literals do not prove absence of shared memory or DMA.',
        ],
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sections', required=True)
    ap.add_argument('--syx')
    ap.add_argument('--json')
    args = ap.parse_args()
    result = facts(args.sections, args.syx)
    text = json.dumps(result, indent=2) + '\n'
    if args.json:
        Path(args.json).parent.mkdir(parents=True, exist_ok=True)
        Path(args.json).write_text(text)
    else:
        print(text, end='')


if __name__ == '__main__':
    main()
