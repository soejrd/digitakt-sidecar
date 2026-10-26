#!/usr/bin/env python3
"""Reproducible, read-only static checks for the Digitakt mk1 OS 1.54 audio path.

This tool does not contain or download firmware. Point it at a lawful local
Digitakt_OS1.54.syx and, optionally, the extracted section_3_MAIN_OS.bin. It
prints the container and MAIN hashes, the audio/transport anchors found in the
1.54 image, and the OS 1.54 routing-menu strings. It is intentionally narrow:
it records what the image says, not what the hardware is claimed to do.

The firmware image and the extracted sections must stay outside this repo.
"""
import argparse
import hashlib
import json
import os
import pathlib
import struct
import sys

ANCHORS = {
    'main_load': 0x40000400,
    'ssi1_receive_dma_dest': 0x80001000,
    'audio_dma_tcd_receive': 0xFC045680,
    'audio_dma_tcd_transmit': 0xFC0456C0,
    'audio_transmit_buffer': 0x4BA9F080,
    'audio_transmit_half': 0x4BA9F180,
    'render_vector_191': 0x40077420,
    'per_track_source_buffers': 0x80001A18,
    'source_stride': 128,
    'routing_int_to_main_string': 0x401C57AA,
    'routing_usb_to_main_string': 0x401C57B6,
    'routing_pre_post_string': 0x401C57C7,
}

STRINGS = {
    'INT TO MAIN': 0x401C57AA,
    'USB TO MAIN [dB]': 0x401C57B6,
    'PRE/POST FADER': 0x401C57C7,
    'PRE-FX (Rytm wording; absence is meaningful)': None,
    'POST-FX (Rytm wording; absence is meaningful)': None,
    'SAMPLER ONLY (Rytm wording; absence is meaningful)': None,
}


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def find_image(syx, explicit):
    if explicit:
        return pathlib.Path(explicit)
    here = pathlib.Path(syx).resolve().parent
    for pattern in ('sections/*MAIN_OS*.bin', '*MAIN_OS*.bin'):
        hits = sorted(here.glob(pattern))
        if len(hits) == 1:
            return hits[0]
    return None


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('syx', help='lawful local Digitakt_OS1.54.syx')
    ap.add_argument('--main-image', help='extracted section_3_MAIN_OS.bin (default: search beside the .syx)')
    ap.add_argument('--json', help='also write these facts as JSON')
    args = ap.parse_args(argv)

    syx = pathlib.Path(args.syx)
    if not syx.is_file():
        raise SystemExit('missing firmware file: %s' % syx)
    image = find_image(syx, args.main_image)
    if image is None or not image.is_file():
        raise SystemExit('missing extracted MAIN OS image; pass --main-image or extract sections first')

    b = image.read_bytes()
    facts = {
        'container_sha256': sha256(syx),
        'main_sha256': sha256(image),
        'main_length': len(b),
        'anchors': dict(ANCHORS),
        'strings': {},
        'embedded_pointer_hits': {},
    }

    for text, va in STRINGS.items():
        needle = text.encode()
        if va is None:
            off = b.find(needle)
            facts['strings'][text] = {'present': off >= 0}
            continue
        off = va - ANCHORS['main_load']
        present = 0 <= off < len(b) and b.startswith(needle, off)
        facts['strings'][text] = {'address': '0x%08X' % va, 'present': present}

    # Absolute BE pointers to the three routing strings, plus the render and
    # receive anchors. A hit is source evidence of a reference; absence is not
    # proof of no reference (PC-relative operands are not searched here).
    for name in ('routing_int_to_main_string', 'routing_usb_to_main_string',
                 'routing_pre_post_string', 'render_vector_191',
                 'ssi1_receive_dma_dest', 'per_track_source_buffers'):
        target = ANCHORS[name]
        pat = struct.pack('>I', target)
        hits, at = [], b.find(pat)
        while at >= 0:
            hits.append('0x%08X' % (ANCHORS['main_load'] + at))
            at = b.find(pat, at + 1)
        facts['embedded_pointer_hits'][name] = hits[:64]

    print(json.dumps(facts, indent=2))
    if args.json:
        with open(args.json, 'w', encoding='utf-8') as f:
            json.dump(facts, f, indent=2)
            f.write('\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
