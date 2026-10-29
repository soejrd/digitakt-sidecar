#!/usr/bin/env python3
"""Cross-build X01 in a new disposable tree using local cached dependencies.

Requires Docker, rytm-circle, and an existing Pi 4 Circle build cache. Never
changes deps/, an installed kernel, a disk, or hardware. Output must have no
whitespace because the upstream engine setup expands source lists in shell.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

from x01_prepare_host import prepare, ROOT


def capture(args, **kwargs):
    return subprocess.check_output(args, text=True, **kwargs).strip()


def build(output, evidence):
    output = output.resolve()
    if any(c.isspace() for c in str(output)):
        raise ValueError('Use a new path without whitespace, e.g. /tmp/x01-pi-build')
    cache = ROOT/'deps/sidecar-firmware'
    circle = cache/'pi/circle/build'
    for name in ('circle', 'circle-engine'):
        revision = capture(['git', '-C', str(circle/name), 'rev-parse', 'HEAD'])
        assert revision == '7a81e1b480fe7d19fc1fc695b89bff0a0848f44b', revision
    assert (circle/'circle-engine/.engine-ready').is_file()
    config = (circle/'circle-engine/Config.mk').read_text()
    assert 'RASPPI = 4' in config and 'AARCH = 64' in config
    image = capture(['docker', 'image', 'inspect', 'rytm-circle', '--format', '{{.Id}}'])
    prepare(output)
    # Upstream build.sh resolves its root using git. Import only the local
    # public dependency's pinned base, leaving the opt-in overlay as a diff.
    pin = next(r['revision'] for r in json.loads((ROOT/'sources.json').read_text())['repositories']
               if r['name'] == 'sidecar-firmware')
    subprocess.run(['git', 'init', '-q', str(output)], check=True)
    subprocess.run(['git', '-C', str(output), 'fetch', '-q', '--depth=1', '--no-tags',
                    str(cache), pin], check=True)
    subprocess.run(['git', '-C', str(output), 'reset', '-q', '--mixed', 'FETCH_HEAD'], check=True)
    for relative in ('pi/circle/build', 'teensy/libraries'):
        shutil.copytree(cache/relative, output/relative, dirs_exist_ok=True)
    log = output/'x01-build.log'
    with log.open('w') as stream:
        result = subprocess.run(['./pi/engine/build.sh', 'kernel'], cwd=output,
                                stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text()[-6000:])
        result.check_returncode()
    engine = output/'pi/engine'
    prefix = ['docker', 'run', '--rm', '-v', str(output)+':/repo:ro', 'rytm-circle']
    sizes = capture(prefix+['aarch64-none-elf-size', '/repo/pi/engine/kernel8-rpi4.elf'])
    text, data, bss = map(int, sizes.splitlines()[-1].split()[:3])
    compiler = capture(prefix+['aarch64-none-elf-g++', '--version']).splitlines()[0]
    symbols = capture(prefix+['aarch64-none-elf-nm', '-S', '--size-sort', '--radix=d',
                              '/repo/pi/engine/kernel8-rpi4.elf'])
    peer = [line.split() for line in symbols.splitlines() if line.endswith('_ZN7dt_linkL4peerE')]
    assert len(peer) == 1, peer
    report = {
        'scope': 'isolated opt-in X01 Pi 4 AArch64 kernel cross-build; cached Circle libraries; no hardware',
        'command': 'python3 scripts/x01_build_host.py --output /tmp/x01-pi-build',
        'dependency_base': pin, 'circle_base': revision,
        'docker_image': image, 'compiler': compiler,
        'kernel_sha256': hashlib.sha256((engine/'kernel8-rpi4.img').read_bytes()).hexdigest(),
        'kernel_bytes': (engine/'kernel8-rpi4.img').stat().st_size,
        'elf_sections_bytes': {'text': text, 'data': data, 'bss': bss},
        'x01_peer_static_bytes': int(peer[0][1]),
        'source_sha256': {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
                          for name in ('host/x01_dt_link.cpp', 'scripts/x01_prepare_host.py',
                                       'scripts/x01_build_host.py',
                                       'patches/sidecar-firmware/experimental/0001-x01-stereo-return.patch')},
        'limits': ['Cross-build only; not a reviewed hardware candidate.',
                   'Static ELF sizes do not measure runtime heap, stack peaks, CPU margin or USB timing.',
                   'Existing cached Circle libraries are reused; not a clean rebuild of all third-party libraries.'],
        'pass': True}
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, default=ROOT/'evidence/x01/pi-build.json')
    args = parser.parse_args()
    build(args.output, args.evidence)
