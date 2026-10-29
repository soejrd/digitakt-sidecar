# V01 — one-voice qualification

**IN PROGRESS.** Ownership renewed by Codex at `2026-10-28T18:41:42Z`.
Phase 0 preservation passed; Phase 1 reproduced the dense-LFO failure.
Phase 2 is incomplete. V01P is rejected; V01N is physically untested.
V01 is not accepted. F was installed and reproduced the lag; G compact full-snapshot pair is verified/staged offline, hardware untested.

Target remains Digitakt mk1 OS 1.54 + Pi 4B/Circle, one USB1 voice on track 1.
More voices, Teensy and a separate persistence database remain outside scope.
The user's local `PLAN.md` edits are preserved.

## Plan review and correction

The acceptance areas were appropriate, but the old six-phase sequence mixed
investigation with qualification and repeated candidate/hardware gates. It also
had stale next actions and unnecessary requirements for a full knob grid,
ten-minute testing at every point, and a deliberately failing above-budget
setting. Use the three gates below instead. Keep short controls for diagnosis;
reserve the long run for the final supported configuration.

**Critical diagnostic correction:** V01D/N/P time displays use
`(blocks * 1000 + 15) / 32`. But `usb_track_clock` increments once per
32-frame audio render, not once per 32 Hz UI tick. At 48 kHz it runs at
1500 blocks/s. The old displayed milliseconds are inflated by 46.875×.
[Correction and regression](../evidence/v01/diagnostic-clock-correction.json).

| Original display | Approximate duration at 48 kHz |
| --- | --- |
| V01D control UGAP 1625 | 34.7 ms |
| V01D dense UGAP 7781 / 8469 | 166.0 / 180.7 ms |
| V01D dense WGAP 562 | 12.0 ms |
| V01D dense BATCH 94 | 2.0 ms |
| V01P dense BATCH 969 | 20.7 ms elapsed, including preemption |

These are reinterpretations of rounded display values, not new measurements.
The original JSON observations are retained unchanged; their `_ms` fields and
multi-second-counter interpretations are superseded by this correction.
The human-reported multi-second Save/Play/Stop lag remains evidence of a real
failure. A render-clock metric cannot measure wall time while rendering itself
stops; worker batch elapsed time is not worker CPU consumption. Maxima since
boot include startup and cannot locate a stall within a trial.

The diagnosis is therefore **device-side responsiveness failure with scheduling
contention suspected**, not proven multi-second UI-task starvation. V01P's
responsive UI and growing late-event/FIFO counters remain a valid negative
result. Pi faults were absent in the particular muted V01D trial; this does not
rule out transport or Pi scheduling in other trials.

## Evidence retained

| Evidence | What it establishes |
| --- | --- |
| [Phase 0](../evidence/v01/phase0-preflight.json) | Original source/pin/recovery preservation passed; historical inventory, not current installed-state verification |
| [Phase 1](../evidence/v01/phase1-reproduction.json) | 48/35 and 32/32 short controls passed; 64/64 lagged even muted; no Pi faults during that interval |
| [Phase 1 trace](../evidence/v01/phase1-s2-64-64-trace.json) | 128 accepted rows over ~133 ms; cannot explain the entire stall |
| [V01D trial](../evidence/v01/phase2-device-diagnostic.json) | Human observations and raw display readings; time labels corrected above |
| [V01P trial](../evidence/v01/phase2-v01p-result.json) | Priority 5 restores perceived UI response but fails timely playout; rejected |
| [V01N model](../evidence/v01/phase2-exchange-noself.json) | No-self-wake compiled exchange passes; no physical scheduling guarantee |

Physical observations, emulator results, source facts and hypotheses must stay
separate. Do not overwrite old evidence to make a new candidate look tested.

## Gate A — resolve dense-modulation failure

**Offline preparation complete:** corrected, interval-scoped diagnostic
candidates V01B (priority-14/self-wake baseline) and V01C (same priority, no
self-wake) build/boot and pass panel, reset, unit-conversion, native-task and
compiled-exchange checks. The full host regression and ten-patch replay pass.
Verified recovery packages are staged; both candidates have since had physical diagnostic trials.
[Candidate evidence](../evidence/v01/gate-a-candidates.json),
[physical runbook](V01-gate-a-runbook.md).

Selecting another machine then USB1 starts a new diagnostic window. UI/worker
maxima exclude the previous window; guard faults stay sticky. SECS allows a
stopwatch comparison before interpreting render-clock durations. Old V01N
artifacts retain the wrong units; use the rebuilt V01C. Do not call this
instrumentation change a scheduling fix. Do not print a trace while measuring:
`trxtrace` dumps previously caused FIFO growth.

Ranked hypotheses for the next controlled investigation (not findings):

1. **High-priority readiness/worker contention.** If repeated self-posts starve
   stock work, removing them at the same priority should improve UI service
   while preserving delivery slack. Static native inspection shows the wait
   clears all pending posts, ruling out one-wait-per-old-credit accumulation.
   Continuous producer signals may still prevent a sleep in either variant.
2. **Native serializer/USB critical-section pressure.** If interrupt-masked
   sends or USB backpressure dominate, capture-to-submit/completion age should
   grow with dense effective changes even when self-posts are removed.
3. **Stock UI/save/render interaction.** If the UI's own processing or disrupted
   render cadence dominates, wall-time stalls will persist despite bounded
   worker readiness; render-block timing may under-report them.
4. **Host scheduling/deadline margin.** If events arrive promptly but are applied
   late, compare arrival and due-frame slack with Pi control/render cost. V01P
   makes this relevant, but does not isolate it.

Use the existing 32/32 control and 64/64 failing case with all other LFO settings,
note/velocity, routing, project and kernel held constant. Start muted. Compare
corrected baseline with corrected no-self-wake, changing only that behavior.
Read counters before/after a short interval; stop on lag, fault or audio failure.
Do not repeat a broad sweep while this known failure is unexplained. A failed
V01N result ends that hypothesis trial; choose the next probe from its evidence.
Do not cycle arbitrary priorities or enlarge latency buffers to obtain a pass.

The current emulator tests can catch ordering, protocol and queue failures.
They **cannot reproduce the exact hardware responsiveness failure**: native
exchange synthesizes USB completions and has no dense-run UI latency assertion;
the host test models worker wake as a boolean, not native semaphore scheduling.
A hardware trial is presently the red-capable loop for that symptom. Add an
emulator regression only where it exercises the mechanism actually found.

After identifying a cause, fix that layer and rerun the same failing/control
pair. Preserve ordered START identity, fail-closed deadlines, bounded queues,
task-only USB and allocation-free/nonblocking audio callbacks. Any deliberate
change to control cadence, latency or event semantics needs an explicit contract
and audio/ordering evidence; it is not an incidental optimization.

**Exit:** one paired candidate passes the formerly failing case with usable UI,
no audio stop, no new deadline/order/overflow fault or FIFO underrun, bounded
queues and intact guards. Record effective updates/s and delivery slack as well
as knob settings. Existing short passes do not establish a supported budget.

### V01B physical baseline — failure reproduced (2026-10-29)

[Preflight](../evidence/v01/gate-a-v01b-preflight.json),
[32/32 control](../evidence/v01/gate-a-v01b-control.json),
[64/64 failure](../evidence/v01/gate-a-v01b-dense.json).
Human confirmed the diagnostic SECS advanced ten seconds over an independent
ten-second interval. At 32/32, FADE X and 120 BPM, settings and a short Play/Stop
run with track-1 trigs were responsive: WGAP 35 ms, BATCH 1 ms, UGAP 35 ms,
FAULT 0. Pi FIFO stayed 18; no playout faults; 23 new STARTs were consumed.
This is a short muted control pass, not audible or sustained qualification.

At 64/64, setting depth was extremely laggy before Play; SRC took about ten
seconds to act and the UI then lagged behind for roughly half a minute. The
human extracted WGAP 33 ms, BATCH 9 ms, UGAP 225 ms, FAULT 0. Pi remained muted,
FIFO 18, playout faults 0, source queue peak 6. No dense Play was attempted.
The service-tick metric does not explain the whole input-response delay; do not
conflate UI tick continuity with panel response. Next: verified V01C paired
trial, starting with a fresh Pi boot and 32/32 control before dense settings.

### V01C physical comparison — no-self-wake insufficient (2026-10-29)

[Preflight](../evidence/v01/gate-a-v01c-preflight.json),
[control](../evidence/v01/gate-a-v01c-control.json),
[dense failure](../evidence/v01/gate-a-v01c-dense.json).
A manual first trigger was needed to establish an active control stream after
boot. FIFO rose 7→14 during that interval; this activation issue remains open.
The subsequent active 32/32 muted control was responsive, with no new Pi faults
or underruns and all 20 new STARTs consumed. Diagnostics: WGAP 34 ms, BATCH 1 ms,
UGAP 36 ms, FAULT 0. Human stopwatch/SECS check passed again.

Attempting 64/64 made setting controls extremely laggy and nearly unusable before
Play. Human estimated extracting the page would take about ten minutes; further
panel reading was stopped. Exact final knob settings and dense diagnostics are
unavailable. Pi faults stayed 0, FIFO stayed 14, queue peak 4. Thus removing
self-wake alone does not remove this failure; it does not exclude contention
from render-driven worker wakes or native USB/send critical sections. Future
trials should avoid manual diagnostic extraction under this failure. Next low
cost isolation is active-link versus cable-disconnected panel responsiveness;
no further firmware candidate is prepared yet.

### USB isolation and remote probe (2026-10-29)

After V01C dense lag, unplugging USB led to about six seconds of severe lag
with BPM flashing, then normal responsiveness (human observation). This
implicates active USB-related work but changes audio, MIDI, task load and session
state together; no particular masked send or IRQ is proven responsible.

[V01E/Pi remote probe](../evidence/v01/remote-candidate.json) is verified/staged,
Pi SD written/read-back verified and safely ejected ([deployment](../evidence/v01/remote-sd-deployment.json)); boot and human Digitakt V01E flash remain pending. It adds low-rate, best-effort telemetry isolated from the voice
protocol. Read-only DTIM0 counter/mode samples measure send critical sections,
UI gaps and worker batches in raw ticks. Timer movement/rate must be calibrated
on hardware before interpreting tick durations. The Pi retains last snapshot;
agent polls `v01status` and `trxstatus`, avoiding manual frozen-page extraction.
[Procedure and rollback](V01-remote-probe.md). This is diagnostic instrumentation,
not a scheduling fix, and its additive overhead must be acknowledged.

### V01E measured failure and V01F elapsed follow-up (2026-10-29)

[Boot](../evidence/v01/remote-boot.json),
[calibration](../evidence/v01/remote-control-calibration.json),
[reversible speed onset](../evidence/v01/remote-speed-reversal.json),
[severe result](../evidence/v01/remote-severe-result.json).
DTIM0 advances at ~132 MHz relative to render time at mode 3. Active 32/32
was clean with FIFO 14/faults 0. At SPD64/DEP32 the native send rate rose from
~284/s to ~533/s; mild sluggishness subsided on returning SPD32. This is a
human-reported approximate onset, not a supported numeric boundary.

At confirmed SPD64/DEP100, the human described ~3 fps. The recorder stopped
on FIFO 14→101; the log places a later rise to 145 before USB removal. Seven
late faults were retained when queried afterward; each onset is not precisely
captured. Individual masked sends stayed <=~23.8 us, aggregate measured masked
send fraction ~1.5% near first FIFO failure. Final retained UI gap ~215 ms and
worker batch elapsed max ~8.7 ms do not establish aggregate CPU occupancy.
The Pi still showed ~4.3% average core-3 load and 10 us block max. This weakens
long masked-send blocking as the sole explanation, not scheduling/IRQ bursts.
Muted active synthesis still runs before the final output mute.

[V01F paired diagnostic](../evidence/v01/duty-candidate.json) is verified/staged;
[F SD deployment](../evidence/v01/duty-sd-deployment.json) is read-back verified
and safely ejected. Boot and human F flash confirmation remain pending. Two cumulative elapsed counters bracket the worker and existing
native render events. They overlap under preemption; do not sum as CPU usage.
The 92-byte version-2 packet remains within the 96-byte transport limit, and
paired Pi accepts legacy E snapshots with an explicit version tag. No voice,
event-order, priority or deadline fix is claimed. Next step is the paired
installation and a short source-rate-matched capture, with no manual DIAG reads.

### V01F physical result and V01G targeted candidate (2026-10-29)

[Boot](../evidence/v01/duty-boot.json),
[32/32 baseline](../evidence/v01/duty-baseline.json),
[dense ramp](../evidence/v01/duty-dense-result.json).
F and its matching Pi kernel were physically installed and verified. At SPD64,
the human reports noticeable lag generally above DEP32, obvious around DEP55;
exact final DEP was not recorded. Unplugging USB restored responsiveness.
FIFO remained 17, faults 0, guards 0. This is a responsiveness failure despite
clean Pi counters, not a supported workload pass.

Native render brackets measured ~83% at baseline and ~85% during dense updates.
Worker elapsed fraction rose from ~3.6% to 33–36% at ~850–912 native sends/s.
Masked sends remained ~1.5–1.6%, individual max ~24.7 us. These elapsed brackets
overlap under IRQ preemption and must never be added or called pure CPU usage.
The association supports reducing per-event work, but does not isolate its
precise scheduling cause. UI tick gap peaked at 141 ms; this is not panel latency.

[Verified V01G pair](../evidence/v01/compact-candidate.json) tests a lossless compact full-snapshot format: version 3, 45 SysEx bytes
instead of 61 when all eight SRC values are integer 0..127. Each becomes one
7-bit byte and decodes to the identical 8.8 value. Every event, START, timestamp,
session, sequence, generation, signed note and velocity remains intact; no
coalescing, throttling or new latency. Fractional/negative/out-of-range SRC falls
back to exact legacy v1. Replays retain their original wire format. Version-2
session control and F diagnostic telemetry are unchanged.

The native serializer emits 60 USB-MIDI bytes for 45 SysEx bytes, versus 84 for
61. Both fit the measured bulk endpoint; this is not a USB packet-boundary claim.
The shorter message also uses the native serializer's existing short-message
branch. Retain masked capacity preflight and completion checks. Source facts
predict less per-event work; only hardware can establish responsiveness margin.

G requires the matching Pi decoder (accepts v1 and v3); F Pi does not support v3.
Keep USB disconnected while installing both. Recovery is F Pi plus existing
I04R/stock Digitakt recoveries. No physical G pass is claimed. After paired
verification, compare muted 32/32 then the same SPD64 ramp, stopping at lag or
new faults/underruns. Do not repeat prolonged frozen-panel extraction.

## Gate B — native persistence and musical edit semantics

Source/emulator mapping may proceed while Gate A awaits hardware. Establish
where USB1 machine identity, eight SRC values, LFO/locks and sound references
live in the native sound/pattern/project model before adding storage. Test only
operations that actually exist on this Digitakt; do not invent a separate kit
save workflow. The device remains authoritative and the Pi rebuilds transient
state from its ordered stream.

Use distinguishable A/B values in a disposable project. For each case record
before/action/after, stored state, observed final stream/PCM, and unintended
project-dirty changes. Test persistence once before reconnecting the Pi so a
host cache cannot conceal a device restore failure.

| Case | Required outcome |
| --- | --- |
| Project save/reload and cold boot | Machine, SRC, LFO, local processing and sound references restore; Pi cannot overwrite them with stale state |
| Sound save/load; track and pattern copy/paste | State follows native ownership; editing the copy does not change the source |
| Supported undo/reload operation | Correct native values return and pending old events cannot overwrite them |
| Track/trig mute and unmute | Native musical mute/tail behavior is preserved; no stale trigger replay; neighboring tracks work |
| Sound locks and return to base sound | Correct locked values and restoration at render time; START/voice identity remains ordered |
| Pattern/project/sound switch during play | Fresh values take effect in order; invalidate queues/session when old state could otherwise survive |
| Copy/load USB1 onto another track | Either explicit support or safe, documented unsupported behavior; never silently reroute the wrong voice |

Do not require a new handshake for every ordinary sound/value edit by default.
Determine which transitions need session invalidation from the state/queue
contract. Track mute need not kill a native release or effect tail unless the
stock behavior requires it. These were overconstraints in the earlier plan.

**Exit:** every applicable row has evidence or an explicit unsupported boundary;
required save/restore fields cannot be waived merely because reconnect works.

## Gate C — recovery and final combined qualification

Freeze one numeric profile after Gate A: firmware/kernel hashes, LFO destination,
SPD/DEP/MULT/WAVE/MODE/FADE/SPH, tempo, note/velocity, note/lock density and stock
track load. A supported budget describes this profile and measured event rate,
not just a speed/depth number. Retest 32/32, the prior 64/64 failure and the
worst intended workload; expand the sweep only if it answers a boundary question.
A deliberately failing setting is optional; inject link loss or completion stall
to verify fail-closed behavior without demanding a hardware overload.

Pre-register UI response criteria before qualification: panel and Play/Stop
response within 100 ms by wall-time observation, and maximum trial UI-service
gap <=100 ms, excluding boot and explicit project-load operations. Check the
clean baseline can meet these; if not, revise with a recorded rationale before
candidate scoring. These are proposed engineering criteria, not prior user
acceptance. Recheck relative onset timing against I04's accepted ~11.4 ms and
record any latency tradeoff explicitly.

Run USB pull/replug during active PCM, Pi reboot and Digitakt restart separately
at least three times each. Back up state before power cycles. Every true
reconnect must silence old PCM until a fresh session, START and matching
ACTIVATE; stale ACKs/notes must not cross sessions. A failed cycle stops the
series and needs diagnosis, not extra passing retries to average it away.

Then run **one ten-minute combined soak** at the worst supported profile with
notes, locks, UI edits, mutes and previously verified sound/state changes. Record
start/end fault/FIFO/USB/drop counters, queue/slack maxima, guards, Pi render/
control cost and human listening/UI observations. Zero new faults, underruns or
dropped/inserted blocks are required. A shorter human-accepted run is reported
as a limited result, not a full soak pass. Larger repeat counts are justified
only by intermittency or a changed recovery mechanism.

**Exit:** Gates A–C pass with explicit human listening acceptance, reproducible
source/patches, exact installed pair and verified rollback. V01 stays open until
then; more voices and Teensy stay deferred.

## Hardware and next action

Current installed pair: **V01F diagnostic** and Pi kernel
`0276c26b58092490f2b3b3fee89c99d4b4c8309144b0f53b13fbae0104354068`.
The last test ended USB disconnected and the UI responsive; Pi output muted,
trace off, FIFO 17 and fault 0. Freshly boot Pi for the next counter baseline.
G Pi SD is [written/read-back verified and safely ejected](../evidence/v01/compact-sd-deployment.json);
boot and human G Transfer confirmation remain pending. F is the last boot-verified
pair. No physical G pass is claimed.

Restore I04R before normal use from
`private/v01/firmware-V01P/Digitakt-I04R-previous.syx` (SHA256
`6a04ae9dd7791c7b11489af0061ea0012464922d5b6ca6935450f2b712f7fa2b`).
I04Q, H02F and stock 1.54 recoveries remain preserved. The agent controls Pi
mute/trace/status; the human handles Transfer and SD insertion. Keep lowered
speakers, known routing and disposable backed-up state for hardware audio tests,
per `AGENTS.md`. No bootloader/wiring changes. Keep firmware/projects/raw traces
private; replay dependency changes at the pinned base. V01 remains unaccepted.
