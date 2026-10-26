# Agent instructions

Read TASKBOARD.md, PROGRESS.md and PLAN.md before work. TASKBOARD owns task status, dependencies and claims; PROGRESS is a short current handoff. Consult docs/REFERENCES.md for relevant external sources and each dependency's own instructions before modifying it.

## Workflow

1. Claim one READY task with your name and UTC timestamp. Preserve existing edits and other agents' claims. Across separate clones, the coordinator assigns non-overlapping IDs; local claims are not a distributed lock.
2. Deliver its acceptance evidence. Distinguish hardware measurements, emulator results, source facts and hypotheses. Cite revisions and reproducible commands.
3. If blocked, record the missing input and release condition, then continue independent READY work. Do not replace integrated-machine work with a MIDI-only demo.
4. Update the task board and the existing task report. Keep PROGRESS concise; Git history preserves previous sessions. Remove superseded troubleshooting notes once final evidence replaces them.
5. Review the diff, run relevant checks and commit only your work. Report results, limitations and next task. Push/publish/contact others only when requested.

## Technical boundaries

- Target: Digitakt mk1 OS 1.54 + Pi 4B; Teensy later. R02 captured USB endpoint capacities; channel semantics/rate still need a live stream. R03 passes on the 1.53 emulator, not 1.54 hardware.
- Keep host/engine code separate from firmware-derived research and respect dependency contribution rules. Keep Elektron firmware, extracted data, dumps, snapshots and personal projects in ignored private storage. Preserve original user files; generated fixtures are disposable.
- Prepare a verified candidate and recovery procedure for a human hardware flash. Do not alter the bootloader or internal wiring. Before new hardware audio tests, ask the human to lower speakers; start with silence and a quiet, limited/faded-in tone.
- Preserve TRX-B2's native 44.1 kHz behavior; resample to the measured device rate. Audio callbacks must not allocate or wait for USB.
- deps/ contains ignored clones pinned by sources.json. Preserve changes as replayable patches with base revision under patches/, or an authorized accessible pinned commit. Dirty ignored clones are not deliverables.
- Store compact shareable evidence without personal paths or serial numbers. Keep passing proofs and useful negative findings; avoid duplicate logs, copied upstream manuals and repeated status documents.
