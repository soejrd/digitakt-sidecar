#!/usr/bin/env python3
"""Build the pinned patched Unicorn in an isolated private package directory.

Run using a Python environment with the pinned emulator's dependencies. Public
source is fetched by the upstream installer. Run outside a sandbox that denies
the native host cache query. No emulator firmware or hardware is accessed.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path('private/r03-runtime'),
                        help='new isolated runtime directory; existing roots are preserved')
    args = parser.parse_args()
    import unicorn
    if unicorn.__version__ != '2.1.4':
        raise SystemExit('Use an environment containing unicorn==2.1.4')
    project = Path(__file__).resolve().parents[1]
    root = args.root.resolve()
    root.mkdir(parents=True, exist_ok=False)
    shutil.copytree(Path(unicorn.__file__).resolve().parent, root / 'unicorn')
    env = dict(os.environ, PYTHON=sys.executable,
               PYTHONPATH=os.pathsep.join((str(root), str(project / 'deps/digiemu'))))
    # Ensure an inherited override cannot redirect the native compatibility
    # checks back to an unrelated library instead of the new isolated build.
    env.pop('LIBUNICORN_PATH', None)
    subprocess.run(['bash', str(project / 'deps/digiemu/tools/install-patched-unicorn.sh')],
                   cwd=project, env=env, check=True)
    print('Use PYTHONPATH=' + env['PYTHONPATH'])


if __name__ == '__main__':
    main()
