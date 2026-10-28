# Progress handoff

Task status/claims: [TASKBOARD.md](TASKBOARD.md). Pins: sources.json. Read AGENTS.md and PLAN.md before work. Earlier investigation is in Git and task reports.

## Current outcome

**I04 DONE by human acceptance of the one-voice prototype.** I04R firmware + Pi 128-frame/2.667 ms buffer now play real 44.1 kHz TRX-B2 through native SRC controls, locks/shallow LFO and local filter/AMP/sends; note/PTCH policy and sequenced velocity pass. Human: clean through final regression, timing/envelopes “super acceptable”; requested wrap-up. [Acceptance](evidence/i04/physical-acceptance.json), [report](docs/I04-external-trx-voice.md).

**Measured timing:** same 120 BPM native-click reference, analog relative onset median **11.417ms**, range 11.021–11.750 ms across 19 hits (old I04Q 115.833 ms). Short-run spread 0.729 ms; not calibrated manual-key latency or worst-case jitter. Clean timing interval: 20/20 STARTs including recovery, zero faults, FIFO 3 startup underruns unchanged; zero recorder drops. Pi observed max block 10 µs for 12 frames/250 µs, max control pass 18 µs, core 3≤3.5% during timed pattern; not dense worst-case CPU proof. [Timing](evidence/i04/event-worker-physical-timing.json).

**Known failure explicitly deferred to V01:** increasing LFO speed/amount (e.g. PTCH) can stop TRX audio and make Digitakt sluggish. Captured code3 late parameter event **260frames/5.417ms late**; host disarmed. Retained fault 1, FIFO later 29 versus clean 3; interval includes later listening/high modulation, no precise boundary. No OS crash dump; sender CPU/transport/host scheduling cause unisolated. [Failure/reproduction lead](evidence/i04/v01-dense-lfo-handoff.json).

## Hardware / recovery

- User disconnected Pi and Digitakt for shutdown; do not contact hardware until available. Before disconnection Pi explicitly muted, recorder idle and original channel/bit-depth/prefix settings restored.
- Installed Digitakt I04R SHA256 `6a04ae9dd7791c7b11489af0061ea0012464922d5b6ca6935450f2b712f7fa2b`; private `private/i04/firmware-I04R/` includes previous I04Q, H02F and stock1.54 recovery. Only MAIN changed; protected sections verified. Human handles Transfer; no computer UI access.
- Installed Pi build `Oct 28 2026 13:38:57`, SHA256 `c057f063c24770a520b842e66d4d7e10655bdda3d45de05e1b67edc85d34a0f2`. Card rollback `kernel8-rpi4.before-event-worker.img` / `SHA256SUMS.before-event-worker`; local `private/i04/sd-backup-event-worker/`. [Deployment](evidence/i04/event-worker-sd-deployment.json).
- Last timing setup: right output→left input cable, input monitoring off, disposable pattern USB1 right/native track2 left. User may change routing after closure. Raw WAVs/logs/projects/firmware stay ignored/private.

## Next: V01 IN PROGRESS — Phase 0 PASS

Claimed by Codex `2026-10-28T15:51:11Z`; **no hardware contact or audio test**. [Phase 0 preflight](evidence/v01/phase0-preflight.json) passes: 50/50 frozen source hashes, ten-patch replay at pinned base, all five dependency pins, I04R/stock/I04Q/H02F firmware recovery, installed and pre-I04 kernels, and private-asset exclusion. Installed pair remains I04R `6a04ae9…f7fa2b` + Pi kernel `c057f06…4a0f2`. [Phase 1 runbook](docs/V01-phase1-runbook.md) and [preregistration](evidence/v01/phase1-preregistration.json) are prepared but not executed; `trxtrace=on` mutes the voice, so audible and trace passes are paired. Next: recover the accepted numeric PTCH baseline and run read-only persistence source/emulator inspection; hardware waits for the [release gate](docs/V01-phase1-runbook.md#release-gate-and-safety).

`trxtrace=on` forces mute; collect status before `trxtrace` dump (printing can stall output). For installed buffer use analyzer **`--delay 128`**, not historical2048 default. Preserve fail-closed deadlines/order/STARTs; do not mask overload with coalescing or rebasing.

Implementation: I04R native event-driven RTOS task, bounded render semaphore signal, task-only USB, immediate handshake RX and batched active ACKs. Duplicate mdkit target smoothing removed for final Digitakt values; per-hit velocity after limiter. [Candidate/checks](evidence/i04/event-worker-candidate.json). ASan/UBSan suite, ColdFire render imports, compiled UI/native exchange and genuine task/stack checks pass; patches 0001–0010 replay at sidecar-firmware `6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457`, tracked overlay retains unchanged mdkit `96157fe4fbda17bf4cae3168f52437d8232e6570`.

Established foundations: [H01 transport](docs/H01-stock-usb-stream.md), [H02 routing](docs/H02-real-track-pipeline.md), [I03 controls](docs/I03-src-control-link.md), [R04 ordering](docs/R04-control-ordering.md). More voices/Teensy remain deferred pending V01/capacity. Unrelated user PLAN.md edits remain local and excluded from commits. No push/publishing.
