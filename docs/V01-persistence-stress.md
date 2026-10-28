# V01 — persistence and stress qualification plan

Status: **IN PROGRESS — Phase 0 complete**, claimed by Codex at
`2026-10-28T15:51:11Z`. Phase 0 was offline provenance verification only; no
device was contacted and no audio test ran. Acceptance is not claimed.

Target: Digitakt mk1 OS 1.54, Raspberry Pi 4B/Circle, one USB1 voice. The scope
is the existing I04R one-voice prototype: dense modulation, persistence and
copy/undo semantics, mutes/sound locks, restart/reconnect, and sustained stress.
More voices and the Teensy remain deferred.

## Acceptance boundary

V01 is DONE only when all of the following are evidenced:

1. Dense modulation has a numeric supported budget with responsive UI, no
   audio stop, no deadline/order/overflow fault, and no new FIFO underrun.
2. Save/load, copy/paste, undo, track/trig mutes, and sound locks preserve the
   intended device state and rebuild the host state without stale audio.
3. Digitakt restart, Pi restart, USB reconnect and power-cycle cases either
   recover cleanly or fail closed to silence and require a fresh session.
4. A sustained stress profile passes without unbounded queues, stack/guard
   faults, CPU contention, or silent state loss.
5. The installed candidate, recovery path and reproducible commands are
   recorded; required human listening acceptance is explicit.

Passing host/emulator tests alone do not satisfy the physical cases. Human
listening alone does not replace the instrumentation required to explain a
failure. Unsupported cases must be stated rather than hidden by buffering or
coalescing.

## Evidence classes

| Class | Meaning in this task | Examples |
| --- | --- | --- |
| Physical measurement | Captured from the Digitakt/Pi pair under stated routing and settings | Fault counters, timing traces, USB/FIFO telemetry, recorded audio |
| Emulator result | Compiled or portable code exercised with a model; not a hardware guarantee | Dense queue tests, native worker exchange, persistence fixtures |
| Source fact | Static 1.54 or pinned-source behavior with a revision/address | Render hook order, storage/copy call sites |
| Hypothesis | A proposed cause to test; never acceptance evidence | Worker starvation, buffer underrun, engine overload |

## Established evidence and limits

**Physical measurement:** after the I04 acceptance run, raising LFO speed/amount
(reported for PTCH) stopped TRX-B2 audio and made the Digitakt feel sluggish.
The retained status was fault code 3 (`SC_PF_LATE`) at source block 762393,
260 frames/5.417 ms late; the host disarmed. FIFO underruns later rose from 3
to 29, but that interval also included ordinary listening, so it does not
attribute all 26 new underruns to dense modulation. No OS crash dump exists.
Exact LFO speed, depth, destination values and the failure threshold were not
captured. See [the I04 handoff](../evidence/i04/v01-dense-lfo-handoff.json).

**Physical measurement:** the accepted clean I04 path had zero new playout
faults and zero new FIFO underruns in its timing take. Native-click-to-analog
onset was median 11.417 ms (19 hits), and observed core 3 busy time was at most
3.5% during that short run. This is a baseline, not dense worst-case headroom.
See [I04 timing](../evidence/i04/event-worker-physical-timing.json).

**Emulator result:** the 128-frame event-worker model passed 1000 dense integer
updates and 64 STARTs with ACK loss/recovery. The earlier native stress run hit
source-queue full/overflow once; batching active ACKs made the unchanged test
pass. This proves the modelled queue/cadence, not hardware USB or scheduling.
See [I04 candidate evidence](../evidence/i04/event-worker-candidate.json).

**Emulator/measurement lead:** an earlier fractional-SRC overload showed 128
records covering 84.667 ms of source time but 117.833 ms of requested time,
with worker gaps every 63 records; suppressing engine-ineffective fractional
changes removed that observed physical sluggishness. Dense LFO events must be
checked for a similar rate/backpressure mismatch rather than assumed identical.
See [I04 report](I04-external-trx-voice.md).

**Source fact:** final SRC values are captured at `ev_render_voices`, after
native lock restore/apply, smoothing and LFO. The device clock uses 32-frame
blocks, and the host playout anchor is currently 128 frames/2.667 ms. A late
event is rejected by design; it must not be rebased or silently merged.
See [R04 ordering](R04-control-ordering.md) and
[I03 control link](I03-src-control-link.md).

**Unresolved:** the cause of the dense-LFO failure is not isolated. Candidate
causes remain device render/capture cost, native worker/queue starvation, USB
transport/ACK pacing, Pi scheduling/playout deadline, and engine DSP cost. No
persistence claim yet exists for the USB1 machine's eight SRC values or for
LFO/lock state across the requested edit and restart operations.

## Hypotheses to falsify

- **H1 — device capture is too dense:** each LFO-driven effective change
  produces more records than the device can transmit, filling the queue or
  delaying the worker.
- **H2 — native worker contention:** the worker or USB/receive servicing steals
  enough time from the Digitakt UI/render path to cause the reported
  sluggishness, independently of Pi capacity.
- **H3 — host deadline is too tight:** dense parameter bursts reach the Pi too
  close to the 128-frame playout due time, producing `SC_PF_LATE` even though
  the control path is otherwise correct.
- **H4 — USB transport/ACK pacing:** packet completion, ACK batching or a
  saturated USB-MIDI path delays records before they reach playout.
- **H5 — engine overload:** the TRX-B2 engine plus resampling cannot sustain the
  requested effective update rate on core 3 within the realtime budget.
- **H6 — source-rate artifact:** LFO updates are changing effective engine
  values at a cadence the control contract assumes is slower, rather than
  merely changing the requested LFO settings.

Each hypothesis must be disproved by an experiment that changes one layer while
holding the effective parameter stream and routing constant. A workaround that
only enlarges buffers is diagnostic, not a qualified fix.

## Phase 0 — preflight and preservation

1. Freeze the exact firmware, kernel, patch set and source hashes under test.
   Keep firmware, raw traces, WAVs, projects and snapshots in ignored private
   storage; commit only compact, shareable evidence.
2. Preserve `PLAN.md` and any other agents' edits. Do not rewrite existing I04
   evidence to make V01 look complete.
3. Confirm the recovery assets from I04 remain present: installed I04R, previous
   I04Q/H02F, stock 1.54 firmware, and the card rollback image/manifest. A new
   candidate gets a fresh directory and its own verified recovery record.
4. Do not modify `deps/` without a replayable patch under `patches/` at the
   pinned base revision. Do not commit private firmware or user data.

**Hardware release condition:** the human must confirm the Pi and Digitakt are
powered/available, speakers or headphones are lowered, a disposable project and
backups are ready, routing is known, and a paired candidate/recovery has been
reviewed. Until then V01 continues with independent source, host and emulator
work only.

### Phase 0 result — PASS (2026-10-28T15:57:51Z)

[Preflight record](../evidence/v01/phase0-preflight.json), reproducible with
`python3 tools/v01_phase0.py --output evidence/v01/phase0-preflight.json`.

| Check | Result |
| --- | --- |
| Installed Digitakt firmware | I04R SHA256 `6a04ae9…f7fa2b`, corroborated by the separate human-installation and SD-deployment records |
| Installed Pi kernel | SHA256 `c057f06…4a0f2`, 2474600 bytes, 128-frame playout |
| Frozen implementation sources | 50/50 hashes match `evidence/i04/event-worker-candidate.json` |
| Dependency patches | Ten patches replay at `sidecar-firmware` `6cff67b…a0457`; replay script passes |
| Dependency pins | All five `sources.json` revisions match; only the expected `sidecar-firmware` overlay is dirty |
| Firmware recovery | I04R, I04Q, H02F and stock 1.54 `.syx` files present and hash-verified |
| Pi recovery | Installed rollback `0c544fb…070ed` and pre-I04 `a46476a…28a82` images present and hash-verified; rollback `SHA256SUMS` present |
| Private assets | Firmware/kernel artifacts ignored; zero private or firmware/audio binaries tracked |

**Limits recorded by Phase 0:** the local full-SD backup is represented by its
rollback kernel and manifest, not an enumerated card image. The private I04R
manifest's `scope` still says “not installed”; installation is established by
the separate human-installation/deployment evidence, not that stale label.
Phase 0 did not resolve the dense-LFO bottleneck, persistence locations or
supported modulation budget.

**Release still required:** human confirmation of Pi/Digitakt availability,
lowered speakers or headphones, a disposable project with backups, known
routing, and reviewed paired candidate/recovery. No hardware action occurred.

## Phase 1 — reproduce the dense-LFO failure

Use a disposable USB1 track-1 project and a quiet, limited/faded-in tone. Start
muted/silent, then raise the monitor level only after the control session is
healthy. The installed `trxtrace` forces mute and retains only 128 admissions;
run paired audible and trace passes with the same settings instead of treating
them as one run. Record status before dumping the trace, because printing can
stall output. Use analyzer delay 128 for the installed buffer.

Pre-register the sweep in device units rather than vague labels. Start from the
accepted slow/shallow PTCH case and sweep PTCH LFO speed and depth in 0–127
steps (at least four speeds by four depths), then repeat the worst passing and
first failing settings. For every row record:

- Digitakt LFO destination, speed, depth, waveform, trigger mode and track;
- exact firmware/kernel hashes, routing, pattern, source note and velocity;
- effective parameter changes per second, blocks between records, queue depth,
  worker/service cadence, ACKs, FIFO, fault code and retained fault fields;
- Pi requested/rendered frame clocks, playout queue and minimum slack, control
  pass time, core 3 CPU, dropped blocks and audio continuity;
- human observation of audio and UI responsiveness, with the moment of failure;
- whether recovery required a fresh session or power cycle.

Stop a case at the first audio stop, fault or visible UI stall. Preserve the
trace and exact settings; do not continue into unrelated listening. A failure
without a frozen trace is a useful lead, not a qualified budget point.

The executable protocol is [V01 Phase 1 runbook](V01-phase1-runbook.md). It
records the critical installed-trace constraint: `trxtrace=on` forces the voice
to mute, so audible and trace passes are paired rather than conflated. The runbook
also defines the numeric grid, required telemetry, stop rules and the physical
release gate. It is prepared but **not executed** as of
`2026-10-28T15:59:00Z`.

## Phase 2 — isolate the bottleneck

Build a timeline where device block timestamps, worker writes/ACKs, USB events
and Pi enqueue/render deadlines can be compared without treating unrelated
clocks as synchronized. Then test the layers independently:

1. **Device capture:** compare effective changes with captured records. Check
   whether the source queue fills, whether capture cadence changes with LFO
   settings, and whether the worker stops or merely delivers late.
2. **Device UI/service:** measure UI tick continuity, worker service gaps,
   stack guards and native completion progress under the same effective stream.
   This separates H2 from H4.
3. **Host playout:** calculate due-frame slack and queue peak for every event;
   vary delay and worker batch cadence only as diagnostic controls to identify
   where lateness is introduced. Do not adopt a larger delay as the fix until
   the cause is known.
4. **Transport:** compare USB packet/completion progress, MIDI RX/TX and ACK
   cadence with the effective update stream. Repeat with a synthetic dense
   control stream of known cadence to distinguish USB pacing from LFO capture.
5. **Engine:** feed the same effective parameter cadence directly to the
   production TRX-B2 path on the Pi, bypassing USB/control, and measure render
   time and deadline margin. If this fails alone, H5 is supported; if it passes,
   the fault is upstream or in scheduling.

Record negative controls: slow LFO with identical audio/UI load, dense
engine-ineffective changes, dense effective changes, and the same run after a
fresh reconnect. Exact source revisions and commands belong with each result.

## Phase 3 — fix and qualify a modulation budget

Fix the root cause at the responsible layer while preserving fail-closed
deadlines, ordering, START identity and immediate handshake/ACK behavior. No
audio-callback allocation, USB wait, unbounded backlog, coalescing of meaningful
changes, or deadline rebasing.

If rate limiting is required, it must be based on semantic equivalence: an
update may be suppressed only when it cannot change engine output, and the
reason must be counted. Unsupported density must produce silence or a clean
reconnect, never stale audio or a hung UI.

The supported budget is a table of numeric settings that passed the physical
qualification profile, not a single knob position. Its minimum evidence is:

- 10 minutes continuous at each supported dense profile, with zero new playout
  faults and zero new FIFO underruns beyond the recorded startup baseline;
- bounded source/playout queues and intact stack guards throughout;
- no audio stop, no dropped/inserted blocks, no stale note, and acceptable UI
  responsiveness by human observation plus an instrumented UI/service metric;
- at least one just-above-budget setting that fails closed cleanly and recovers
  with a fresh session. This documents the boundary rather than implying it is
  unlimited.

The UI threshold and exact soak durations must be frozen after Phase 2 produces
the clean baseline and before testing the fix candidate. Changing them after a
failure requires a recorded rationale.

## Phase 4 — persistence and edit semantics

Do not create a private sidecar database as the default. First establish which
USB1 state is already stored by the Digitakt's native machine/sound/pattern
path, and let the device remain authoritative. The host should rebuild its
transient session and engine state from the ordered control stream.

Use read-only source/emulator inspection to locate 1.54 storage and copy paths,
then verify behavior on a disposable project and pattern. The minimum matrix is:

| Case | What must be checked |
| --- | --- |
| Save project, power cycle, reload | USB1 machine identity, eight SRC values, LFO settings, filter/AMP/sends and sound-lock references restore; host silences until a new session activates |
| Save/load sound or kit | SRC values/defaults and relevant sound-owned state travel correctly; no stale host values override loaded state |
| Copy/paste track, sound and pattern | Copied state is complete and independent; destination starts a clean voice/session where required |
| Undo | Restores the pre-edit values without leaving a queued event that later overwrites the restored state |
| Track mute/unmute and trig mutes | Muted audio is silent, unmute does not replay stale PCM, and neighboring tracks keep working |
| Sound locks on steps | Locked sound values are applied at the correct render point; switching back restores the prior sound; host voice identity is not confused with sound identity |
| Load/switch while playing | Old session/queue is invalidated, no old audio survives, and a fresh handshake is required |
| Copy/load into another track | Unsupported tracks fail closed; no state is silently applied to track 1 or another voice |

Each case needs at least: pre-edit state, edit, save/copy, restart/load, observed
values/PCM, and whether the device marked the project modified. If 1.54 does not
persist a required field, record the exact missing field and either fix it with
a replayable patch or mark it unsupported; do not claim save/load from a
successful control-link reconnect alone.

## Phase 5 — restart, reconnect and sustained stress

Run these as separate cases so one recovery does not hide another failure:

- USB cable pull/replug with audible PCM already playing (minimum ten cycles);
- Pi reboot and fresh process start (at least three cycles);
- Digitakt project/pattern restart and machine reselection;
- Digitakt restart and power cycle only after backing up the disposable state;
- dense LFO plus note starts, locks, mute toggles and sound-lock changes;
- save/load and undo between runs, not only while idle.

Every reconnect must produce silence until a fresh nonzero session, the first
START, and matching ACTIVATE; no old sequence, ACK, note or PCM may cross it.
Recovery must not require a hidden coalescing rule or a stale snapshot.

A sustained run combines the supported dense profile with sequencer starts and
UI edits for at least the accepted H02-style ten-minute window. Record start and
end counters rather than only final values, and restart the interval after any
fault rather than averaging across it. A shorter human-accepted run may be
reported as such, but it does not establish the full soak claim.

## Phase 6 — candidate, hardware gate and report

Only after host/emulator evidence and a reviewed diff:

1. Build a fresh candidate with source/patch hashes, protected-section checks,
   native/UI/exchange probes and the full existing regression set.
2. Prepare the paired Pi kernel and recovery artifacts in private storage.
   Record the exact human flash/SD steps and rollback path.
3. Ask the human to lower speakers and confirm availability. Start with Pi
   muted and silence, then use a quiet, limited/faded-in tone. No bootloader or
   wiring changes.
4. Run the pre-registered physical matrix, stop on faults, preserve private
   traces, and restore the original channel/bit-depth/prefix settings.
5. Update this report with measured results, emulator/source facts, unresolved
   limits and the exact next action. Update `TASKBOARD.md` and keep
   `PROGRESS.md` a short handoff. Link compact evidence only.

## Immediate next action

Complete Phase 0 inventory and static persistence inspection, then draft the
numeric dense-LFO sweep and pre-register the UI/soak thresholds. Do not contact
hardware until the human confirms the Phase 0 release condition and the paired
candidate/recovery is ready.

## Open release conditions

- **Hardware:** human availability, lowered speakers, disposable project,
  backups and reviewed candidate/recovery.
- **Persistence:** establish the actual 1.54 storage/load/copy locations for
  USB1 values and LFO/lock behavior before claiming save/load or copy/paste.
- **Dense modulation:** reproduce with exact numeric settings and a private
  trace; isolate the layer before choosing a fix.
- **Dependency changes:** keep each as a replayable patch at
  `sidecar-firmware` `6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457` or an
  authorized accessible pinned commit; dirty ignored clones are not
  deliverables.
