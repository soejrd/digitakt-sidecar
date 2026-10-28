#!/usr/bin/env python3
"""Stage only a private, silent-by-default Pi kernel and verified H02 rollback.
Never mount media, write an SD card, contact hardware or flash firmware.
"""
import argparse
import re
import hashlib
import json
import pathlib

root = pathlib.Path(__file__).resolve().parent.parent
sha = lambda data: hashlib.sha256(data).hexdigest()
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--evidence', default='evidence/i04/host.json')
ap.add_argument('--trial-name', default='trial')
args = ap.parse_args()
assert re.fullmatch(r'[a-zA-Z0-9_-]+', args.trial_name), 'invalid private trial name'
evidence = json.loads((root / args.evidence).read_text())
build = next(item for item in evidence['checks'] if item.get('target') == 'Pi 4 AArch64 Circle')
for name, expected in evidence['source_sha256'].items():
    assert sha((root / name).read_bytes()) == expected, f'stale source evidence: {name}'
kernel = (root / 'deps/sidecar-firmware/pi/engine/kernel8-rpi4.img').read_bytes()
assert sha(kernel) == build['kernel_sha256'] and len(kernel) == build['kernel_bytes']
rollback = (root / 'private/h02/trial/kernel8-rpi4-clock.img').read_bytes()
h02 = json.loads((root / 'evidence/h02/sd-clock-deployment.json').read_text())
assert sha(rollback) == h02['new_kernel_sha256'], 'rollback is not the physically booted H02 kernel'
assert (root / 'private/h02/trial/Digitakt-H02F-mode6.syx').is_file()
assert (root / 'private/h02/trial/Digitakt-OS1.54-stock-recovery.syx').is_file()
manifest = {
    'task': 'I04', 'scope': 'private staging only; not deployed',
    'kernel_sha256': sha(kernel), 'kernel_bytes': len(kernel),
    'rollback_sha256': sha(rollback), 'default_audio': 'silent; explicit trx=on after valid session',
    'source_sha256': evidence['source_sha256'],
    'digitakt': 'existing H02F mode6 candidate; no firmware change for I04',
}
files = {
    'kernel8-rpi4.img': kernel,
    'SHA256SUMS': (sha(kernel)+'  kernel8-rpi4.img\n').encode(),
    'recovery/kernel8-rpi4.img': rollback,
    'recovery/SHA256SUMS': (sha(rollback)+'  kernel8-rpi4.img\n').encode(),
    'manifest.json': (json.dumps(manifest, indent=2)+'\n').encode(),
}
# Compare everything before writing anything. Refuse to overwrite a prior trial.
folder = root / 'private/i04' / args.trial_name
for name, data in files.items():
    path = folder / name
    assert not path.exists() or path.read_bytes() == data, f'existing trial differs: {name}'
for name, data in files.items():
    path = folder / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    assert path.read_bytes() == data
print('I04 private candidate and physically verified H02 rollback staged/read-back verified; no deployment')
