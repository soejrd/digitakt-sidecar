# Digitakt integrated sidecar

Run TRX-B2 externally on a Pi 4B, controlled from a Digitakt mk1 OS 1.54 audio track, with audio returned through that track's processing.

> **Work in progress.** This is an early experimental prototype, not a working product. Nothing has been flashed to a Digitakt, and hardware integration remains unproven. What passes today is emulator and host-level work: see [Progress](PROGRESS.md) for exactly what is and is not established. Expect breakage, rework and gaps in the docs.

## Credits and origin

This project is based on **Sidecar** by [Trig Condition](https://github.com/trigcondition), the Analog Rytm MKII voice and effects expander. Their work is where this started, and their [Digitakt guide](https://github.com/trigcondition/sidecar-expander/blob/main/src/content/docs/developers/digitakt.md) is the reference for the stock-firmware USB experiment. Many thanks to them for publishing it in the open.

- [sidecar-firmware](https://github.com/trigcondition/sidecar-firmware) and [sidecar-mods](https://github.com/trigcondition/sidecar-mods) by Trig Condition: the Sidecar engine, USB transport and Rytm-side integration that this builds on (pinned at v0.2.0 in `sources.json`).
- [digiemu](https://github.com/irpina/digiemu) by irpina: the Digitakt emulator used for testing.
- [elekloader](https://github.com/irpina/elekloader) by irpina: the mod loader used to build OS candidates.

What this repository adds on top is Digitakt-specific: deeper firmware integration (source replacement before a track's filter/amp) and the research, tooling and evidence around it. It is not affiliated with or endorsed by Trig Condition or Elektron. Elektron firmware is not included; you supply your own.

## Project

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
