#!/usr/bin/env python3
"""Verify and stage the private V01D diagnostic firmware and its recovery chain."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(root / "deps/elekloader"))
from elekloader.syx import Syx, verify
from elekloader.devices.digitakt_mk1 import DEVICE

sha = lambda data: hashlib.sha256(data).hexdigest()
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument("--candidate", default="private/v01-diag-6")
ap.add_argument("--label", default="V01D")
ap.add_argument("--output", default="private/v01/firmware-V01D")
args = ap.parse_args()

folder = root / args.candidate
meta = json.loads((folder / "fixture.json").read_text())
assert meta["firmware_label"] == args.label
assert meta["compiler_defines"] == ["USB_TRACK_DIAGNOSTIC"]
for name, expected in meta["source_hashes"].items():
    assert sha((root / name).read_bytes()) == expected, "stale candidate: " + name

raw = (folder / "artifacts/I02.syx").read_bytes()
assert sha(raw) == meta["syx_sha256"]
stock = Syx((root / "private/i02-fixture/Digitakt_OS1.54.syx").read_bytes())
new = Syx(raw)
facts = verify(raw, stock, new.section(3), DEVICE, version=args.label)

ui = json.loads((root / "evidence/v01/phase2-diagnostic-ui.json").read_text())
runtime = json.loads((root / "evidence/v01/phase2-diagnostic-runtime.json").read_text())
exchange = json.loads((root / "evidence/v01/phase2-diagnostic-exchange.json").read_text())
diagnostic = json.loads((root / "evidence/v01/phase2-diagnostic-candidate.json").read_text())
assert ui["ui_emulator"]["candidate_sha256"] == sha(raw)
assert runtime["candidate_sha256"] == sha(raw) and runtime["status"] == "verified"
assert exchange["candidate"]["syx_sha256"] == sha(raw) and exchange["status"] == "verified"
assert diagnostic["candidate_sha256"] == sha(raw) and diagnostic["diagnostic_page"]["labels"] == [
    "RCLK", "UI", "WORK", "WGAP", "BATCH", "UGAP", "FAULT"]

recovery_files = {
    "Digitakt-I04R-previous.syx": root / "private/i04/firmware-I04R/Digitakt-I04R-event-worker.syx",
    "Digitakt-I04Q-previous.syx": root / "private/i04/firmware-I04R/Digitakt-I04Q-previous.syx",
    "Digitakt-H02F-previous.syx": root / "private/i04/firmware-I04R/Digitakt-H02F-previous.syx",
    "Digitakt-OS1.54-stock-recovery.syx": root / "private/i04/firmware-I04R/Digitakt-OS1.54-stock-recovery.syx",
}
expected = {
    "Digitakt-I04R-previous.syx": "6a04ae9dd7791c7b11489af0061ea0012464922d5b6ca6935450f2b712f7fa2b",
    "Digitakt-I04Q-previous.syx": "75f5b66ae5db95676eb767ece3239ec81a9281a42966fbca7689b22a8b14bb8b",
    "Digitakt-H02F-previous.syx": "5acb609a4b409153d2eed7b528becf4ca553d7cce548031f00d4910c4e0ebd33",
    "Digitakt-OS1.54-stock-recovery.syx": "f78ba80fa7b1da5fb0e1ff61ad61e9e71aafe79f4364fc49679f3651353e3cf6",
}
for name, path in recovery_files.items():
    assert path.is_file() and sha(path.read_bytes()) == expected[name], "recovery changed: " + name

manifest = {
    "label": args.label,
    "scope": "private diagnostic candidate; human installation only",
    "candidate_sha256": sha(raw),
    "verification": facts,
    "source_hashes": meta["source_hashes"],
    "diagnostic_page": diagnostic["diagnostic_page"],
    "checks": [
        "compiled boot and assignment",
        "UI page labels and formatters",
        "native RTOS genuine sleep/wake and stack guards",
        "compiled native transport/ACK/loss/recovery",
        "I04 host/emulator regression",
    ],
    "recovery": {name: expected[name] for name in recovery_files},
    "human_gate": "Install only with speakers lowered, disposable project backed up, and the recovery files retained.",
}
files = {"Digitakt-V01D-diagnostic.syx": raw, **{name: path.read_bytes() for name, path in recovery_files.items()},
         "manifest.json": (json.dumps(manifest, indent=2) + "\n").encode()}

out = root / args.output
for name, data in files.items():
    path = out / name
    assert not path.exists() or path.read_bytes() == data, "existing different artifact: " + name
for name, data in files.items():
    path = out / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    assert path.read_bytes() == data
print(f"{args.label} verified; diagnostic and I04R/I04Q/H02F/stock recovery staged at {out}")
