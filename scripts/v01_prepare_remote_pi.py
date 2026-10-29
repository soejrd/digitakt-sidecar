#!/usr/bin/env python3
"""Add a verified paired Pi candidate + exact prior-kernel rollback to a V01 package.
Offline only: no mounts, SD writes, network calls or deployment.
"""
import argparse,hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parent.parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--label',choices=('V01E','V01F','V01G'),default='V01E')
ap.add_argument('--evidence',default='evidence/v01/remote-pi-build.json')
ap.add_argument('--rollback',default='private/i04/event-worker-final/kernel8-rpi4.img')
ap.add_argument('--rollback-sha',default='c057f063c24770a520b842e66d4d7e10655bdda3d45de05e1b67edc85d34a0f2')
a=ap.parse_args()
r=json.loads((root/a.evidence).read_text())
for name,digest in r['source_sha256'].items():assert sha(root/name)==digest,name
image=root/'deps/sidecar-firmware/pi/engine/kernel8-rpi4.img'
assert sha(image)==r['kernel_sha256'] and image.stat().st_size==r['kernel_bytes']
rollback=root/a.rollback
assert sha(rollback)==a.rollback_sha
out=root/('private/v01/firmware-'+a.label)
m=json.loads((out/'manifest.json').read_text());assert m['label']==a.label
assert 'V01_REMOTE_DIAGNOSTIC' in m['compiler_defines']
assert sha(out/('Digitakt-'+a.label+'-diagnostic.syx'))==m['candidate_sha256']
manifest={'scope':'paired remote diagnostics only; not installed','digitakt_sha256':m['candidate_sha256'],'kernel_sha256':r['kernel_sha256'],'kernel_bytes':r['kernel_bytes'],'rollback_sha256':sha(rollback),'playout_delay_frames':128,'default_audio':'silent; do not arm during muted isolation trial','source_sha256':r['source_sha256'],'checks':a.evidence,'rate_limit':'at most one best-effort diagnostic frame per 1500 render blocks; no telemetry backlog','raw_timer':'read-only DTIM0 count/mode; calibrate on hardware before interpreting ticks'}
files={'pi/kernel8-rpi4.img':image.read_bytes(),'pi/recovery/kernel8-rpi4.img':rollback.read_bytes(),'paired-pi.json':(json.dumps(manifest,indent=2)+'\n').encode()}
for name,data in files.items():
 p=out/name;assert not p.exists() or p.read_bytes()==data,'existing different file: '+name
for name,data in files.items():
 p=out/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
 assert p.read_bytes()==data
print('Paired Pi candidate and physically used pre-probe rollback staged; no deployment')
