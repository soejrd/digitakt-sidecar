#!/usr/bin/env python3
"""Clone pinned public dependencies; optionally reuse existing local Git objects."""
import argparse
import json
from pathlib import Path
import subprocess


def git(*args, cwd=None, capture=False):
    result = subprocess.run(["git", *args], cwd=cwd, check=True,
                            text=True, stdout=subprocess.PIPE if capture else None)
    return result.stdout.strip() if capture else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    entries = json.loads((root / "sources.json").read_text())["repositories"]
    (root / "deps").mkdir(exist_ok=True)
    for entry in entries:
        name, revision, url = entry["name"], entry["revision"], entry["url"]
        target = root / "deps" / name
        if target.exists():
            actual = git("rev-parse", "HEAD", cwd=target, capture=True)
            if actual != revision:
                raise SystemExit(f"{name}: HEAD {actual} differs from pin {revision}; preserving checkout")
            dirty = git("status", "--porcelain", cwd=target, capture=True)
            print(f"{name}: pinned checkout present" + (" (local edits preserved)" if dirty else ""))
            continue
        local = args.source_root.resolve() / name if args.source_root else None
        source = str(local) if local and (local / ".git").exists() else url
        git("clone", "--no-hardlinks", "--no-checkout", source, str(target))
        # Network clones may need the pinned revision from history; full clone is intentional.
        git("checkout", "--detach", revision, cwd=target)
        git("remote", "set-url", "origin", url, cwd=target)
        print(f"{name}: ready at {revision}")


if __name__ == "__main__":
    main()
