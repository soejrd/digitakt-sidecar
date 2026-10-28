# Progress handoff

Task status/claims: [TASKBOARD.md](TASKBOARD.md). Pins: sources.json. Read AGENTS.md and PLAN.md before work. Earlier investigation is in Git and task reports.

## Current outcome

**I04 DONE by human acceptance of the one-voice prototype.** I04R firmware + Pi 128-frame/2.667 ms buffer now play real 44.1 kHz TRX-B2 through native SRC controls, locks/shallow LFO and local filter/AMP/sends; note/PTCH policy and sequenced velocity pass. Human: clean through final regression, timing/envelopes “super acceptable”; requested wrap-up. [Acceptance](evidence/i04/physical-acceptance.json), [report](docs/I04-external-trx-voice.md).

**Measured timing:** same 120 BPM native-click reference, analog relative onset median **11.417ms**, range 11.021–11.750 ms across 19 hits (old I04Q 115.833 ms). Short-run spread 0.729 ms; not calibrated manual-key latency or worst-case jitter. Clean timing interval: 20/20 STARTs including recovery, zero faults, FIFO 3 startup underruns unchanged; zero recorder drops. Pi observed max block 10 µs for 12 frames/250 µs, max control pass 18 µs, core 3≤3.5% during timed pattern; not dense worst-case CPU proof. [Timing](evidence/i04/event-worker-physical-timing.json).

**Known failure explicitly deferred to V01:** increasing LFO speed/amount (e.g. PTCH) can stop TRX audio and make Digitakt sluggish. Captured code3 late parameter event **260frames/5.417ms late**; host disarmed. Retained fault 1, FIFO later 29 versus clean 3; interval includes later listening/high modulation, no precise boundary. No OS crash dump; sender CPU/transport/host scheduling cause unisolated. [Failure/reproduction lead](evidence/i04/v01-dense-lfo-handoff.json).

## V01 — current owner and next step

Codex renewed ownership `2026-10-28T18:41:42Z`. V01 remains IN PROGRESS.
[Revised three-gate plan](docs/V01-persistence-stress.md): scheduling diagnosis,
native persistence/edit semantics, then recovery and one combined soak.
Phase 0 passed; Phase 1 reproduced 64/64 lag; Phase 2 is incomplete.

**Diagnostic correction:** V01D/N/P converted 1500-Hz render blocks as 32-Hz
ticks, inflating displayed milliseconds by 46.875×. UGAP 8469 is approximately
181 ms, not 8.469 s. Human-reported seconds-long lag remains valid; multi-second
UI starvation is not established by these counters. [Correction/test evidence](evidence/v01/diagnostic-clock-correction.json).
Actual C UI/worker producer/formatter regression failed before the fix and passes
afterward; dense event-worker/START/ACK/overflow checks pass. No new firmware
build or hardware test yet. Old candidate source manifests are stale after the
correction; rebuild rather than reusing V01N artifacts.

V01P priority 5 remains rejected: responsive UI but growing Pi late faults and
FIFO underruns, even muted. No-self-wake remains physically untested. Next:
prepare corrected interval-scoped diagnostics and verified baseline/recovery,
then compare baseline vs no-self-wake at 32/32 and 64/64. Existing model tests
cannot reproduce the exact hardware UI failure; do not treat them as proof.

## Hardware / recovery

- Last recorded installation: **V01P diagnostic**, Pi muted. No live contact in
  this session. Restore I04R before normal use from
  `private/v01/firmware-V01P/Digitakt-I04R-previous.syx`; I04Q/H02F/stock also
  retained. I04R SHA256 `6a04ae9dd7791c7b11489af0061ea0012464922d5b6ca6935450f2b712f7fa2b`.
- Pi kernel SHA256 `c057f063c24770a520b842e66d4d7e10655bdda3d45de05e1b67edc85d34a0f2`,
  128-frame playout. Rollback: `private/i04/sd-backup-event-worker/`.
- Fresh Pi reboot required for clean counter baselines. `trxtrace=on` forces
  mute; read status before dumping (printing caused underruns). Analyzer delay
  is **128**, not historical 2048. Preserve deadline/order/START behavior.
- Human handles flash; confirm availability/lowered speakers/disposable backups
  before new audio tests. Original user project and recorder settings preserved.

Implementation foundations and reproducible checks are in
[the I04 report](docs/I04-external-trx-voice.md). Ten replayable patches target
sidecar-firmware `6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457`; mdkit is unchanged.
Keep firmware/raw captures private. Local user `PLAN.md` edit is excluded from
commits. No push/publishing.
