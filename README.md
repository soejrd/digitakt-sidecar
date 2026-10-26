# Digitakt integrated sidecar

Run TRX-B2 externally on a Pi 4B, controlled from a Digitakt mk1 OS 1.54 audio track, with audio returned through that track's processing. Full hardware integration remains unproven.

- [Task board](TASKBOARD.md): work, ownership and blockers. **R07 is next.**
- [Progress](PROGRESS.md): established results and remaining uncertainty.
- [Plan](PLAN.md): goal, architecture and acceptance gates.
- [Agent instructions](AGENTS.md) and [copyable prompt](AGENT-PROMPT.md).
- [References](docs/REFERENCES.md): sources organized by project use.

## Setup and checks

```sh
python3 scripts/bootstrap.py
# Or reuse existing clones: --source-root /path/to/clones
```

sources.json pins dependency revisions. Clones in deps/ are ignored; replay [the I01 patch](patches/sidecar-firmware/README.md) before running `./scripts/test_i01.sh`.

The [R03 reproduction guide](docs/R03-REPRODUCTION.md) covers the isolated Unicorn build, private fixture preparation and nine emulator assertions. Generated cards/snapshots may be removed and rebuilt with the tracked tools. Keep firmware and personal projects private.
