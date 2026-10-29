# Progress handoff

Task status/claims: [TASKBOARD.md](TASKBOARD.md). Pins: sources.json. Read AGENTS.md and PLAN.md before work. Earlier investigation is in Git and task reports.

## Current outcome

**I04 DONE by human acceptance of the one-voice prototype.** I04R firmware + Pi 128-frame/2.667 ms buffer now play real 44.1 kHz TRX-B2 through native SRC controls, locks/shallow LFO and local filter/AMP/sends; note/PTCH policy and sequenced velocity pass. Human: clean through final regression, timing/envelopes “super acceptable”; requested wrap-up. [Acceptance](evidence/i04/physical-acceptance.json), [report](docs/I04-external-trx-voice.md).

**Measured timing:** same 120 BPM native-click reference, analog relative onset median **11.417ms**, range 11.021–11.750 ms across 19 hits (old I04Q 115.833 ms). Short-run spread 0.729 ms; not calibrated manual-key latency or worst-case jitter. Clean timing interval: 20/20 STARTs including recovery, zero faults, FIFO 3 startup underruns unchanged; zero recorder drops. Pi observed max block 10 µs for 12 frames/250 µs, max control pass 18 µs, core 3≤3.5% during timed pattern; not dense worst-case CPU proof. [Timing](evidence/i04/event-worker-physical-timing.json).

**Known failure explicitly deferred to V01:** increasing LFO speed/amount (e.g. PTCH) can stop TRX audio and make Digitakt sluggish. Captured code3 late parameter event **260frames/5.417ms late**; host disarmed. Retained fault 1, FIFO later 29 versus clean 3; interval includes later listening/high modulation, no precise boundary. No OS crash dump; sender CPU/transport/host scheduling cause unisolated. [Failure/reproduction lead](evidence/i04/v01-dense-lfo-handoff.json).

## V01 — severe case measured; aggregate elapsed probe ready

V01 IN PROGRESS, Codex owner. [Plan](docs/V01-persistence-stress.md),
[remote procedure](docs/V01-remote-probe.md). B/C both failed dense settings;
priority-5 V01P rejected earlier, no-self-wake alone insufficient. USB removal
restores UI after a few seconds. The agent controls Pi mute/trace/status via
Sidecar HTTP; human handles Digitakt panel/Transfer and Pi SD insertion.

**Installed pair:** human confirmed V01E and Pi remote kernel `8df83a5a…`.
Pi boot/receiver verified; SECS and read-only DTIM0 calibrated at ~132 MHz,
mode 3. [Boot](evidence/v01/remote-boot.json),
[calibration](evidence/v01/remote-control-calibration.json).
32/32 muted active baseline: FIFO 14 stable, faults 0. SPD64/DEP32 mild lag,
approximate onset above SPD40, reversible when SPD returns to 32. Native send
rate ~284→533/s. [Reversal](evidence/v01/remote-speed-reversal.json).

**Severe result:** human confirms SPD64/DEP100 (not a fixed 64/64 trial), about
3 fps; unplugging restored response after uncertain few seconds. Recorder
stopped at first new FIFO underrun, 14→101; log shows 145 before unplug and
7 retained late faults queried afterward. Longest send critical section ~23.8 us,
aggregate measured send critical fraction ~1.5% near first FIFO failure. Final
retained UI service gap ~215 ms, worker batch elapsed max ~8.7 ms. Pi core-3
average about 4.3%, block max 10 us. These argue against long masked sender
calls alone as the dominant mechanism; aggregate worker/render occupancy and
other task/IRQ work remain unmeasured. [Result](evidence/v01/remote-severe-result.json).
No raw tick UI gap is a direct input latency or screen-FPS measurement.

**Next probe V01F:** [verified pair](evidence/v01/duty-candidate.json), package
`private/v01/firmware-V01F/`. Adds cumulative worker and native-render elapsed
ticks via existing core events; version-2 packet 92 bytes, same low rate,
priority14/no-self-wake/deadlines. Pi accepts/tags legacy v1 packets. Worker
elapsed includes preemptions and overlaps renderer: never sum as pure CPU.
Host/sanitizer/codec/receiver tests, twelve-patch replay, Pi build and compiled
exchange pass (12 snapshots, 0 malformed, both totals advance, stack 1368 B).
No F image installed. Exact immediate Pi rollback is the currently installed
E kernel `8df83a5a…`; accepted-prototype `c057…` remains available.

**Next human step:** keep USB disconnected; power off Pi and insert SD into Mac
for backed-up F kernel update, then human V01F Transfer flash. Agent first
verifies version-2 counters/calibration at 32/32. No more manual page extraction.
Watch tool now saves each sample continuously and supports a stop marker.
The early activation FIFO rise and hotplug fault remain separate open issues.

## Hardware / recovery

- Last human-reported installation: **V01E diagnostic**, Pi muted by agent.
  Restore I04R before normal use from
  `private/v01/firmware-V01P/Digitakt-I04R-previous.syx`; I04Q/H02F/stock also
  retained. I04R SHA256 `6a04ae9dd7791c7b11489af0061ea0012464922d5b6ca6935450f2b712f7fa2b`.
- Pi card updated to SHA256 `8df83a5a2712d18429948dc2d77b4496a4efab545907b1cf2e9771bf31f2308e`,
  128-frame playout; E boot/receiver verified. Immediate rollback `c057…` is in
  `private/v01/sd-backup-before-remote/` and on-card pre-remote backup files.
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
