# Digitakt integrated sidecar

Run TRX-B2 externally on a Pi 4B, controlled from a Digitakt mk1 OS 1.54 audio track, with audio returned through that track's processing.

> **Work in progress.** This is an early experimental prototype, not a working product. Nothing has been flashed to a Digitakt, and hardware integration remains unproven. What passes today is emulator and host-level work: see [Progress](PROGRESS.md) for exactly what is and is not established. Expect breakage, rework and gaps in the docs.

## Credits and origin

This project is based on **Sidecar** by [Trig Condition](https://github.com/trigcondition), the Analog Rytm MKII voice and effects expander. Their work is where this started, and their [Digitakt guide](https://github.com/trigcondition/sidecar-expander/blob/main/src/content/docs/developers/digitakt.md) is the reference for the stock-firmware USB experiment. Many thanks to them for publishing it in the open.

- [sidecar-firmware](https://github.com/trigcondition/sidecar-firmware) and [sidecar-mods](https://github.com/trigcondition/sidecar-mods) by Trig Condition: the Sidecar engine, USB transport and Rytm-side integration that this builds on (active v0.2.0 pin; the ported v0.3.0 upgrade target is recorded in `sources.json`).
- [digiemu](https://github.com/irpina/digiemu) by irpina: the Digitakt emulator used for testing.
- [elekloader](https://github.com/irpina/elekloader) by irpina: the mod loader used to build OS candidates.

What this repository adds on top is Digitakt-specific: deeper firmware integration (source replacement before a track's filter/amp) and the research, tooling and evidence around it. It is not affiliated with or endorsed by Trig Condition or Elektron. Elektron firmware is not included; you supply your own.

## Project

- [Task board](TASKBOARD.md): work, ownership and blockers. **U01 (v0.3.0 migration) is highest priority.**
- [Progress](PROGRESS.md): established results and remaining uncertainty.
- [Plan](PLAN.md): goal, architecture and acceptance gates.
- [Agent instructions](AGENTS.md) and [copyable prompt](AGENT-PROMPT.md).
- [References](docs/REFERENCES.md): sources organized by project use.
- [v0.3.0 migration](docs/UPGRADE-v0.3.0.md): new-base patch series, replay commands and open cross-build/hardware gates.

## Setup and checks

```sh
python3 scripts/bootstrap.py
# Or reuse existing clones: --source-root /path/to/clones
```

sources.json pins the active dependency revisions. Clones in deps/ are ignored. The historical v0.2 series is in [patches/sidecar-firmware](patches/sidecar-firmware/README.md); the ported v0.3.0 target is in [patches/sidecar-firmware-v0.3.0](patches/sidecar-firmware-v0.3.0/README.md).

The v0.3.0 commit is already available in the normal dependency checkout here.
On a fresh clone, fetch tags first (`git -C deps/sidecar-firmware fetch --tags`).

```sh
python3 scripts/prepare_v030_source.py --output /tmp/dtsc-v030
python3 scripts/test_v030_replay.py
```

The v0.3.0 series is desktop-replayed only; the Pi 4 cross-build and physical requalification remain open. For the historical v0.2 I01 check, replay [its patch](patches/sidecar-firmware/README.md) before running `./scripts/test_i01.sh`.

The [R03 reproduction guide](docs/R03-REPRODUCTION.md) covers the isolated Unicorn build, private fixture preparation and nine emulator assertions. Generated cards/snapshots may be removed and rebuilt with the tracked tools. Keep firmware and personal projects private.
