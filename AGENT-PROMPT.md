# Prompt for an external agent

Copy the following into an agent opened at this repository's root. Optionally replace the final task selection with a specific task ID.

---

Work on this Digitakt integrated sidecar project. Read AGENTS.md, TASKBOARD.md, the latest PROGRESS.md entry and PLAN.md first, then docs/BASELINE.md. Use those files as durable project context rather than relying on chat history.

The intended instrument is a Digitakt mk1 on OS 1.54 with TRX-B2 synthesized externally on a Raspberry Pi 4B. The Digitakt's audio-track SRC page, sequencer parameter locks and LFOs should control it; returned audio should pass through the track's normal filter, amp and effects sends. First prove one external voice. A MIDI-only external synth with MAIN return is not the goal. The Teensy is a later target.

Select and claim the highest-priority READY task with satisfied dependencies, or work on the task ID assigned by the coordinator. Read its acceptance criteria and relevant source instructions. Execute useful work, not just another plan. Keep scope bounded to an evidence-backed task. Use scripts/bootstrap.py to obtain pinned sources if missing.

Treat hardware measurements, emulator results, inspected source, calculations and hypotheses separately. The inherited two-channel Digitakt return is an Overwitch source-table finding; the physical descriptors and route into a pre-filter track buffer remain unverified. Do not infer Digitakt behavior from Rytm constants.

When blocked, record the exact missing input and unblock condition in TASKBOARD.md, then continue independent READY work. Ask only for information/action that cannot be obtained from the sources or current environment. Read-only hardware inspection may proceed; hardware flashing or internal wiring requires a concrete tested candidate and human involvement under AGENTS.md.

Before finishing: run relevant checks, preserve all dependency edits as replayable patches or accessible pinned commits, update TASKBOARD.md, append PROGRESS.md and make a focused local commit of your work. Report task IDs, artifacts, tests, unresolved blockers and the next actionable task. Coordinate shared files through the task owner; never overwrite another agent's work. Do not push or contact maintainers unless explicitly requested.

Start with the highest-priority available READY task. If all work is blocked, identify the smallest human action that releases it and explain the evidence.
