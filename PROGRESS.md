# Progress handoff

Task status/claims: [TASKBOARD.md](TASKBOARD.md). Pins: sources.json. Read AGENTS.md and PLAN.md before work. Earlier investigation is in Git and task reports.

## Current outcome

**I04 DONE by human acceptance of the one-voice prototype.** I04R firmware + Pi 128-frame/2.667 ms buffer now play real 44.1 kHz TRX-B2 through native SRC controls, locks/shallow LFO and local filter/AMP/sends; note/PTCH policy and sequenced velocity pass. Human: clean through final regression, timing/envelopes “super acceptable”; requested wrap-up. [Acceptance](evidence/i04/physical-acceptance.json), [report](docs/I04-external-trx-voice.md).

**Measured timing:** same 120 BPM native-click reference, analog relative onset median **11.417ms**, range 11.021–11.750 ms across 19 hits (old I04Q 115.833 ms). Short-run spread 0.729 ms; not calibrated manual-key latency or worst-case jitter. Clean timing interval: 20/20 STARTs including recovery, zero faults, FIFO 3 startup underruns unchanged; zero recorder drops. Pi observed max block 10 µs for 12 frames/250 µs, max control pass 18 µs, core 3≤3.5% during timed pattern; not dense worst-case CPU proof. [Timing](evidence/i04/event-worker-physical-timing.json).

**Known failure explicitly deferred to V01:** increasing LFO speed/amount (e.g. PTCH) can stop TRX audio and make Digitakt sluggish. Captured code3 late parameter event **260frames/5.417ms late**; host disarmed. Retained fault 1, FIFO later 29 versus clean 3; interval includes later listening/high modulation, no precise boundary. No OS crash dump; sender CPU/transport/host scheduling cause unisolated. [Failure/reproduction lead](evidence/i04/v01-dense-lfo-handoff.json).

## V01 — V01B and V01C both reproduce severe UI lag

Codex ownership renewed `2026-10-28T22:49:17Z`. V01 remains IN PROGRESS.
[Plan](docs/V01-persistence-stress.md), [trial runbook](docs/V01-gate-a-runbook.md),
[paired candidate evidence](evidence/v01/gate-a-candidates.json).

**Offline preparation passes.** V01B = priority-14/self-wake baseline; V01C =
same priority/no-self-wake. Both have corrected timing, machine-selection trial
reset, SECS stopwatch comparison, and sticky guard faults. Both build/boot and
pass panel, compiled reset/formatter, native sleep/wake/guards, and dense
compiled exchange/ACK/reconnect checks. Full I04 host regression and ten-patch
replay pass. Packages: `private/v01/firmware-V01B/` and `firmware-V01C/`, each
with I04R/I04Q/H02F/stock rollback. Both have since been installed for diagnostic trials; neither is production-qualified.
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

**V01C physical result:** human installed no-self-wake candidate; agent confirmed
Pi muted/trace off. Initial Pi control stream was inactive until a manual trig;
first activation raised FIFO 7→14 (unresolved), then it stayed stable. Active
32/32 Play/Stop was responsive, WGAP 34/BATCH 1/UGAP 36 ms, FAULT 0; no new
faults/underruns and all 20 new STARTs consumed. Attempting 64/64 again caused
extreme UI lag before Play; final achieved knobs/diagnostics were not extracted
because the panel was nearly unusable. Pi remained fault 0, FIFO 14, queue peak
4. [Control](evidence/v01/gate-a-v01c-control.json),
[failure](evidence/v01/gate-a-v01c-dense.json). No-self-wake alone is not a fix.

**USB isolation:** after unplugging USB, human reported about six seconds of
severe lag/BPM flashing, then normal UI response. Active USB-related work is
implicated; disconnect changes audio/MIDI/tasks/session together, so it does not
identify a particular sender call.

**Remote probe ready:** [V01E/Pi pair](evidence/v01/remote-candidate.json),
[procedure](docs/V01-remote-probe.md). One best-effort snapshot per render-second
reports UI/worker gaps plus raw DTIM0 ticks around masked native sends, worker
batches and UI service. Pi `v01status` exposes last snapshot; the agent polls it.
No manual DIAG extraction. Codec/device/actual Circle tests, full host suite,
eleven-patch replay, Pi cross-build and compiled emulator transport pass (12
snapshots, 0 malformed, native stack 1352 B). Pi SD updated below; Digitakt V01E not yet installed.
Package `private/v01/firmware-V01E/` contains Digitakt/stock recovery plus paired
Pi kernel and exact `c057…` pre-probe rollback. Hardware raw timer rate still
needs calibration; do not infer microseconds from ticks.

**Pi SD update complete:** [deployment](evidence/v01/remote-sd-deployment.json).
New kernel `8df83a5a…` read-back verified; all 21 manifest entries pass, 60 other
existing non-system files unchanged. Known `c057…` kernel and old manifest
backed up on card as `kernel8-rpi4.before-v01-remote.img` and
`SHA256SUMS.before-v01-remote`, also locally in
`private/v01/sd-backup-before-remote/`. Card safely ejected; boot not verified.

**Next physical action:** human returns SD and powers Pi, keeps Pi–Digitakt USB
disconnected, then uses Transfer to install V01E. Agent verifies Pi build,
re-mutes/disables trace after reconnection, and first checks remote progress and
raw timer against wall time before another dense attempt.

Earlier V01D/N/P times were inflated by 46.875×; UGAP 8469 is ~181 ms, not
8.469 s. Human-observed multi-second lag remains valid. Do not reuse old
candidate artifacts or their ms interpretations. Static native wait clears all
pending posts, so one-wait-per-old-credit accumulation is ruled out; repeated
self-post readiness remains a hypothesis. V01P priority 5 stays rejected.

## Hardware / recovery

- Last human-reported installation: **V01C diagnostic**, Pi muted by agent.
  Restore I04R before normal use from
  `private/v01/firmware-V01P/Digitakt-I04R-previous.syx`; I04Q/H02F/stock also
  retained. I04R SHA256 `6a04ae9dd7791c7b11489af0061ea0012464922d5b6ca6935450f2b712f7fa2b`.
- Pi card updated to SHA256 `8df83a5a2712d18429948dc2d77b4496a4efab545907b1cf2e9771bf31f2308e`,
  128-frame playout; boot pending. Immediate rollback `c057…` is in
  `private/v01/sd-backup-before-remote/` and on-card pre-remote backup files.
  Earlier I04 rollback remains in `private/i04/sd-backup-event-worker/`.
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
