#!/usr/bin/env python3
"""Verify V01 Phase 0 artifact, source, dependency and recovery provenance."""
import argparse
import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CANDIDATE = ROOT / "evidence/i04/event-worker-candidate.json"
INSTALLATION = ROOT / "evidence/i04/event-worker-installation.json"
DEPLOYMENT = ROOT / "evidence/i04/event-worker-sd-deployment.json"
FIRMWARE_MANIFEST = ROOT / "private/i04/firmware-I04R/manifest.json"
PATCH_SCRIPT = ROOT / "scripts/test_i04_replay.py"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run(*args, cwd=ROOT):
    return subprocess.run(args, cwd=cwd, text=True, capture_output=True)


def git(*args, cwd=ROOT):
    result = run("git", *args, cwd=cwd)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or f"git {' '.join(args)} failed")
    return result.stdout.strip()


def artifact(name, expected):
    path = ROOT / name
    actual = sha256(path) if path.is_file() else None
    return {"path": name, "expected_sha256": expected, "actual_sha256": actual,
            "pass": actual == expected}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    candidate = json.loads(CANDIDATE.read_text())
    installation = json.loads(INSTALLATION.read_text())
    deployment = json.loads(DEPLOYMENT.read_text())
    firmware_manifest = json.loads(FIRMWARE_MANIFEST.read_text())

    source_mismatches = []
    for name, expected in sorted(candidate["source_sha256"].items()):
        path = ROOT / name
        actual = sha256(path) if path.is_file() else None
        if actual != expected:
            source_mismatches.append({"path": name, "expected": expected, "actual": actual})

    firmware = [
        artifact("private/i04/firmware-I04R/Digitakt-I04R-event-worker.syx",
                 firmware_manifest["candidate_sha256"]),
        artifact("private/i04/firmware-I04R/Digitakt-OS1.54-stock-recovery.syx",
                 firmware_manifest["stock_recovery_sha256"]),
        artifact("private/i04/firmware-I04R/Digitakt-I04Q-previous.syx",
                 firmware_manifest["previous_I04Q_sha256"]),
        artifact("private/i04/firmware-I04R/Digitakt-H02F-previous.syx",
                 firmware_manifest["previous_H02F_sha256"]),
    ]
    kernels = [
        {"role": "installed", **artifact("private/i04/event-worker-final/kernel8-rpi4.img",
                                          deployment["new_kernel_sha256"])},
        {"role": "pre_i04_recovery", **artifact(
            "private/i04/event-worker-final/recovery/kernel8-rpi4.img",
            "a46476a30ee94c0c32dfe05983626869c198e6522fa0a065e338b0b227228a82")},
        {"role": "installed_rollback", **artifact(
            "private/i04/sd-backup-event-worker/kernel8-rpi4.img",
            deployment["old_kernel_sha256"])},
    ]

    sources = json.loads((ROOT / "sources.json").read_text())["repositories"]
    dependencies = []
    for source in sources:
        path = ROOT / "deps" / source["name"]
        actual = git("rev-parse", "HEAD", cwd=path)
        changes = git("status", "--porcelain", cwd=path).splitlines()
        dependencies.append({"name": source["name"], "expected_revision": source["revision"],
                             "actual_revision": actual, "dirty_entries": len(changes),
                             "pass": actual == source["revision"]})

    replay = run("python3", str(PATCH_SCRIPT))
    ignored = []
    for item in firmware + kernels:
        checked = run("git", "check-ignore", "-q", item["path"])
        ignored.append({"path": item["path"], "ignored": checked.returncode == 0})

    tracked_private = git("ls-files", "private").splitlines()
    tracked_binaries = [name for name in git("ls-files").splitlines()
                        if name.lower().endswith((".syx", ".img", ".wav"))]
    record = {
        "task": "V01",
        "phase": "0 preflight and preservation",
        "date_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "offline artifact/source/provenance verification only; no hardware contact or audio test",
        "hardware_contacted": False,
        "evidence_classes": {
            "source_fact": "Manifest and pinned-revision hashes; patch replay",
            "physical_measurement": "None in Phase 0",
            "hypothesis": "Dense-LFO cause remains untested",
        },
        "repository": {
            "head": git("rev-parse", "HEAD"),
            "pre_existing_uncommitted": [name for name in git("status", "--porcelain").splitlines()
                                         if name.endswith(" PLAN.md")],
        },
        "installed_pair": {
            "digitakt_firmware": "I04R",
            "firmware_sha256": installation["candidate_sha256"],
            "firmware_manifest_scope": firmware_manifest["scope"],
            "pi_kernel_sha256": deployment["new_kernel_sha256"],
            "pi_kernel_bytes": deployment["new_kernel_bytes"],
            "playout_delay_frames": deployment["playout_delay_frames"],
            "provenance": [
                "evidence/i04/event-worker-installation.json",
                "evidence/i04/event-worker-sd-deployment.json",
            ],
        },
        "sources": {
            "manifest": "evidence/i04/event-worker-candidate.json",
            "checked": len(candidate["source_sha256"]),
            "mismatches": source_mismatches,
            "pass": not source_mismatches,
        },
        "patches": {
            "base_revision": "6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457",
            "count": len(list((ROOT / "patches/sidecar-firmware").glob("*.patch"))),
            "command": "python3 scripts/test_i04_replay.py",
            "result": replay.stdout.strip(),
            "returncode": replay.returncode,
            "pass": replay.returncode == 0,
        },
        "dependencies": dependencies,
        "recovery": {
            "firmware": firmware,
            "kernels": kernels,
            "rollback_manifest_present": (ROOT / "private/i04/sd-backup-event-worker/SHA256SUMS").is_file(),
            "pass": all(item["pass"] for item in firmware + kernels),
        },
        "private_assets": {
            "all_ignored": all(item["ignored"] for item in ignored),
            "tracked_private_files": len(tracked_private),
            "tracked_firmware_or_audio_binaries": len(tracked_binaries),
            "pass": all(item["ignored"] for item in ignored)
                    and not tracked_private and not tracked_binaries,
        },
        "unresolved": [
            "Hardware release condition is not satisfied in this turn; no device availability, routing or human safety confirmation was obtained.",
            "The full SD-card backup is not enumerated locally; the rollback kernel and its SHA256SUMS are present and hash-verified.",
            "The I04 private firmware manifest still says 'not installed'; installation is corroborated by the separate human-installation and SD-deployment records, not by that scope string.",
            "The dense-LFO bottleneck, persistence locations and supported modulation budget remain unmeasured.",
        ],
        "release_condition": "Human confirms Pi/Digitakt availability, lowered speakers or headphones, disposable project and backups, known routing, and a reviewed paired candidate/recovery before any physical Phase 1 action.",
        "next_action": "Perform read-only OS 1.54 persistence source/emulator inspection and freeze dense-LFO sweep/UI/soak thresholds; do not contact hardware until the release condition is met.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(record, indent=2) + "\n")
    passed = (record["sources"]["pass"] and record["patches"]["pass"]
              and all(item["pass"] for item in dependencies)
              and record["recovery"]["pass"] and record["private_assets"]["pass"])
    print(f"V01 Phase 0 preflight: {'PASS' if passed else 'FAIL'}")
    if not passed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
