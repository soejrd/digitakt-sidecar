#!/usr/bin/env python3
"""Offline replay at the pinned base; no checkout changes."""
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="i04-replay-") as directory:
    archive = subprocess.Popen(
        ["git", "-C", str(root / "deps/sidecar-firmware"), "archive",
         "6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457"], stdout=subprocess.PIPE)
    subprocess.run(["tar", "-xf", "-", "-C", directory], stdin=archive.stdout, check=True)
    assert archive.wait() == 0
    for patch in sorted((root / "patches/sidecar-firmware").glob("*.patch")):
        subprocess.run(["git", "apply", str(patch)], cwd=directory, check=True)
    for name in ["pi/engine/Makefile", "pi/engine/dt_link.cpp", "pi/engine/dt_link.h",
                 "pi/engine/cores.cpp", "pi/engine/output.cpp", "pi/engine/output.h", "NOTICE"]:
        assert (pathlib.Path(directory) / name).read_bytes() == (
            root / "deps/sidecar-firmware" / name).read_bytes(), name
print("Nine patches replay at pinned base; I04 production files match: PASS")
