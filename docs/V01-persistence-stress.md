# V01 — one-voice qualification

**IN PROGRESS.** Ownership renewed by Codex at `2026-10-29T15:05:09Z`.
Original Phase 0 preservation passed; Phase 1 reproduced the dense-LFO failure.
Current Gate A remains incomplete: G was physically installed and failed at
SPD64/DEP48. V01P is rejected; V01N remains physically untested.
V01 is not accepted. Next work is the emulator investigation under Gate A.

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

### Current next work — emulator reproduction and causal isolation

This investigation remains **part of V01 Gate A**, owned by Codex. It is not a
new V02 or an additional qualification gate. Gate A has failed physically,
most recently on G at SPD64/DEP48. Gates B (persistence/edit semantics) and C
(recovery/soak) remain required afterward. No next hardware candidate is ready.

**Known:** lag grows with effective-update rate; USB removal previously restored
responsiveness; lowering worker priority improved UI but broke delivery;
no-self-wake and compact messages did not solve lag. G can lag with Pi fault0
and stable FIFO11. **Unknown:** dominant execution cost and scheduling/blocking
mechanism. CPU saturation is a hypothesis, not a measured finding. Worker and
render elapsed counters overlap; periodic UI ticks do not measure panel latency.

**Existing test gap:** `tools/i03_integrated_probe.py` runs compiled firmware
and native serialization, but synthesizes USB completions, injects replies using
saved/restored CPU context and does not assert dense-load panel-event latency.
`tools/i04_worker_runtime_probe.py` verifies genuine worker sleep/wake, but does
not reproduce the full contention workload. Their passes establish functional
properties and cannot establish absence of this performance bug.

Use the following sequence before another optimization or hardware flash:

1. **Audit and configure the execution model.** Use the pinned OS1.54 G build
   and a reproducible disposable snapshot. Inventory HLE shortcuts, artificial
   semaphore releases, IRQ sources and USB/ACK substitutions. Use native float
   and pixel routines (`Session(..., hle=False)`) and the available cycle-clock
   mode in `deps/digiemu/emu/cftiming.py` where supported. Establish CPU-clock
   assumptions or report bounded frequency sensitivity; the ~132MHz diagnostic
   timer is not proof of CPU frequency. The cycle model estimates instruction
   costs and omits physical cache/DDR/bus effects; report decoder mismatches and
   timing assumptions explicitly. Keep protocol-only tests as separate checks.
2. **Build an agent-run panel-latency reproduction.** Drive fixed 32/32 control
   and 64/48 failing-profile inputs, keeping LFO/tempo/track load and STARTs
   fixed. Record actual effective event rate; hardware references are ~284/s
   and ~816/s respectively, not exact emulator acceptance targets. Prefer the
   native LFO path; label any synthetic event replay as a component experiment.
   Inject a known button/encoder event at a fixed emulated time and measure
   enqueue→dispatch→handler completion and visible response where observable.
   Resolve/verify mk1 OS1.54 UI hooks; the existing `emu/uitrace.py` documents
   Digitakt II facts and its addresses/layouts must not be assumed compatible.
   Retain real native task switching and interrupt context. Replace direct
   context-injected ACK shortcuts with scheduled arrival when testing scheduling.
3. **Establish the regression verdict.** Provide one reproducible command,
   `tools/v01_contention_probe.py` (now includes a native MIDI control component;
   full USB/audio extension pending), that runs
   both profiles and reports panel latency/backlog, task runnable/wait states,
   audio deadlines, queue occupancy and protocol faults. Repeat fixed inputs
   to establish determinism. Start with Gate C's proposed 100ms response limit,
   verify the baseline can meet it and record any justified revision before
   scoring candidates. The important red signal is the actual panel-service
   failure; packet delivery alone is insufficient. A cycle estimate alone is
   not a hardware responsiveness proof.
4. **Attribute execution and isolate one path at a time.** Capture function/task
   self instruction counts and estimated cycles separately from inclusive time,
   interrupt work, blocked time and ready-but-unscheduled UI time. Trace native
   wake/dispatch, RX/ACK parsing and handling, serializer/USB service and UI
   dispatch. Use test-only path substitutions, preserving comparable input
   arrival rates and exposing their effects on sessions/deadlines. Rank these
   falsifiable hypotheses before experiments:

   | Hypothesis | Prediction to test |
   | --- | --- |
   | Task wake/dispatch pressure dominates | Reducing dispatch overhead in the diagnostic harness at the same event workload reduces UI runnable delay; trace attributes the removed cost |
   | RX/ACK processing dominates | Isolating ACK parse/handling cost at the same scheduled arrivals materially changes panel latency or measured execution cost |
   | Native USB service/interrupt pressure dominates | Delay follows completion/IRQ scheduling or native service work when event production is held constant |
   | UI queue/handler backlog dominates | Events arrive but accumulate or spend time in native UI handlers without the predicted loss of CPU service to the worker |

   These are diagnostic experiments, not permission to ship reduced ACK cadence,
   dropped updates or altered deadlines. Preserve evidence of failed hypotheses.
5. **Handle emulator disagreement explicitly.** If dense UI lag does not
   reproduce, report the measured software costs and name the remaining model
   discrepancy: completion/IRQ timing, stock wait shortcuts, memory contention
   or another evidenced missing path. Test model changes individually within
   documented assumptions. Do not tune fake delays until the desired failure
   appears or claim a root cause from a passing simplified model. Any required
   hardware probe must measure a specific unresolved variable with an explicit
   prediction and short stop condition; no repeated broad knob sweeps.
6. **Fix the identified mechanism and rerun.** Once a causal path is established,
   keep its failing test, implement the smallest justified change and rerun the
   original workload, ordering/deadline/reconnect checks and baseline. Only then
   stage one paired candidate with recovery and a short physical confirmation.
   If architectural capacity is the limitation, record the supported budget or
   required architecture change explicitly instead of weakening acceptance.

**Investigation deliverable:** a repeatable probe, compact attribution/ablation
report with exact revisions/model assumptions, and either a demonstrated causal
mechanism plus regression or an evidenced emulator gap plus one targeted
measurement proposal. No emulator reproduction or root cause is claimed yet.

#### Native panel/LFO preflight and evidenced USB gap (2026-10-29)

Implemented [v01_contention_probe.py](../tools/v01_contention_probe.py).
[Summary](../evidence/v01/contention-investigation.json), detailed runs at
[250 MHz](../evidence/v01/contention-preflight-250mhz.json) and
[200 MHz](../evidence/v01/contention-preflight-200mhz.json).
These are **disconnected component measurements**, not an active-link
reproduction. The command deliberately reports `emulator-gap; Gate A NOT SCORED`.
No firmware candidate, hardware change or causal fix resulted from this work.

Reproduce with the existing private G fixture and patched local Unicorn:

```sh
PYTHONPATH=private/r03-runtime:deps/digiemu \
  deps/digiemu/.venv/bin/python tools/v01_contention_probe.py \
  --candidate private/v01-compact-g \
  --output private/v01/contention-repeat.json
```

Use `--fsys 200000000` for the second frequency assumption. Each command runs
both profiles for 500 emulated ms. On this host the JIT needs the host cache
query permitted by the execution environment; the default sandbox caused
process exit 132 before emulation. No hardware connection is involved.

**Validated input seam.** The prepared snapshot selects USB1 through the native
UI, outside the timing window, and is deleted afterward. Measurement uses
`hle=False`, CycleClock, real RTOS switching and INTC2 source13 reschedules.
Panel bytes arrive at fixed shared-clock deadlines in UART8's DMA ring; vector
154 is delivered only when the programmed INTC mask and CPU IPL allow it.
`Session.press()` itself raises this vector without a level/mask check, so it
is used only during disposable setup. No ACK or panel callback is inserted by
saving/restoring the measured CPU context.

The mk1 OS1.54 hooks resolve to queue-send `0x40001b7a`, queue `0x40610c28`,
main loop `0x4000b6e4` (pop at +12), and key-dispatch call `0x4000b764`.
All four injected edges (track-1 trig press/release and SRC press/release) were
observed with matching codes and flags through enqueue, pop, dispatch and next
main-loop entry. This validates the used 16-byte record layout and dispatch
seam on mk1; it does not validate every Digitakt II UiTrace hook or prove
visible screen/Save completion latency. Guarded IRQ delivery and DMA ring wrap
are covered by `tests/test_v01_contention_probe.py`.

**Native workload encoding.** OS1.54's actual SPD formatter at `0x40065842`
displays `raw - 0x4000`; DEP at `0x4006585c` displays
`2*raw - 0x8000`, in 8.8 units. The probe verifies these instruction bytes.
SPD32 therefore uses `0x6000`; approximate SPD64 uses the valid maximum
`0x7ffe`, not signed `0x8000`. DEP32/48 use `0x5000`/`0x5800`.
Early discarded setup runs wrote these fields incorrectly; they are retained
only in ignored private storage and are not performance evidence.
The native render call at `0x40077ea2` loads tempo from `0x4020df10`; the
disposable fixture sets 14400 and observes that native LFO argument (120 BPM).
Other seeds are MULT4, FADE64, PTCH destination17, triangle0, phase0, FREE0,
PTCH60. Effective changes use G's integer truncation/clamp, not fractional
smoothing activity. No synthetic per-render modulation or delay is added.

| Assumed core | Setting | Effective changes/s | Max arrival→dispatch-pass completion | Queue depth max |
| --- | --- | --- | --- | --- |
| 250 MHz | 32/32 | 249.675 | 0.609 ms | 1 |
| 250 MHz | ~64/48 | 747.027 | 0.619 ms | 1 |
| 200 MHz | 32/32 | 249.674 | 1.196 ms | 1 |
| 200 MHz | ~64/48 | 747.025 | 1.196 ms | 1 |

Both 250 MHz reports were exactly equal, including snapshot hash, event times,
cycle counters and all profile results. All runs had one START/profile,
751 completed audio render IRQs/profile, zero estimated render overruns,
zero decoder mismatches, zero artificial semaphore releases and intact worker
guards. These rates count parameter changes, whereas physical ~284/~816/s
counts native sends (including other protocol traffic); equality is not an
acceptance target. Four early button edges and 500 ms do not qualify a soak.

**Execution-model audit and gap.** Every measured profile has USB_READY0,
gate0, zero native submission calls at `0x40004832`, zero compiled native RX
bridge calls, and zero completed USB IRQs (vector209). Worker execution is
only the disconnected retry path (15 runs), not the dense active-link worker.
This is a missing execution path, not a negative test of ACK or USB pressure.
The measured source list contains SSI, software eDMA, forced INTC sources and
the panel source; it contains no USB controller/completion source.

The missing handoff is now demonstrated independently by
[v01_usb_dispatch_probe.py](../tools/v01_usb_dispatch_probe.py) and
[its evidence](../evidence/v01/usb-dispatch-gap.json). The installed snapshot
has USB vector209 at `0x4000605e`, programmed IPL6; it ORs status into
`0x406104d8` and, at `0x400060e6`, forces INTC1 source6 through `0xfc04c014`.
Vector134 at `0x40007044`, IPL4, acknowledges that force bit and performs the
deferred USB work. Its status1 path walks endpoint completions at
`0x400071a2` onward. Existing Session/probe sources cover INTC0 and the added
INTC2 reschedule source, **not INTC1 source6**.

One fixed scheduled USB status4 event enters the real native vector209 ISR.
With the existing source set, native vector134 executes zero times and status4
plus the force bit remain pending after 10 emulated ms. Adding only the generic
`ForcedInterrupts(..., sources=(6,), base=0xfc04c000, first_vector=128)` source
makes native vector134 execute once and clear both. No delay, firmware patch,
handler substitution or saved/restored callback context is involved. Both
cases have no decoder mismatch or artificial wait. This is a regression for
the emulator's missing event route, not the hardware UI bug. The deliberate
ISR-entry test uses native IPL and CPU-mask deferral but bypasses the peripheral
mask; status4 tests a bounded status-clear branch, not DMA or RX/ACK handling.

```sh
PYTHONPATH=private/r03-runtime:deps/digiemu \
  deps/digiemu/.venv/bin/python tools/v01_usb_dispatch_probe.py \
  --candidate private/v01-compact-g \
  --output private/v01/usb-dispatch-repeat.json
```

Result: `Native USB handoff: missing source retains status; modelled source
dispatches/clears: PASS`. The adapter uses the pinned emulator's existing
generic interrupt model; no dependency source edits require replay patches.

The existing integrated exchange's native serializer stops at `0x40004832`:
its hook consumes the payload, increments queued bytes and returns directly.
Its render hook clears queued bytes as synthetic completion, sets USB status,
and supplies returned PCM; replies are context-injected every 5 ms. Thus
native descriptor submission, completion/USB ISR cost, and the real arrival
context/batching of RX bytes are excluded even if cycle timing is added.
The new preflight does not recycle those substitutions to claim a workload
pass. It also found no stock semaphore shortcut actually firing in these
windows; stock wait faking remains enabled by Session and must be audited
again when USB tasks become runnable.

Further timing limits: core frequency remains assumed, memory is zero-wait,
software eDMA copies complete without physical bus time, and cache/DDR/audio
DMA contention is absent. The cycle hook precharges entire translated blocks,
so event timestamps have block granularity. Exception-entry charges outside
the stepper also make the cycle and peripheral clocks differ slightly; the
report exposes that drift and uses CycleClock consistently for event latency.
The existing vector statistics are model output, not physical CPU occupancy.
There is no active-link function/task self-cost or UI runnable-time attribution
yet, and no hardware CPU-saturation conclusion.

The continuation below now wires INTC1 source6 and native MIDI descriptor/RX
dispatch into the panel probe. Native periodic USB and audio traffic remain
missing. Source-derived semantics should be implemented before requesting a
hardware run; do not replace missing USB work with a fitted synthetic delay.

**One justified hardware measurement, only if source work leaves the USB
service timing/batching unresolved:** measure native vectors209 **and134** self
time and their nested overlap with worker spans alongside panel
enqueue→dispatch latency.
This specifically tests whether USB interrupt/service bursts consume the UI's
available service time. Current send critical-section counters exclude this
path, and worker/render brackets cannot provide it by subtraction.

Prepare/verify bounded counter instrumentation in software first. Collect
per-window count and total/max self ticks for each USB IRQ, RX callback count/batch size,
worker self ticks with IRQ overlap removed, and the four panel-event timestamps;
retain summaries for low-rate retrieval after the timed window. Do not print
or transmit a full trace during the run. Verify nesting/counter arithmetic
against emulator execution before proposing an installation.
The single proposed procedure is 10 s muted 32/32, then 10 s at the already
failing ~64/48, with one SRC and one FLTR press in each window. Stop immediately
on lag/new fault/FIFO growth and unplug; no larger depth sweep or manual DIAG
extraction. If increasing USB self/blocked time accounts for delayed panel
dispatch, the missing native service path becomes the next causal target.
If it remains small while worker self time or UI-handler backlog grows, that
prediction fails and the same capture selects the alternative path. This is a
measurement proposal, not an optimization candidate or a request to flash now.

#### Native MIDI control integration — continuation (2026-10-29)

[Compact evidence](../evidence/v01/native-control-component.json) records the
native model regressions, exact source hashes, repeatability, panel response,
queue state and exclusive estimated execution costs. Raw emulator reports and
the inherited uncommitted files are preserved in ignored `private/v01/`.
No firmware/Pi changes or hardware measurements were made.

**Inherited work audit.** The other agent had integrated source6 and periodic
status-event injection and started `tools/v01_usb_device_model.py`, but had not
demonstrated an active descriptor path. Its 13 small tests passed; running
`--usb-device` against G returned `invalid-preflight` with zero primes,
completions and RX callbacks, USB_READY0 and gate0. The fixture was still
disconnected. This was unfinished integration, not completion of step2.

The new [native completion probe](../tools/v01_usb_completion_probe.py) exposed
two independent errors through the actual OS1.54 submit/ISR/callback path:

| Native component case | Inherited result | Corrected result |
| --- | --- | --- |
| OUT index3, 60 bytes, native TX-accounting callback | Callback runs but queued count stays60: zero transferred bytes reported | One callback; queued count0 |
| IN index5, 60 bytes | No prime/completion/callback; queued count60 | One callback; queued count0 |
| Two native IN submissions, 60+24 bytes, before any completion | Additional regression | Both callbacks; queued count0 |
| OUT request64 with short payload4 | Additional regression | One callback reports4; accounting remainder60 |

The model now uses the native endpoint masks (IN index5 is bit17, not bit5),
reads the hardware QH next field at +8, follows dTD next pointers, and leaves
the software QH head/tail to ISR134. Token high16 is residual bytes; native
ISR134 subtracts it from requested length. Native PRIME remains visible until
accepted, so back-to-back masked submissions cannot overwrite the first
descriptor. OUT requests wait for an explicit host payload; no empty receive
completions are invented. W1C acknowledgements precede later completions,
RUN/STOP and USBINTR.UI are respected, and native flush polling is acknowledged.
The snapshot's USBINTR was0, making a configured-controller setup precondition
necessary rather than silently bypassing the peripheral mask.

**Configured control component.** Setup executes native MIDI configuration
`0x4000463e(1)` and readiness/replenishment `0x40004516` outside measurement.
It explicitly supplies RUN/UI-interrupt/queue-base configuration and an already
synchronized Overbridge route (mode6, sync1). This replaces enumeration/audio
startup for the component test; it is not a boot/reconnect proof. These stores
are made once, never repeated at render hooks. Active native MIDI queues are
adopted on snapshot resume. During measurement there are no submission hooks,
queued-byte clearing, returned-PCM injection or receive-context calls.

The [portable peer](../tools/v01_usb_peer.py) consumes native IN DMA payloads
using the existing compiled Circle protocol helper. Replies are packetized as
USB MIDI and scheduled into OUT buffers; native ISR209→source6→ISR134 dispatches
`0x400044ee`, which posts the native receive job at `0x40004760`. The compiled
RX bridge runs at IPL0, then the genuine worker handles ACKs. Host protocol
processing is instantaneous in device time; one transaction per endpoint per
assumed 125us host poll is an explicit policy, not measured physical scheduling
or a packet interval inferred from endpoint capacity.

Reproduce both fixed profiles, with early buttons and four additional
FLTR/SRC edges at 350/380/430/460ms after control has become active:

```sh
PYTHONPATH=private/r03-runtime:deps/digiemu \
  deps/digiemu/.venv/bin/python tools/v01_contention_probe.py \
  --candidate private/v01-compact-g --usb-device --usb-peer \
  --profile-execution --output private/v01/native-control.json
```

Use `--fsys 200000000` for the second declared frequency assumption. The native
completion regression command is:

```sh
PYTHONPATH=private/r03-runtime:deps/digiemu \
  deps/digiemu/.venv/bin/python tools/v01_usb_completion_probe.py \
  --candidate private/v01-compact-g --output private/v01/native-completion.json
```

| Assumed core | Setting | Installed snapshots / 500ms | Max arrival→dispatch-pass completion | Worker exclusive model cycles | USB209+134 exclusive model cycles |
| --- | --- | --- | --- | --- | --- |
| 250 MHz | 32/32 | 119 | 3.153ms | 0.853% | 0.121% |
| 250 MHz | ~64/48 | 354 | 3.242ms | 2.465% | 0.358% |
| 200 MHz | 32/32 | 118 | 6.557ms | 1.067% | 0.136% |
| 200 MHz | ~64/48 | 353 | 7.323ms | 3.053% | 0.401% |

Two complete 250MHz reports are exactly equal. Each profile has all eight
button edges completed, one stable active session/START, gate1, native RX at
IPL0, no artificial semaphore releases, no decoder mismatch, no guard fault,
no model error and no latched queue overflow/fatal flag. Sampled source queue
peak is1 (up to2 at 200MHz); these are sampled lower bounds, not exact
watermarks. Source→portable-host age is at most one render block; ACTIVATE
processing can follow the source block by12 blocks at baseline or4 at dense
load. That is a sender ACK-service observation, not Circle playout slack.
The 17 focused tests pass in the pinned emulator environment.

[Execution attribution](../tools/v01_execution_profile.py) partitions entered
block costs by current native TCB or top interrupt frame. The pinned scheduler
loads its current TCB from `0x4399e798`; this instruction is byte-verified.
Selected linked function/PC ranges report self costs separately from task
totals. At 250MHz, render IRQ191 accounts for ~61.0–61.2% of estimated cycles;
at 200MHz, ~76.3–76.4%. There are zero unattributed cycles in these runs.
Branch resolution/entry costs can land at an adjacent block boundary. These
are estimates, with no physical memory cost, and are not summed with inclusive
worker/render elapsed brackets. Ready-but-unscheduled UI time and visible
screen completion still are not measured.

**Continuation:** the periodic and audio paths below now execute. The physical
failure is still not reproduced; Gate A remains NOT SCORED.

### Native periodic USB and audio — continuation (2026-10-29)

[Compact evidence](../evidence/v01/native-audio-component.json). Native boot
writes USB timer0 load9999/control0x81000000. Model `--usb-periodic` now latches
bit24 every10ms and SOF status0x80 at high-speed microframes; native IRQ209 and
IRQ134 process those events. These semantics come from
[NXP MCF54418RM](https://www.nxp.com/docs/en/reference-manual/MCF54418RM.pdf),
sections24.3.2/24.3.4. The dTD completion frame field is an11-bit **frame**
number, while FRINDEX low3 bits count microframes (24.5.2.2.3/24.3.4.4).

`--usb-audio` runs native stream stops, clears/rebuilds the native audio QHs,
then runs native OUT/IN constructors. Native render code produces seven-frame
368-byte IN packets; each earns one silent88-byte OUT return, as in physical
H01. No empty OUT completions or fitted audio delays are generated. Native
queue/callback/receive paths execute. Enumeration remains a component setup
precondition, and the portable peer does not model Pi synthesis/USB jitter.

Two concrete model repairs were necessary: retain dTD IOC after completion,
and store FRINDEX>>3 in the completion frame field without destroying the page
address. With the old frame semantics, both LFO profiles had136 counter gaps
and two endpoint7 prime faults in120ms. Correct frame semantics yield826
packets, zero gaps/faults,180 render blocks and panel maximum0.551ms. The
frame regression fails with the old behavior and passes with the correction.
Native full/short/chained completion regressions also still pass. **This is
an emulator fix, not evidence of the physical LFO culprit.**

Fixed500ms runs retain the same eight button edges and native LFO settings:

| Model workload / assumed core | Setting | Snapshots | Audio IN / returns | Max panel service | Worker exclusive cycles | Render exclusive cycles |
| --- | --- | --- | --- | --- | --- | --- |
| Periodic + MIDI /250MHz | 32/32 |119|0|3.150ms|0.85%|~61%|
| Periodic + MIDI /250MHz | ~64/48 |354|0|3.240ms|2.46%|~61%|
| Audio + periodic + MIDI /250MHz |32/32|119|3432|3.237ms|0.858%|63.708%|
| Audio + periodic + MIDI /250MHz |~64/48|354|3432|3.775ms|2.454%|63.791%|
| Audio + periodic + MIDI /200MHz |32/32|119|3431|7.281ms|1.073%|79.262%|
| Audio + periodic + MIDI /200MHz |~64/48|353|3431|9.195ms|3.032%|79.364%|

Every audio profile has750 render blocks, zero audio counter gaps, matching
return credits, no model errors, gate1, one stable session, no decoder mismatch
or artificial worker wait. At200MHz render IRQ worst modeled duration is
~80.1% of its deadline, with no modeled overrun. Timer0 ticks50/SOF ticks4000
per profile. These are zero-wait **model** costs, not hardware CPU utilization.
The CLI rejects startup-only audio, missing return credits and counter gaps.
21 focused tests pass. Repeat:

```sh
PYTHONPATH=private/r03-runtime:deps/digiemu \
  deps/digiemu/.venv/bin/python tools/v01_contention_probe.py \
  --candidate private/v01-compact-g --usb-device --usb-peer \
  --usb-periodic --usb-audio --profile-execution --duration-ms 500 \
  --fsys 200000000 --output private/v01/native-audio.json
```

Use `--fsys 250000000` for the other declared core assumption.

**Progression decision:** missing periodic/audio traffic alone does not produce
the lag under these assumptions. Do not repeat more density sweeps or tune USB
poll delays. Do not yet calibrate hardware costs from the newly captured raw
firmware brackets: `emu/dtim.py` computes diagnostic DTCN0 reads from `self.clock`,
updated only in `step/service`. A native short bracket can observe stale time
while CycleClock advances. The probe explicitly labels these raw totals
unsuitable for hardware cost calibration. They remain private diagnostic data.

The next bounded investigation is clock fidelity, then a hardware/model cost
comparison. First make DTCN0 reads coherent with CycleClock, with a regression
that reads twice inside one execution step. Preserve the declared132MHz bus
assumption; it is not proof of core frequency. Separately, native render entry
(`0x40076c4c`) timestamps DTIM2, and USB ISR209 (`0x4000606c`) uses its relative
phase. DTIM2 is configured for an external pin clock, which the emulator does
not drive: eight observed native alignment samples read0. Clean packet counters
do not validate that physical clock/phase relation.

Recover the external input relationship from source/pin configuration before
inventing a rate. If it cannot be established offline, **one targeted measurement**
is justified: buffer a bounded set of DTCN2/FRINDEX pairs at consecutive32-frame
render entries, then retrieve after timing stops. Its sole question is whether
DTIM2 advances with the audio sample clock and by how many ticks/sample; no
knob sweep or timing-time trace dump. No new hardware candidate/flash is
requested now. Physical cache/DDR/bus costs, core clock, Pi scheduling and the
complete physical project remain unmodeled. Gate A is IN PROGRESS/unaccepted.

Preserve ordered START identity, full effective state, fail-closed deadlines,
bounded queues, task-only USB and allocation-free/nonblocking audio callbacks.
Any change to control cadence, ACK policy, latency or event semantics needs an
explicit contract and audio/ordering evidence. Do not cycle arbitrary priorities,
enlarge latency buffers to score a pass, or ask for another speculative flash.

**Gate A exit remains physical:** one paired candidate passes the formerly
failing case with usable UI, no audio stop, no new deadline/order/overflow fault
or FIFO underrun, bounded queues and intact guards. Record effective updates/s
and delivery slack along with the full profile. Emulator results support this
qualification but do not replace final physical listening/UI acceptance.

### Earlier Gate A trials and evidence

The B/C/E/F/G sections below retain the trial sequence. Their next-step language
records historical decisions; the emulator investigation above is the current
next action. Existing SECS/reset/clock correction and recovered firmware proofs
remain valid. Do not print full traces during physical timing measurements:
`trxtrace` dumps previously caused FIFO growth.

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

### V01G first active baseline

[G boot](../evidence/v01/compact-boot.json) and human Transfer confirmation are
complete. [Muted 32/32 counter baseline](../evidence/v01/compact-baseline.json):
7 samples over 12 seconds, active controls, 2 STARTs consumed, fault/guards0,
FIFO11 stable, UI gap34 ms. FIFO rose 0→11 sometime between disconnected boot
and first active capture; startup behavior remains unresolved. Native sends
~284/s, worker elapsed ~3.4–3.7%, render ~82.9% (overlapping brackets). No dense,
human responsiveness or audible G acceptance yet. Next step changes only SPD
to64 while DEP stays32, with remote capture and early stop on lag/new faults.

### V01G dense result — compact messages insufficient

[SPD64/DEP32](../evidence/v01/compact-speed-result.json): slight sluggishness,
fault0/FIFO11 stable. [SPD64/DEP48](../evidence/v01/compact-depth48-result.json):
human reports quite laggy. G therefore fails Gate A despite no new Pi faults,
underruns or guard errors in this capture. UI tick maximum93 ms is not input
latency. At ~816 native sends/s, worker elapsed ~25–28%, native render
~84.6–84.8%, masked send ~1.23%; elapsed brackets overlap. Shorter snapshots
alone do not resolve the failure. Profiles differ from F, so no exact speedup
or worsening is established. Capture stopped; no further density increase.
USB unplug requested; recovery confirmation pending.

Next work is offline compiled per-update profiling to distinguish task dispatch,
ACK decode/handling and serialization costs. Do not prepare another hardware
variant solely from byte-count estimates. Periodic UI ticks cannot establish
panel-event service latency. ACK cadence changes require an explicit protocol
contract and deadline/reconnect tests; no dropping/throttling events is implied.

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

Installed pair: **V01G diagnostic** and Pi kernel
`2b027a172e089af8fc273e4b2351d7fd72a7223b92f8ac6dfc8b20c491e2df5f`.
Human installation and [Pi boot](../evidence/v01/compact-boot.json) are confirmed.
The G dense trial ended with fault0/FIFO11 stable, Pi output muted, trace off.
USB unplug was requested; physical disconnection/recovery confirmation is still
pending. Do not infer current cable state from the earlier disconnected boot.
F Pi rollback and manifest are preserved in the pre-compact SD/local backups.

Next action is **Gate A's emulator reproduction and causal isolation plan**.
Hardware trials are on hold pending that investigation; Gates B/C stay open.

Restore I04R before normal use from
`private/v01/firmware-V01P/Digitakt-I04R-previous.syx` (SHA256
`6a04ae9dd7791c7b11489af0061ea0012464922d5b6ca6935450f2b712f7fa2b`).
I04Q, H02F and stock 1.54 recoveries remain preserved. The agent controls Pi
mute/trace/status; the human handles Transfer and SD insertion. Keep lowered
speakers, known routing and disposable backed-up state for hardware audio tests,
per `AGENTS.md`. No bootloader/wiring changes. Keep firmware/projects/raw traces
private; replay dependency changes at the pinned base. V01 remains unaccepted.
