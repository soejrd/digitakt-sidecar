#!/usr/bin/env python3
"""Add verified Pi candidate + exact pre-probe rollback to the V01E package.
Offline only: no mounts, SD writes, network calls or deployment.
"""
import hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parent.parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
r=json.loads((root/'evidence/v01/remote-pi-build.json').read_text())
for name,digest in r['source_sha256'].items():assert sha(root/name)==digest,name
image=root/'deps/sidecar-firmware/pi/engine/kernel8-rpi4.img'
assert sha(image)==r['kernel_sha256'] and image.stat().st_size==r['kernel_bytes']
rollback=root/'private/i04/event-worker-final/kernel8-rpi4.img'
assert sha(rollback)=='c057f063c24770a520b842e66d4d7e10655bdda3d45de05e1b67edc85d34a0f2'
out=root/'private/v01/firmware-V01E'
m=json.loads((out/'manifest.json').read_text());assert m['label']=='V01E'
assert 'V01_REMOTE_DIAGNOSTIC' in m['compiler_defines']
assert sha(out/'Digitakt-V01E-diagnostic.syx')==m['candidate_sha256']
manifest={'scope':'paired remote diagnostics only; not installed','digitakt_sha256':m['candidate_sha256'],'kernel_sha256':r['kernel_sha256'],'kernel_bytes':r['kernel_bytes'],'rollback_sha256':sha(rollback),'playout_delay_frames':128,'default_audio':'silent; do not arm during muted isolation trial','source_sha256':r['source_sha256'],'checks':'evidence/v01/remote-pi-build.json and remote-exchange.json','rate_limit':'at most one best-effort diagnostic frame per 1500 render blocks; no telemetry backlog','raw_timer':'read-only DTIM0 count/mode; calibrate on hardware before interpreting ticks'}
files={'pi/kernel8-rpi4.img':image.read_bytes(),'pi/recovery/kernel8-rpi4.img':rollback.read_bytes(),'paired-pi.json':(json.dumps(manifest,indent=2)+'\n').encode()}
for name,data in files.items():
 p=out/name;assert not p.exists() or p.read_bytes()==data,'existing different file: '+name
for name,data in files.items():
 p=out/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
 assert p.read_bytes()==data
print('Paired Pi candidate and physically used pre-probe rollback staged; no deployment')
