# Progress handoff

Task status/claims: [TASKBOARD.md](TASKBOARD.md). Pins: sources.json. Read AGENTS.md and PLAN.md before work. Earlier investigation is in Git and task reports.

## Current outcome

**I04 DONE by human acceptance of the one-voice prototype.** I04R firmware + Pi 128-frame/2.667 ms buffer now play real 44.1 kHz TRX-B2 through native SRC controls, locks/shallow LFO and local filter/AMP/sends; note/PTCH policy and sequenced velocity pass. Human: clean through final regression, timing/envelopes “super acceptable”; requested wrap-up. [Acceptance](evidence/i04/physical-acceptance.json), [report](docs/I04-external-trx-voice.md).

**Measured timing:** same 120 BPM native-click reference, analog relative onset median **11.417ms**, range 11.021–11.750 ms across 19 hits (old I04Q 115.833 ms). Short-run spread 0.729 ms; not calibrated manual-key latency or worst-case jitter. Clean timing interval: 20/20 STARTs including recovery, zero faults, FIFO 3 startup underruns unchanged; zero recorder drops. Pi observed max block 10 µs for 12 frames/250 µs, max control pass 18 µs, core 3≤3.5% during timed pattern; not dense worst-case CPU proof. [Timing](evidence/i04/event-worker-physical-timing.json).

**Known failure explicitly deferred to V01:** increasing LFO speed/amount (e.g. PTCH) can stop TRX audio and make Digitakt sluggish. Captured code3 late parameter event **260frames/5.417ms late**; host disarmed. Retained fault 1, FIFO later 29 versus clean 3; interval includes later listening/high modulation, no precise boundary. No OS crash dump; sender CPU/transport/host scheduling cause unisolated. [Failure/reproduction lead](evidence/i04/v01-dense-lfo-handoff.json).

## V01 — V01B physical failure reproduced; V01C next

Codex ownership renewed `2026-10-28T22:49:17Z`. V01 remains IN PROGRESS.
[Plan](docs/V01-persistence-stress.md), [trial runbook](docs/V01-gate-a-runbook.md),
[paired candidate evidence](evidence/v01/gate-a-candidates.json).

**Offline preparation passes.** V01B = priority-14/self-wake baseline; V01C =
same priority/no-self-wake. Both have corrected timing, machine-selection trial
reset, SECS stopwatch comparison, and sticky guard faults. Both build/boot and
pass panel, compiled reset/formatter, native sleep/wake/guards, and dense
compiled exchange/ACK/reconnect checks. Full I04 host regression and ten-patch
replay pass. Packages: `private/v01/firmware-V01B/` and `firmware-V01C/`, each
with I04R/I04Q/H02F/stock rollback. Neither is installed or hardware-qualified.
No Pi code/kernel change. Hardware resumed 2026-10-29. Human installed V01B;
agent controls Sidecar via its existing HTTP /cmd and reads /status and /log.
Agent confirmed trx=off and trxtrace=off; user only handles Digitakt panel/flash.

**Physical baseline:** SECS matches a ten-second stopwatch interval. Muted
32/32 control responsive with trigs: WGAP 35/BATCH 1/UGAP 35 ms, FAULT 0;
FIFO 18 unchanged, no Pi faults, 23 new STARTs consumed. At 64/64 setting depth
already caused severe lag; SRC about ten seconds, backlog about half a minute.
WGAP 33/BATCH 9/UGAP 225 ms, FAULT 0. Pi FIFO still 18, faults 0, queue peak 6.
Stopped before dense Play. [Control](evidence/v01/gate-a-v01b-control.json),
[failure](evidence/v01/gate-a-v01b-dense.json). Tick gap is not input latency.

**Next physical action:** human install verified V01C, fresh Pi reboot; agent
re-confirms muted/trace-off and new counter baseline. Repeat 32/32 control then
64/64 setting phase; stop at lag/new faults/underruns. Do not adopt a UI-only
improvement. Runbook and private capture script remain available.

Earlier V01D/N/P times were inflated by 46.875×; UGAP 8469 is ~181 ms, not
8.469 s. Human-observed multi-second lag remains valid. Do not reuse old
candidate artifacts or their ms interpretations. Static native wait clears all
pending posts, so one-wait-per-old-credit accumulation is ruled out; repeated
self-post readiness remains a hypothesis. V01P priority 5 stays rejected.

## Hardware / recovery

- Last human-reported installation: **V01B diagnostic**, Pi muted by agent.
  Restore I04R before normal use from
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
