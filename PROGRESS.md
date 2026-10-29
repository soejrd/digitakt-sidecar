# Progress handoff

Task status/claims: [TASKBOARD.md](TASKBOARD.md). Pins: sources.json. Read AGENTS.md and PLAN.md before work. Earlier investigation is in Git and task reports.

## Current outcome

**I04 DONE by human acceptance of the one-voice prototype.** I04R firmware + Pi 128-frame/2.667 ms buffer now play real 44.1 kHz TRX-B2 through native SRC controls, locks/shallow LFO and local filter/AMP/sends; note/PTCH policy and sequenced velocity pass. Human: clean through final regression, timing/envelopes “super acceptable”; requested wrap-up. [Acceptance](evidence/i04/physical-acceptance.json), [report](docs/I04-external-trx-voice.md).

**Measured timing:** same 120 BPM native-click reference, analog relative onset median **11.417ms**, range 11.021–11.750 ms across 19 hits (old I04Q 115.833 ms). Short-run spread 0.729 ms; not calibrated manual-key latency or worst-case jitter. Clean timing interval: 20/20 STARTs including recovery, zero faults, FIFO 3 startup underruns unchanged; zero recorder drops. Pi observed max block 10 µs for 12 frames/250 µs, max control pass 18 µs, core 3≤3.5% during timed pattern; not dense worst-case CPU proof. [Timing](evidence/i04/event-worker-physical-timing.json).

**Known failure explicitly deferred to V01:** increasing LFO speed/amount (e.g. PTCH) can stop TRX audio and make Digitakt sluggish. Captured code3 late parameter event **260frames/5.417ms late**; host disarmed. Retained fault 1, FIFO later 29 versus clean 3; interval includes later listening/high modulation, no precise boundary. No OS crash dump; sender CPU/transport/host scheduling cause unisolated. [Failure/reproduction lead](evidence/i04/v01-dense-lfo-handoff.json).

## V01 — F reproduces lag; compact full-snapshot G candidate

V01 IN PROGRESS, Codex owner. [Plan](docs/V01-persistence-stress.md).
B/C fail; V01P priority reduction rejected. No-self-wake alone insufficient.
E severe SPD64/DEP100 gave ~3 fps, FIFO growth before unplug and late faults.
[Result](evidence/v01/remote-severe-result.json). Corrected diagnostic clock is
1500 blocks/s; old D/N/P displays overstate ms by 46.875×.

**Installed F pair verified:** [boot](evidence/v01/duty-boot.json),
[32/32 baseline](evidence/v01/duty-baseline.json),
[dense result](evidence/v01/duty-dense-result.json). At SPD64, lag generally above
DEP32 and obvious around DEP55 (approximate human ramp, final value unrecorded).
USB removal restored responsiveness. Pi muted, faults 0, FIFO 17 stable, guards 0.
Native render elapsed ~83→85%, worker ~3.6→33–36% at ~850–912 sends/s. Brackets
overlap under preemption; never sum as CPU utilization. Masked send ~1.6%,
max ~24.7 us; no proof of a single dominant mechanism. USB now disconnected.

**G candidate:** preserves every event/full state/START/timestamp/order, reduces
integer-SRC snapshots 61→45 SysEx bytes (native encoded bytes 84→60). Exact v1
fallback for fractional/negative SRC. Sender replay keeps original format;
matching Pi accepts v1/v3. F telemetry, priority14/no-self-wake, deadlines and
128-frame playout unchanged. Hardware must show improvement; no fix claimed.
[Verified G pair](evidence/v01/compact-candidate.json), staged in
`private/v01/firmware-V01G/`; no deployment. Full sanitizer host suite, compact
codec/worker/scheduler/recovery, twelve-patch replay and compiled exchange pass
(12 remote packets, 0 malformed, stack 1368 B); paired Pi kernel builds.

**Next:** human can
insert SD for verified write/read-back/eject and flash G using Transfer, keeping
USB disconnected until matching Pi boot is verified. Trial starts muted at
32/32, then same SPD64 ramp; stop promptly on lag/fault/FIFO growth. Agent polls
remote diagnostics; no manual DIAG extraction. Startup/reconnect faults remain
open; Gates B persistence and C reconnect/soak still outstanding.

## Hardware / recovery

- Last human-reported installation: **V01F diagnostic**, Pi muted by agent.
  Restore I04R before normal use from
  `private/v01/firmware-V01P/Digitakt-I04R-previous.syx`; I04Q/H02F/stock also
  retained. I04R SHA256 `6a04ae9dd7791c7b11489af0061ea0012464922d5b6ca6935450f2b712f7fa2b`.
- Pi card updated to F SHA256 `0276c26b58092490f2b3b3fee89c99d4b4c8309144b0f53b13fbae0104354068`,
  128-frame playout; F boot verified. Immediate E rollback `8df83a5a…` is in
  `private/v01/sd-backup-before-duty/` and on-card pre-duty backup files.
  Earlier `c057…` remains in pre-remote backups.
  Earlier I04 rollback remains in `private/i04/sd-backup-event-worker/`.
- Fresh Pi reboot required for clean counter baselines. `trxtrace=on` forces
  mute; read status before dumping (printing caused underruns). Analyzer delay
  is **128**, not historical 2048. Preserve deadline/order/START behavior.
- Human handles flash; confirm availability/lowered speakers/disposable backups
  before new audio tests. Original user project and recorder settings preserved.

Implementation foundations and reproducible checks are in
[the I04 report](docs/I04-external-trx-voice.md). Twelve replayable patches target
sidecar-firmware `6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457`; mdkit is unchanged.
Keep firmware/raw captures private. Local user `PLAN.md` edit is excluded from
commits. No push/publishing.
