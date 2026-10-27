#!/usr/bin/env python3
"""Hash-pinned OS 1.54 USB-MIDI transport anchors for the I03 adapter.

Checks, from the stock MAIN image only, the instruction facts the adapter and
its host model depend on. No firmware output, no execution, no hardware.
Requires Capstone and the pinned deps/digiemu decoder (PYTHONPATH).
"""
import argparse
import json
from pathlib import Path

from r07_handoff_map import BASE, MAIN_SHA, load_main

# (address, expected mnemonic, substring that must appear in the operands)
EXPECTED = [
    # sender 0x40004990: readiness gate and both waits
    (0x400049A2, "cmp.l", "$40610474"),
    (0x400049AC, "move.l", "$fc0b01c4"),
    (0x400049B6, "moveq", "#$2f"),
    (0x400049D0, "movea.l", "$40610478"),
    (0x400049D8, "bge.b", "$400049d0"),
    (0x400049E0, "movea.l", "$4061047c"),
    (0x400049E6, "adda.l", "#$4ba84000"),
    (0x400049FE, "move.l", "$40610478"),
    (0x40004A06, "cmpi.l", "#$ffb"),
    (0x40004A0C, "bgt.b", "$400049fe"),
    # the completion callback that is the only drain of 0x40610478
    (0x40004404, "move.l", "$4399e9fc"),
    (0x40004416, "sub.l", "$40610478"),
    # receiver 0x40004760: per-packet copy with the CIN table
    (0x400047A8, "moveq", "#$f"),
    (0x400047AA, "and.l", "d4"),
    (0x400047AC, "move.l", "$2c(a7, d0.l)"),
    (0x400047CC, "movea.l", "$18(a7, d1.l)"),
    (0x400047D8, "move.b", "(a0)+, (a1)"),
    # callback slots and their invocation
    (0x40004808, "jsr", "(a0)"),
    (0x4000481E, "jsr", "(a0)"),
    (0x400045AA, "move.l", "$4061046c"),
    (0x400045B4, "move.l", "$40610470"),
    # ev_tick, where the adapter is dispatched (core 4.1 owns the site)
    (0x4000A770, "jsr", "$400ca574"),
]
TABLE_AT = 0x4017CE70
TABLE = [0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--sections", required=True)
    ap.add_argument("--output", type=Path, required=True)
    a = ap.parse_args()
    from dt2.coldfire import disasm

    image = load_main(a.sections)
    rows = {}
    for lo, hi in ((0x40004400, 0x40004520), (0x400045A6, 0x40004832),
                   (0x40004990, 0x40004A20), (0x4000A770, 0x4000A776)):
        for pc, _, mn, ops in disasm(image, BASE, lo, hi):
            rows[pc] = (mn, ops)
    verified = {}
    for pc, mn, want in EXPECTED:
        actual = rows.get(pc)
        assert actual and actual[0] == mn and want in actual[1], (hex(pc), actual)
        verified[hex(pc)] = f"{mn} {actual[1]}"
    import struct
    table = list(struct.unpack_from(">16I", image, TABLE_AT - BASE))
    assert table == TABLE, table
    report = {
        "firmware": "Digitakt mk1 OS 1.54",
        "hardware_test": False,
        "level": "static instruction anchors only; no execution",
        "main_sha256": MAIN_SHA,
        "anchors": verified,
        "cin_copy_table": {"address": hex(TABLE_AT), "longs": table},
        "facts": {
            "sender": "0x40004990",
            "sender_ready_flag": "0x40610474 must be 1",
            "sender_isr_flag": "0xFC0B01C4 must be nonzero",
            "staging_ring": "0x4BA84000 + index at 0x4061047C",
            "queued_counter": "0x40610478, lowered only by the completion callback 0x40004404",
            "wrap_wait": "index > 0xFBF for messages > 0x2F bytes; waits for queued < index - 0x3F",
            "capacity_wait": "queued >= 0xFFB",
            "receive_callback_cable0": "0x4061046C",
            "receive_callback_cable1": "0x40610470",
            "receive_call": "0x40004808 (handler(bytes, count))",
            "ui_tick_site": "0x4000A770, core 4.1",
        },
        "limitations": [
            "Instruction presence is not execution; no DMA, ISR ordering or "
            "physical completion timing is measured.",
            "The real stock receive handlers at 0x400D3298/0x400D34CC were not "
            "disassembled here; chaining is exercised only against a host "
            "stand-in that forwards bytes.",
            "USB-MIDI CIN 0x4 groups carry three data bytes with no bit-7 "
            "test in the receiver, so senders pad the last group with zeroes; "
            "the adapter drops at most two zeroes after F7.",
        ],
    }
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(report, indent=2) + "\n")
    print("I03 transport anchors: PASS")


if __name__ == "__main__":
    main()
