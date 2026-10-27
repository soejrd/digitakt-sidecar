#!/usr/bin/env python3
"""Install/check the Pi 4 WLAN files required by the pinned Circle driver.

Files are fetched from Circle's pinned RPi-Distro revision, verified against
Git blob IDs, and kept outside Git. Does not read network credentials, change
boot settings, or deploy a kernel. Existing differing files are refused.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import os
from pathlib import Path
import urllib.request

REVISION = 'c9d3ae6584ab79d19a4f94ccf701e888f9f87a53'
BASE = f'https://raw.githubusercontent.com/RPi-Distro/firmware-nonfree/{REVISION}/'
FILES = (
    ('brcmfmac43455-sdio.bin', 'debian/config/brcm80211/cypress/cyfmac43455-sdio-minimal.bin', 548728, 'cc9768bde24da6a4abcd1ef92609b5fdde4359a1'),
    ('brcmfmac43455-sdio.clm_blob', 'debian/config/brcm80211/cypress/cyfmac43455-sdio.clm_blob', 2676, 'c905a503e0182c5d4eae657a587949b3df7ef7ae'),
    ('brcmfmac43455-sdio.txt', 'debian/config/brcm80211/brcm/brcmfmac43455-sdio.txt', 2074, '9a20cb7e4e0ec2c3b6aa2563a31a1a04a3baa066'),
    ('LICENCE.firmware-nonfree', 'debian/copyright', 313331, 'a6074c78bdfadfcdbbb86b40f72f3579ced883be'),
)

def valid(data, size, blob):
    return len(data) == size and hashlib.sha1(f'blob {size}\0'.encode() + data).hexdigest() == blob


def fetch(item):
    name, source, size, blob = item
    with urllib.request.urlopen(BASE + source, timeout=60) as response:
        data = response.read(size + 1)
    if not valid(data, size, blob):
        raise ValueError(f'Pinned file verification failed: {name}')
    return name, data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--card-root', type=Path, required=True)
    parser.add_argument('--check', action='store_true', help='Offline verification only')
    args = parser.parse_args()
    root = args.card_root.resolve()
    if not root.is_dir():
        parser.error('card/package root must already exist')
    directory = root / 'firmware'
    pending = []
    for item in FILES:
        name, _, size, blob = item
        path = directory / name
        if path.exists():
            if not valid(path.read_bytes(), size, blob):
                parser.error(f'Existing file differs; refusing overwrite: {name}')
        else:
            pending.append(item)
    if args.check and pending:
        print('MISSING: ' + ', '.join(item[0] for item in pending))
        return 1
    if pending:
        # Validate every download before writing anything to the card.
        with ThreadPoolExecutor(max_workers=4) as pool:
            downloads = list(pool.map(fetch, pending))
        directory.mkdir(exist_ok=True)
        for name, data in downloads:
            path = directory / name
            temporary = path.with_name(name + '.tmp')
            with temporary.open('xb') as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            temporary.rename(path)
    for name, _, size, blob in FILES:
        assert valid((directory / name).read_bytes(), size, blob), name
        print(f'VERIFIED: {name} ({size} bytes)')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
