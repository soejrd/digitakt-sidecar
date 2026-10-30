#!/usr/bin/env python3
"""Cross-build the default v0.3.0 DTSC kernel in an isolated Pi4 tree.

The source is archived from deps/sidecar-firmware at the v0.3.0 upgrade pin,
the root DTSC series is replayed, cached Circle/engine sources are copied in,
and pi/engine/build.sh runs in the existing rytm-circle Docker image.  No
dependency checkout, SD card, kernel or hardware is changed.

Run this outside a sandbox that denies the Docker socket, or grant Docker
access to the agent.  Output paths must not contain whitespace because the
upstream engine setup expands source lists in shell.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from prepare_v030_source import BASE, ROOT, prepare

CIRCLE_BASE = '7a81e1b480fe7d19fc1fc695b89bff0a0848f44b'


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def capture(args, **kwargs):
    return subprocess.check_output(args, text=True, **kwargs).strip()


def docker(*args, cwd=None):
    return capture(['docker', *args], cwd=cwd)


def build(output, evidence):
    output = output.resolve()
    if any(c.isspace() for c in str(output)):
        raise SystemExit('output path must not contain whitespace')
    if output.exists() and any(output.iterdir()):
        raise SystemExit(f'{output}: output directory is not empty')

    try:
        image_ref = 'rytm-circle:latest'
        image = docker('image', 'inspect', image_ref, '--format', '{{.Id}}')
    except (subprocess.CalledProcessError, FileNotFoundError) as error:
        # Docker Desktop occasionally lists an image and runs it by ID while
        # refusing the same tag in `docker image inspect`.  Resolve the ID from
        # the image list and use that exact reference for the build.
        try:
            ids = docker('image', 'ls', '--no-trunc', '--format', '{{.ID}}',
                         'rytm-circle:latest').splitlines()
            ids = [item.strip() for item in ids if item.strip()]
            if len(ids) != 1:
                raise RuntimeError(f'expected one rytm-circle image, found {ids}')
            image_ref = ids[0]
            image = docker('image', 'inspect', image_ref, '--format', '{{.Id}}')
            print(f'rytm-circle tag lookup failed; using image ID {image_ref}')
        except (subprocess.CalledProcessError, FileNotFoundError, RuntimeError) as fallback:
            raise SystemExit(
                'Docker/rytm-circle is unavailable. Start Docker, build/import the '
                'rytm-circle image, and run this command outside a Docker-denying '
                'sandbox. Original error: ' + str(error) +
                '; ID fallback: ' + str(fallback))

    cache = ROOT / 'deps/sidecar-firmware'
    circle = cache / 'pi/circle/build'
    if not (circle / 'circle').is_dir() or not (circle / 'circle-engine').is_dir():
        raise SystemExit('cached Circle tree missing under deps/sidecar-firmware/pi/circle/build')
    revision = capture(['git', '-C', str(circle / 'circle'), 'rev-parse', 'HEAD'])
    if revision != CIRCLE_BASE:
        raise SystemExit(f'Circle base {revision} != {CIRCLE_BASE}')

    prepare(output)
    # pi/engine/build.sh resolves its root through git, and tools/gen_version.sh
    # reads this public dependency revision.  The overlay remains a diff.
    subprocess.run(['git', 'init', '-q', str(output)], check=True)
    subprocess.run(['git', '-C', str(output), 'fetch', '-q', '--depth=1', '--no-tags',
                    str(cache), BASE], check=True)
    subprocess.run(['git', '-C', str(output), 'reset', '-q', '--mixed', 'FETCH_HEAD'], check=True)
    for relative in ('pi/circle/build', 'teensy/libraries'):
        shutil.copytree(cache / relative, output / relative, dirs_exist_ok=True)
    # The cached Circle trees already contain local patches.  Recreate only the
    # engine tree from the pinned Circle commit so v0.3.0's patch order sees
    # the context it was written against.
    staged_circle = output / 'pi/circle/build/circle'
    if not (staged_circle / '.git').exists():
        raise SystemExit('staged Circle checkout is not a git tree')
    staged_engine = output / 'pi/circle/build/circle-engine'
    if staged_engine.exists():
        shutil.rmtree(staged_engine)
    subprocess.run(['git', '-C', str(staged_circle), 'archive', CIRCLE_BASE],
                   stdout=(output / 'circle-base.tar').open('wb'), check=True)
    staged_engine.mkdir(parents=True)
    subprocess.run(['tar', '-xf', str(output / 'circle-base.tar'), '-C', str(staged_engine)], check=True)
    # The Docker container has no git credentials.  Carry the cached hostap
    # submodule and LVGL sources into the clean tree; the libraries built from
    # them are rebuilt in the container.
    cached_engine = circle / 'circle-engine'
    hostap = cached_engine / 'addon/wlan/hostap'
    if not (hostap / 'wpa_supplicant/Makefile.circle').is_file():
        raise SystemExit('cached hostap submodule is missing')
    shutil.copytree(hostap, staged_engine / 'addon/wlan/hostap', dirs_exist_ok=True)
    lvgl = cached_engine / 'addon/lvgl/lvgl'
    if not lvgl.is_dir():
        raise SystemExit('cached LVGL sources are missing')
    shutil.copytree(lvgl, staged_engine / 'addon/lvgl/lvgl', dirs_exist_ok=True)
    # v0.3.0's circle-touch.patch still has the pre-DTSC factory comment in
    # one context line.  Normalize that copied patch only; the dependency
    # checkout stays untouched.
    touch_patch = output / 'pi/circle/circle-touch.patch'
    touch = touch_patch.read_text()
    old_context = ' // rytm-pi hook: the application may define this to claim the Analog Rytm MKII (1935:0b48)\n'
    new_context = ' // rytm-pi hook: the application may define this to claim an Elektron Overbridge device\n'
    if old_context not in touch:
        raise SystemExit('circle-touch patch context changed; review the patch order')
    touch_patch.write_text(touch.replace(old_context, new_context, 1))
    # Upstream build.sh hardcodes the tag.  Let this isolated build use the
    # resolved name or ID without editing deps/.
    build_script = output / 'pi/engine/build.sh'
    script = build_script.read_text()
    script = script.replace(
        'docker image inspect rytm-circle >/dev/null 2>&1 || docker build -t rytm-circle',
        'docker image inspect "$RYTM_CIRCLE_IMAGE" >/dev/null 2>&1 || docker build -t "$RYTM_CIRCLE_IMAGE"', 1)
    script = script.replace(' rytm-circle bash ', ' "$RYTM_CIRCLE_IMAGE" bash ', 1)
    build_script.write_text(script)

    log = output / 'v030-build.log'
    env = dict(os.environ, RASPPI='4', RYTM_CIRCLE_IMAGE=image_ref)
    with log.open('w') as stream:
        result = subprocess.run(['./pi/engine/build.sh', 'kernel'], cwd=output, env=env,
                                stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        tail = log.read_text(errors='replace')[-8000:]
        print(tail)
        raise SystemExit(f'Pi build failed ({result.returncode}); log: {log}')

    engine = output / 'pi/engine'
    image_path = engine / 'kernel8-rpi4.img'
    elf_path = engine / 'kernel8-rpi4.elf'
    for path in (image_path, elf_path):
        if not path.is_file():
            raise SystemExit(f'build did not produce {path}')

    size = capture(['docker', 'run', '--rm', '-v', f'{output}:/repo:ro', image_ref,
                    'aarch64-none-elf-size', '/repo/pi/engine/kernel8-rpi4.elf']).splitlines()[-1]
    text, data, bss = (int(value) for value in size.split()[:3])
    compiler = docker('run', '--rm', '-v', f'{output}:/repo:ro', 'rytm-circle',
                       'aarch64-none-elf-g++', '--version').splitlines()[0]
    symbols = capture(['docker', 'run', '--rm', '-v', f'{output}:/repo:ro', image_ref,
                       'aarch64-none-elf-nm', '-S', '--size-sort', '--radix=d',
                       '/repo/pi/engine/kernel8-rpi4.elf'])
    end_line = [line for line in symbols.splitlines() if line.endswith(' _end')]
    if not end_line:
        raise SystemExit('ELF has no _end symbol')
    end = int(end_line[0].split()[0], 10)
    config = (output / 'pi/circle/build/circle-engine/Config.mk').read_text(errors='replace')
    match = re.search(r'-DKERNEL_MAX_SIZE=(0x[0-9a-fA-F]+)', config)
    if not match:
        raise SystemExit('KERNEL_MAX_SIZE not found in the cached Circle Config.mk')
    limit = int(match.group(1), 16)
    used = end - 0x80000
    if used > limit:
        raise SystemExit(f'kernel image {used} B exceeds KERNEL_MAX_SIZE {limit} B')

    # Static costs relevant to the migration: the output FIFO and the DTSC
    # state.  Runtime stack peaks are deliberately not inferred from these.
    wanted = ('g_Output', 'dt_link', 's_Block', 's_BlockR', 's_St', 's_Midi')
    static_symbols = {}
    for line in symbols.splitlines():
        fields = line.split()
        if len(fields) >= 4 and any(name in fields[-1] for name in wanted):
            static_symbols[fields[-1]] = int(fields[1], 10)

    source_sha = {}
    for path in sorted((ROOT / 'patches/sidecar-firmware-v0.3.0').glob('*.patch')):
        source_sha[str(path.relative_to(ROOT))] = sha256(path)
    for name in ('scripts/prepare_v030_source.py', 'scripts/v030_build_pi.py'):
        source_sha[name] = sha256(ROOT / name)
    report = {
        'scope': 'isolated default v0.3.0 DTSC Pi 4 AArch64 kernel cross-build; cached Circle libraries; no hardware',
        'command': 'python3 scripts/v030_build_pi.py --output /tmp/dtsc-v030-pi-build',
        'dependency_base': BASE,
        'circle_base': revision,
        'circle_engine_revision': capture(['git', '-C', str(output / 'pi/circle/build/circle-engine'), 'rev-parse', 'HEAD']),
        'docker_image': image,
        'docker_image_ref': image_ref,
        'compiler': compiler,
        'kernel_file': 'kernel8-rpi4.img',
        'kernel_sha256': sha256(image_path),
        'kernel_bytes': image_path.stat().st_size,
        'elf_file': 'kernel8-rpi4.elf',
        'elf_sections_bytes': {'text': text, 'data': data, 'bss': bss},
        'kernel_used_bytes': used,
        'kernel_limit_bytes': limit,
        'kernel_headroom_bytes': limit - used,
        'static_symbols_bytes': static_symbols,
        'build_log': 'v030-build.log',
        'build_log_sha256': sha256(log),
        'source_sha256': source_sha,
        'pass': True,
        'limits': [
            'Static ELF/BSS and selected symbol sizes only; runtime stack peaks and CPU margin are not measured.',
            'Cached Circle/engine libraries are reused; this is not a clean rebuild of every third-party library.',
            'Cross-build only; no candidate deployment or hardware qualification.',
        ],
    }
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, default=ROOT / 'evidence/upgrades/v0.3.0-pi-build.json')
    args = parser.parse_args()
    build(args.output, args.evidence)
