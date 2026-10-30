#!/usr/bin/env python3
"""Read fetched v0.3.0 tags and audit patch replay without changing any checkout."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TAG = 'v0.3.0'


def git(repo, *args):
    return subprocess.check_output(['git','-C',str(ROOT/'deps'/repo),*args],text=True).strip()


def main():
    pins = {r['name']:r for r in json.loads((ROOT/'sources.json').read_text())['repositories']}
    targets = {}
    for name in ('sidecar-firmware','sidecar-mods'):
        targets[name] = {'tag':TAG,'revision':git(name,'rev-parse',TAG+'^{}'),
                         'current_pin':pins[name]['revision'],
                         'upstream':pins[name]['url'],
                         'change_summary':git(name,'diff','--shortstat',pins[name]['revision'],TAG)}
    checks = []
    patches = sorted((ROOT/'patches/sidecar-firmware').glob('*.patch'))
    with tempfile.TemporaryDirectory(prefix='digitakt-upgrade-audit-') as directory:
        archive = subprocess.Popen(['git','-C',str(ROOT/'deps/sidecar-firmware'),
            'archive',targets['sidecar-firmware']['revision']],stdout=subprocess.PIPE)
        subprocess.run(['tar','-xf','-','-C',directory],stdin=archive.stdout,check=True)
        archive.stdout.close(); assert archive.wait()==0
        env=dict(os.environ,GIT_CEILING_DIRECTORIES=str(Path(directory).parent))
        for patch in patches:
            result=subprocess.run(['git','apply','--check',str(patch)],cwd=directory,
                                  env=env,capture_output=True,text=True)
            row={'patch':patch.name,'sha256':hashlib.sha256(patch.read_bytes()).hexdigest(),
                 'applies_after_prior_patches':result.returncode==0}
            checks.append(row)
            if result.returncode:
                row['errors']=result.stderr.strip().splitlines(); break
            subprocess.run(['git','apply',str(patch)],cwd=directory,env=env,check=True)
        output=(Path(directory)/'pi/engine/output.h').read_text()
        buckets=all(name in output for name in ('RestartUnderflows()', 'Streams()', 'SetFifoTarget('))
    result={'scope':'offline fetched-tag compatibility audit, not a completed upgrade or hardware validation',
            'command':'python3 scripts/audit_upstream_upgrade.py','targets':targets,
            'root_patch_count':len(patches),'sequential_patch_checks':checks,
            'remaining_patches_not_attempted':len(patches)-len(checks),
            'upstream_has_restart_vs_playback_underflow_buckets':buckets,
            'status':'port-required' if not all(r['applies_after_prior_patches'] for r in checks) else 'root-patches-replay',
            'limits':['Pins and dependency working trees unchanged; only remote refs fetched.',
                      'X01 experimental patch, overlay compilation and Pi cross-build not qualified on v0.3.0.',
                      'sidecar-mods release targets Rytm MKII OS1.73, not Digitakt mk1 OS1.54.',
                      'No firmware/kernel/SD changes and no claim of fixing V01.']}
    destination=ROOT/'evidence/upgrades/v0.3.0-audit.json'
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_text(json.dumps(result,indent=2)+'\n')
    print(result['status']+': '+checks[-1]['patch'])


if __name__=='__main__':main()
