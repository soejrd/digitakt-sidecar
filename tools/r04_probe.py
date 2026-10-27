#!/usr/bin/env python3
"""Verify Digitakt OS 1.54 control-event ordering statically.

Reads only an already extracted, private MAIN image. The report records
hashes, call sites and semantic ordering, never firmware bytes. Dynamic
Unicorn execution is intentionally not used: in this environment it aborts
with SIGILL at JIT memory mapping. The release condition for dynamic evidence
is recorded in docs/R04-control-ordering.md.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'deps/digiemu'))

MAIN_SHA = '5c58bf9e3949ef09977c5fc007a61e8d026931f67f1621238379dfb8ee4d31a2'
BASE = 0x40000400

ANCHORS = {
    'seq_audio_fire': 0x4006F4BE,
    'seq_lock_resolve': 0x4006F46E,
    'lock_apply': 0x40074B0A,
    'lock_restore': 0x40074A40,
    'engine_sound': 0x40077282,
    'engine_block': 0x40074B9E,
    'lfo': 0x400ED766,
    'voice_process': 0x40075184,
    'post_voice': 0x400757FE,
    'render_source_hook': 0x40077FBA,
    'set_param': 0x400771E8,
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--main', type=Path, required=True,
                        help='private section_3_MAIN_OS.bin from the pinned 1.54 fixture')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    image = args.main.read_bytes()
    digest = hashlib.sha256(image).hexdigest()
    if digest != MAIN_SHA:
        raise SystemExit('MAIN image is not the pinned OS 1.54')

    from dt2.coldfire import disasm
    instructions = [(address, mnemonic, operands)
                    for address, _, mnemonic, operands in disasm(image, BASE, BASE, BASE + len(image))]
    targets = {f'{address:x}': name for name, address in ANCHORS.items()}
    calls = {name: [] for name in ANCHORS}
    for address, mnemonic, operands in instructions:
        if mnemonic not in ('jsr', 'jmp', 'bsr', 'bra'):
            continue
        for encoded, name in targets.items():
            if f'${encoded}' in operands.lower():
                calls[name].append({'from': f'0x{address:08X}', 'op': mnemonic})

    render = [{'op': entry['op'], 'target': name, 'from': entry['from']}
              for name, entries in calls.items() for entry in entries
              if 0x40077AC0 <= int(entry['from'], 16) < 0x40077FC8]
    render.sort(key=lambda entry: entry['from'])
    report = {
        'firmware': 'Digitakt mk1 OS 1.54',
        'main_sha256': digest,
        'hardware_test': False,
        'method': 'hash-pinned static disassembly of private MAIN; no execution',
        'anchors': {name: f'0x{address:08X}' for name, address in ANCHORS.items()},
        'callers': calls,
        'render_window': {
            'range': ['0x40077AC0', '0x40077FC8'],
            'calls_in_address_order': render,
        },
        'source_contract': {
            'seq_fire': 'stored step word, condition, note, velocity and stored p-lock list',
            'engine': 'sound copy, p-lock apply/restore, then parameters and LFOs',
            'render': 'fw_voice_start bit, final 8.8 A-H targets, note and velocity',
        },
        'unresolved': [
            'Retrigger is not evidenced as a distinct sequencer event in the pinned sources.',
            'The render consumes final values; no dynamic timestamp or hardware order is claimed.',
            'Lock lifetime is bounded by engine apply/restore sites, but not traced per voice on hardware.',
        ],
        'release_condition': (
            'Run the dynamic probe in an execution environment where the pinned patched Unicorn '
            'can mem_map without SIGILL; retain the same private fixture and report stage '
            'timestamps, track IDs and lock lifetime.'
        ),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
