# Working agreement

At session start read `TASKBOARD.md`, the latest entry in `PROGRESS.md`, and the relevant phase in `PLAN.md`. Read `docs/BASELINE.md` before interpreting inherited findings. Read a dependency's own AGENTS/contribution instructions before modifying it.

For control mapping, host automation, USB transport, project persistence or sample transfers, consult `docs/REFERENCES.md` for relevant sources and their review status.

## Task workflow

1. Select a READY task whose dependencies are DONE, or an independent research task explicitly listed as ready. Claim it in TASKBOARD with a unique owner and UTC timestamp before edits. Work on one coherent task per session unless completing it unblocks a small direct follow-up.
2. Inspect existing work and Git status. Preserve other agents' edits. An IN_PROGRESS claim belongs to its owner; ask the coordinator before taking it over. On separate clones, the coordinator assigns non-overlapping task IDs and merges board changes; local claims alone are not a distributed lock.
3. Produce the task's artifact and acceptance evidence. Separate physical measurements, emulator results, source inspection, calculations and hypotheses. Cite source revision and file/symbol or measurement procedure.
4. Update the task status and evidence links. For BLOCKED, state the exact missing input, who can supply it, and what releases it. Continue another READY task when practical; do not invent hardware results or substitute a smaller product goal.
5. Append a dated PROGRESS entry with task IDs, changes, commands/results, limitations, remaining blockers and next action. Commit only your related tracked changes after reviewing the staged diff. Report the commit and handoff; do not push, publish or message maintainers unless requested.

Status meanings and ownership live only in TASKBOARD. PROGRESS is an append-only session history, not a second task list. Change the plan when evidence requires it; record the decision and affected task IDs.

## Project constraints

- Deliver integrated audio-track machines: SRC controls, sequencer locks/LFO behavior and returned audio through track processing. A dedicated MIDI sequencer track plus MAIN return is a diagnostic, not completion. USB MIDI/SysEx may still carry internal control events.
- Confirmed hardware is Digitakt mk1 OS 1.54 and Pi 4B. Treat USB descriptors, actual channel counts, receive-buffer accessibility and timing as unmeasured until evidence exists. Two return channels are currently a source-table finding, not a hardware capture.
- Keep engine/host code separate from firmware-integration research. The creator's stock-only guide does not authorize a Digitakt firmware port inside their clean-room repo. Record provenance; keep derived research out of upstream-bound contributions unless compatible with that repo's rules.
- Keep Elektron firmware, extracted code/data, dumps, snapshots, private projects and credentials outside tracked files. Use private ignored paths if needed. Do not change the bootloader. Prepare and verify a concrete firmware candidate and recovery procedure before asking the human to perform a hardware flash. This handoff does not authorize physical flashes or internal wiring.
- Start new hardware audio paths with silence, then a quiet tone and fade-in/limited level; ask the human to lower speakers before sending audio. Read-only descriptor capture may proceed when hardware is available.
- Preserve TRX-B2's 44.1 kHz behavior; resample to the measured stream rate. Keep real-time callbacks bounded, with no allocation or waiting on USB.

## Dependencies and evidence

`sources.json` pins public source revisions. `deps/` contains disposable clones, not deliverables. Preserve dependency modifications as reviewed Git patches in `patches/<dependency>/` with base revision and replay instructions, or update the manifest to an accessible committed revision authorized by the human. A dirty ignored dependency alone is not a completed handoff. New project code belongs in tracked `src/`, `tools/` or `tests/` as appropriate.

Save reproducible commands and compact shareable evidence in `evidence/`. Keep hardware serial numbers and personal paths out of committed logs. Existing sanitized baseline logs are historical tests, not a new run in this repository.
