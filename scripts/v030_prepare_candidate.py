#!/usr/bin/env python3
"""Stage a verified v0.3.0 Pi kernel and the installed v0.2 kernel as rollback.

No SD card, bootloader, firmware or hardware is touched.  The staged package is
private and remains uninstalled until a human reviews the manifest and recovery.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROLLBACK = 'private/i04/event-worker-final/kernel8-rpi4.img'
DEFAULT_ROLLBACK_SHA = 'c057f063c24770a520b842e66d4d7e10655bdda3d45de05e1b67edc85d34a0f2'


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-evidence', default='evidence/upgrades/v0.3.0-pi-build.json')
    parser.add_argument('--artifact-dir', default='/tmp/dtsc-v030-pi-build/pi/engine')
    parser.add_argument('--rollback', default=DEFAULT_ROLLBACK)
    parser.add_argument('--rollback-sha', default=DEFAULT_ROLLBACK_SHA)
    parser.add_argument('--trial-name', default='v030-default')
    parser.add_argument('--output-root', default='private/u01')
    args = parser.parse_args()
    if not re.fullmatch(r'[a-zA-Z0-9_-]+', args.trial_name):
        raise SystemExit('invalid trial name')
    evidence_path = ROOT / args.build_evidence
    evidence = json.loads(evidence_path.read_text())
    if not evidence.get('pass'):
        raise SystemExit('build evidence is not a passing build')
    artifact_dir = Path(args.artifact_dir)
    kernel = (artifact_dir / evidence['kernel_file']).read_bytes()
    if sha256(kernel) != evidence['kernel_sha256'] or len(kernel) != evidence['kernel_bytes']:
        raise SystemExit('kernel artifact does not match build evidence')
    rollback = (ROOT / args.rollback).read_bytes()
    if sha256(rollback) != args.rollback_sha:
        raise SystemExit('rollback kernel does not match its pinned hash')
    for name, digest in evidence.get('source_sha256', {}).items():
        if sha256((ROOT / name).read_bytes()) != digest:
            raise SystemExit(f'stale source evidence: {name}')

    manifest = {
        'task': 'U01',
        'scope': 'private staged v0.3.0 Pi candidate; human review required; not installed',
        'kernel_sha256': sha256(kernel),
        'kernel_bytes': len(kernel),
        'build_evidence': args.build_evidence,
        'dependency_base': evidence['dependency_base'],
        'source_sha256': evidence.get('source_sha256', {}),
        'rollback_sha256': sha256(rollback),
        'rollback_source': args.rollback,
        'default_audio': 'silent; explicit trx=on only after a valid DTSC session',
        'digitakt_firmware': 'I04R remains installed; this package changes only the Pi kernel',
        'recovery': 'With the Pi off, restore recovery/kernel8-rpi4.img as kernel8-rpi4.img and its SHA256SUMS; do not flash a Digitakt image.',
        'human_gate': 'Review hashes and recovery; lower speakers; use a disposable project; no agent SD write or hardware flash.',
    }
    files = {
        'kernel8-rpi4.img': kernel,
        'SHA256SUMS': (sha256(kernel) + '  kernel8-rpi4.img\n').encode(),
        'recovery/kernel8-rpi4.img': rollback,
        'recovery/SHA256SUMS': (sha256(rollback) + '  kernel8-rpi4.img\n').encode(),
        'manifest.json': (json.dumps(manifest, indent=2) + '\n').encode(),
    }
    folder = ROOT / args.output_root / args.trial_name
    for name, data in files.items():
        path = folder / name
        if path.exists() and path.read_bytes() != data:
            raise SystemExit(f'existing trial differs: {path}')
    for name, data in files.items():
        path = folder / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        if path.read_bytes() != data:
            raise SystemExit(f'read-back mismatch: {path}')
    print(f'U01 candidate and rollback staged at {folder}; no deployment')


if __name__ == '__main__':
    main()
