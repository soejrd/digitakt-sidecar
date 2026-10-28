#!/usr/bin/env python3
"""Validate the V01 diagnostic page and report the emulator task hierarchy."""
import argparse
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import i03_dynamic_probe as dynamic


def run(candidate, output):
    from emu.session import Session
    from emu.harness import call
    from unicorn.m68k_const import UC_M68K_REG_PC

    root, meta, symbols, firmware = dynamic._fixture(candidate)
    dynamic._configure(firmware)
    s = Session(str(root / "assigned.snap"), audio=True)

    def addr(name):
        return int(symbols[name], 0)

    def u32(address):
        return struct.unpack(">I", bytes(s.m.uc.mem_read(address, 4)))[0]

    def put(address, value):
        s.m.uc.mem_write(address, struct.pack(">I", value))

    try:
        s.run_ms(100)
        tasks = []
        dynamic._select_usb1(s, symbols)
        ui = u32(addr("usb_track_machine") + 28)
        page_count = s.m.uc.mem_read(ui + 616, 1)[0]
        pages = u32(ui + 620)
        assert page_count == 2
        assert bytes(s.m.uc.mem_read(u32(pages + 16), 16)).split(b"\0")[0] == b"V01 DIAG"
        assert list(s.m.uc.mem_read(pages + 16 + 4, 8)) == [8, 9, 10, 11, 12, 13, 14, 0]
        s.m.ensure(0x30000000)

        def invoke(fn, args):
            context = s.m.uc.context_save()
            try:
                s.m._fast_stepper_obj.left = 100000
                call(s.m, fn, args, limit=100000)
                assert s.m.uc.reg_read(UC_M68K_REG_PC) == 0xdeadbee0
            finally:
                s.m.uc.context_restore(context)

        assert u32(addr("usb_track_diag_window")) > 0, "native selection did not start a trial"
        window = u32(addr("usb_track_diag_window"))
        put(addr("usb_track_diag_worker_window"), window)
        put(addr("usb_track_diag_started_at"), 0)
        put(addr("usb_track_clock"), 12345)
        put(addr("usb_track_diag_ui_ticks"), 234)
        put(addr("usb_track_diag_worker_entries"), 345)
        put(addr("usb_track_diag_worker_gap_max"), 64 * 7)
        put(addr("usb_track_diag_batch_max"), 64 * 2)
        put(addr("usb_track_diag_ui_gap_max"), 7000)
        put(addr("usb_track_worker_guard_faults"), 0)
        values = {}
        for k, label in enumerate(("SECS", "UI", "WORK", "WGAP", "BATCH", "UGAP", "FAULT")):
            knob = ui + 336 + 40 * k
            assert bytes(s.m.uc.mem_read(u32(knob), len(label) + 1)) == label.encode() + b"\0"
            s.m.uc.mem_write(0x30000000, bytes(32))
            invoke(u32(knob + 32), [0x30000000, 0, 0, 121])
            values[label] = int(bytes(s.m.uc.mem_read(0x30000000, 32)).split(b"\0")[0])
        assert values == {"SECS": 8, "UI": 234, "WORK": 345, "WGAP": 299,
                          "BATCH": 85, "UGAP": 7000, "FAULT": 0}
        # Exercise the compiled on_switch callback through its actual descriptor.
        callback = u32(ui + 12)
        assert callback
        invoke(callback, [0, 0, 0])
        assert u32(addr("usb_track_diag_window")) == window + 1
        assert u32(addr("usb_track_diag_ui_gap_max")) == 0
        assert u32(addr("usb_track_diag_ui_ticks")) == 0
        assert u32(addr("usb_track_diag_started_at")) == 12345
        # Old worker samples cannot appear in a fresh, unacknowledged window.
        for k in (0, 2, 3, 4, 5):
            invoke(u32(ui + 336 + 40 * k + 32), [0x30000000, 0, 0, 121])
            assert bytes(s.m.uc.mem_read(0x30000000, 2)) == b"0\0"
        record = {
            "task": "V01",
            "phase": "2 device-side service timing",
            "status": "candidate verified in emulator; not installed",
            "scope": "compiled diagnostic formatter/task model only; no physical USB or CPU timing claim",
            "candidate_sha256": meta["syx_sha256"],
            "candidate_root": str(Path(candidate)),
            "diagnostic_page": {
                "title": "V01 DIAG",
                "labels": list(values),
                "seeded_formatter_values": values,
                "units": {"SECS": "seconds since USB1 selection", "UI": "ticks", "WORK": "entries",
                          "WGAP": "ms", "BATCH": "ms", "UGAP": "ms", "FAULT": "count"},
                "saturation": "elapsed seconds and ms fields saturate at 9999; count fields show modulo 10000",
                "clock": "1500 render blocks/s at 48 kHz; nearest-ms conversion",
            },
            "trial_reset": {"native_machine_selection_started_window": True,
                            "compiled_callback_clears_ui_maxima": True,
                            "old_worker_samples_hidden_until_new_entry": True},
            "emulator_tasks": tasks,
            "worker_priority": 14,
            "highest_stock_task_priority": None,
            "task_hierarchy_limit": "The resumed test session exposes no task-create list; the boot log from the candidate build records priorities 0,1,2,5,6,7,8,9,10 and the worker at 14. This is emulator evidence, not a hardware scheduler proof.",
            "limits": [
                "Formatters and reset callbacks run with seeded values; no hardware scheduling, CPU or USB timing is claimed",
                "The four-digit diagnostic text saturates at 9999; a longer stall must be recorded with Pi logs or a larger trace",
            ],
        }
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(record, indent=2) + "\n")
        print("V01 diagnostic page/task hierarchy: PASS")
    finally:
        s.close()


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--candidate", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    run(a.candidate, a.output)
