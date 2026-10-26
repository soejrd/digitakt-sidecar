# Digitakt integrated sidecar

Research and prototype: run TRX-B2 on a Raspberry Pi 4B, selected and controlled from a Digitakt mk1 audio track, with returned audio passing through that track's filter, amp and effects sends.

Target: **Digitakt mk1, OS 1.54; Raspberry Pi 4B**. A Teensy 4.1 is available for later evaluation. First milestone: one external voice on one existing audio track. Feasibility remains unproven.

## Start here

1. Read [AGENTS.md](AGENTS.md).
2. Use [TASKBOARD.md](TASKBOARD.md) to choose work; it is authoritative for status, ownership, dependencies and blockers.
3. Read [PLAN.md](PLAN.md) for architecture and acceptance gates; [PROGRESS.md](PROGRESS.md) records completed sessions.
4. Give an external agent [AGENT-PROMPT.md](AGENT-PROMPT.md).

## Set up sources

Python 3 and Git are sufficient to obtain pinned dependencies:

```sh
python3 scripts/bootstrap.py
```

To reuse existing local clones, pass their common directory (missing clones fall back to their public remote):

```sh
python3 scripts/bootstrap.py --source-root /path/to/existing/clones
```

Clones go in ignored `deps/`. Existing clones are checked for the pinned revision and never reset. This does not fetch engine build dependencies or install toolchains. Read each dependency's instructions before changing or building it. Baseline test: `bash deps/sidecar-firmware/tools/run_portable_tests.sh`.

This is a standalone local Git repository. The previous exploration folder is retained as historical context; this repo is the current handoff. Dependencies and private firmware are not committed here. Source pins are in [sources.json](sources.json); prior findings are in [docs/BASELINE.md](docs/BASELINE.md).
