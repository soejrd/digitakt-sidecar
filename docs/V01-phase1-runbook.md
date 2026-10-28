# V01 Phase 1 — dense-LFO reproduction runbook

Status: **PREPARED — NOT EXECUTED**. This runbook is the pre-registered Phase 1
procedure. It does not claim a physical result, a supported modulation budget or
a root cause. Hardware starts only after the release gate below is explicitly
confirmed and recorded.

Target: installed Digitakt mk1 OS 1.54 I04R and Pi 4B kernel
`c057f063c24770a520b842e66d4d7e10655bdda3d45de05e1b67edc85d34a0f2` from
[Phase 0](../evidence/v01/phase0-preflight.json). One USB1 voice on audio track
1. The project, pattern and recorder state must be disposable.

## Release gate and safety

Do not begin until all of these are true:

- The human confirms Pi and Digitakt are available and the audio path is known.
- Speakers or headphones are lowered; monitoring starts muted or at a safe low
  level. The first audible case is a quiet, limited/faded-in tone, never a
  sudden full-level return.
- A disposable project/pattern and a backup of anything the human wants to keep
  are ready. The existing I04R, I04Q, H02F and stock recovery assets remain
  present and unchanged.
- The human has reviewed this runbook and the paired candidate/recovery state.
  No firmware or SD change is expected for Phase 1.
- No agent flashes firmware, changes the bootloader/wiring, or operates the
  computer UI on the human's behalf.

If any condition is missing, stop at the offline preparation step and record the
exact missing input. Do not substitute a simulated or MIDI-only run for the
physical failure.

## Known instrumentation constraint

The installed `trxtrace=on` command calls `VoiceCommand(false)`: **enabling the
timing trace forces the voice muted and cannot produce an audible failure run**.
The ring retains only 128 admissions and freezes on the first rejected event.
Therefore Phase 1 uses paired passes:

1. **Audible pass:** trace off, voice explicitly armed after a healthy session;
   capture status/counters and human observations.
2. **Trace pass:** same project, pattern, note/velocity and LFO settings; enable
   trace while muted, reproduce the same sweep, then read status before dumping
   the ring. This pass explains the admission/deadline boundary but does not
   claim audible output.

Never compare an audible pass to a trace pass as if they were one run. Record
the pairing explicitly, and do not call a trace-only failure an audio stop.

## Preflight snapshot

Before the first case, record:

- UTC time, user/operator, firmware and kernel hashes, Pi uptime/build;
- `/status`: link, streaming, output mode, FIFO target and underrun counter,
  recorder settings and temperature;
- `trxstatus`: armed state, queue, peak, faults, applied/start/consumed counters
  and the retained fault record;
- the complete Digitakt LFO page values for the selected track: destination
  (`PTCH`), speed (`SPD`), multiplier (`MULT`), fade (`FADE`), waveform
  (`WAVE`), start phase (`SPH`), mode (`MODE`), depth (`DEP`), plus trig mode;
- project/pattern/sound names, track and any sound lock, source note/velocity,
  routing and recorder channel map;
- a baseline listening check with the voice muted, then a quiet limited tone.

Collect status before `trxtrace` because dumping 128 rows can stall output. Use
`tools/i04_analyze_trace.py --delay 128` for the installed buffer. Keep the raw
trace and recorder files private; publish only compact timing metadata.

## Numeric sweep

Units are the Digitakt's displayed `0–127` values for `SPD` and `DEP`; do not
translate them into assumed Hz or decibels. `MULT`, `FADE`, `WAVE`, `SPH` and
`MODE` are held at the accepted baseline unless a row explicitly says otherwise.
The human changes the LFO page and reports the displayed values; the operator
captures a screenshot or exact on-screen values if available. Never infer a
value from a knob position after the fact.

Start from the accepted slow/shallow PTCH LFO case. The historical acceptance
record does not contain its numeric `SPD`/`DEP`, so the first work item is to
capture that accepted setting rather than invent one.

Pre-register this initial grid (four speeds x four depths = 16 rows):

| Row class | SPD | DEP | Purpose |
| --- | --- | --- | --- |
| Baseline | accepted slow value | accepted shallow value | Confirm clean, responsive reference |
| S1 | 32 | 32 | Low-speed/reference control |
| S2 | 64 | 64 | Mid-speed/mid-depth |
| S3 | 96 | 96 | High-speed/high-depth |
| S4 | 127 | 127 | Upper-bound sweep |

For every row, use the same note/velocity and at least 10 seconds of held or
repeated playback after the control session is healthy. If the first failure
occurs, stop immediately. Then repeat the last passing and first failing row
individually to distinguish a boundary from a transient. If `SPD`/`DEP` at the
accepted baseline differ from the table, record the actual pair and use it as
the baseline; do not rewrite history.

For each row capture:

| Field | Required value |
| --- | --- |
| Settings | `SPD`, `DEP`, `MULT`, `FADE`, `WAVE`, `SPH`, `MODE`, destination `PTCH`, trig mode |
| Input | Project/pattern, track, sound/lock, note, velocity, routing |
| Runtime | Firmware/kernel hashes, uptime, build, session state |
| Device counters | Start/end FIFO underruns, queue/peak, faults, applied, starts, consumed |
| Host counters | `trxstatus` before/after, retained fault code/block/flags/delta/now/due |
| Trace | Paired trace-pass count/frozen state and compact analyzer output |
| Transport | USB packet gaps/errors, MIDI bytes/loss, dropped messages if available |
| Performance | Engine block max, CPU core 3, control-pass max, temperature |
| Audio | Stop/continuity, clicks, stale notes, silence, human observation timing |
| UI | Responsive, sluggish, frozen, or recovered; exact observed moment |
| Recovery | Fresh session required, reconnect, restart, or power cycle |

## Audible-pass protocol

For each row, in order:

1. Confirm the voice is muted and the session is active with no new fault.
2. Capture baseline `trxstatus` and `/status` counters.
3. Set the exact LFO row values while muted; confirm the displayed values.
4. Start the same quiet pattern and fade the monitor in to a safe level.
5. Hold/loop for at least 10 seconds; the human reports audio continuity and UI
   responsiveness without changing other controls.
6. Stop at the first audio stop, fault, visible UI stall or deadline failure.
7. Mute immediately; capture end counters and the exact failure moment.
8. Preserve the private log/recording and record whether the session/voice must
   be rearmed or the Pi/Digitakt restarted.

Do not continue a failing row to "see what happens", and do not keep adjusting
LFO depth after a failure. A failure is evidence of a boundary, not a tuning
exercise.

## Trace-pass protocol

After an audible row passes or fails, repeat it with trace enabled only when the
operator can keep the voice muted and the same effective settings can be
reproduced:

1. `trxtrace=off` then `trx=off`; confirm muted and record status.
2. Set the same LFO values and note/velocity; `trxtrace=on`.
3. Run the same duration/pattern. The voice remains muted by design.
4. `trxstatus` before `trxtrace`; then dump the ring.
5. Analyze with delay 128 and retain the compact result. If the ring did not
   freeze on a rejection, mark the trace as non-decisive rather than claiming a
   boundary.

For a first pass/fail boundary, trace the last-passing and first-failing row. If
the installed ring cannot hold the interval, record that limitation and propose
a larger bounded trace as a separate diagnostic patch; do not silently increase
the buffer as a fix.

## Decision rules

- **Pass:** no audio stop, no new FIFO underruns, no fault, no deadline/order/
  overflow fault, responsive UI, and a decisive or explicitly non-decisive trace.
- **Fail:** any audio stop, fault, queue overflow, UI freeze/sluggishness, or
  stale note. Stop the row and preserve counters/trace.
- **Inconclusive:** human reports a stop but counters/trace are absent, or the
  audible and trace settings cannot be shown to match. Record the missing input;
  do not label it a supported/unsupported budget point.
- **Root-cause claim:** only after a Phase 2 layer isolation. A late parameter
  event alone does not establish whether device capture, worker contention, USB
  pacing, host deadline or engine load caused it.

## Phase 1 exit record

Write a compact `evidence/v01/phase1-reproduction.json` containing:

- release-gate confirmation and exact installed artifacts;
- baseline accepted numeric setting (or the reason it could not be recovered);
- every row's settings, pass/fail/inconclusive status and counters;
- first-failing and last-passing boundaries, with trace pairing;
- human observations clearly marked as human-reported;
- hardware/emulator/source class for every claim;
- recovery actions and final safe state (muted, recorder restored, voice off);
- limitations and the Phase 2 hypothesis to test next.

No supported dense-modulation budget is claimed until Phase 3 completes the
soak and fail-closed boundary checks.
