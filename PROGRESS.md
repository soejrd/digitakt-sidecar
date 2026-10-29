# Progress handoff

Task status/claims: [TASKBOARD.md](TASKBOARD.md). Pins: sources.json. Read AGENTS.md and PLAN.md before work. Earlier investigation is in Git and task reports.

## Current outcome

**I04 DONE by human acceptance of the one-voice prototype.** I04R firmware + Pi 128-frame/2.667 ms buffer now play real 44.1 kHz TRX-B2 through native SRC controls, locks/shallow LFO and local filter/AMP/sends; note/PTCH policy and sequenced velocity pass. Human: clean through final regression, timing/envelopes “super acceptable”; requested wrap-up. [Acceptance](evidence/i04/physical-acceptance.json), [report](docs/I04-external-trx-voice.md).

**Measured timing:** same 120 BPM native-click reference, analog relative onset median **11.417ms**, range 11.021–11.750 ms across 19 hits (old I04Q 115.833 ms). Short-run spread 0.729 ms; not calibrated manual-key latency or worst-case jitter. Clean timing interval: 20/20 STARTs including recovery, zero faults, FIFO 3 startup underruns unchanged; zero recorder drops. Pi observed max block 10 µs for 12 frames/250 µs, max control pass 18 µs, core 3≤3.5% during timed pattern; not dense worst-case CPU proof. [Timing](evidence/i04/event-worker-physical-timing.json).

**Known failure explicitly deferred to V01:** increasing LFO speed/amount (e.g. PTCH) can stop TRX audio and make Digitakt sluggish. Captured code3 late parameter event **260frames/5.417ms late**; host disarmed. Retained fault 1, FIFO later 29 versus clean 3; interval includes later listening/high modulation, no precise boundary. No OS crash dump; sender CPU/transport/host scheduling cause unisolated. [Failure/reproduction lead](evidence/i04/v01-dense-lfo-handoff.json).

## V01 — Gate A emulator reproduction and causal isolation

**V01 IN PROGRESS, Codex owner.** Authoritative next-work sequence:
[Gate A emulator investigation](docs/V01-persistence-stress.md#current-next-work--emulator-reproduction-and-causal-isolation).
This is part of V01, not V02. Gates B persistence/edit semantics and C recovery/
soak remain required. No proven root cause or solution; CPU saturation is not
established. Existing functional emulator passes do not assert panel latency
and simplify USB completions/ACK execution context.

**Latest hardware result:** [G SPD64/DEP48 fails](evidence/v01/compact-depth48-result.json),
quite laggy with fault/guards0 and FIFO11 stable. [32/32](evidence/v01/compact-baseline.json)
short counter baseline passed; [64/32](evidence/v01/compact-speed-result.json)
felt slightly sluggish. Compact snapshots did not solve lag. Earlier B/C fail;
P lower priority improved UI but failed delivery. [F](evidence/v01/duty-dense-result.json)
and [E](evidence/v01/remote-severe-result.json) retain useful negative findings.
Worker/render elapsed brackets overlap; never sum as CPU utilization. UI tick
gaps are not panel latency. Old D/N/P ms displays were inflated by46.875×.

**Next work, offline:** audit emulator substitutions; run native routines with
cycle timing assumptions recorded; reproduce dense input→UI dispatch latency;
attribute self execution, IRQs and runnable/blocked time; isolate wake/dispatch,
ACK processing, native USB and UI backlog one path at a time. Planned probe
`tools/v01_contention_probe.py` does not exist yet. Establish a deterministic
failing panel-service test before claiming a fix. If emulation disagrees,
document the concrete missing model behavior and justify one targeted hardware
measurement. No speculative new firmware or repeated depth sweep. Full steps,
hypothesis predictions, deliverables and physical exit criteria live in Gate A.

**Physical state:** G pair installed/boot verified; last captured fault0/FIFO11,
muted/trace off. Unplug requested; recovery/disconnection confirmation pending.
Both watch captures are stopped. Boot→active FIFO0→11 remains an open startup
issue; no audible or dense G acceptance. User's `PLAN.md` edit is preserved.

## Hardware / recovery

- Last human-reported installation: **V01G diagnostic**, Pi muted by agent.
  Restore I04R before normal use from
  `private/v01/firmware-V01P/Digitakt-I04R-previous.syx`; I04Q/H02F/stock also
  retained. I04R SHA256 `6a04ae9dd7791c7b11489af0061ea0012464922d5b6ca6935450f2b712f7fa2b`.
- Pi card updated to G SHA256 `2b027a172e089af8fc273e4b2351d7fd72a7223b92f8ac6dfc8b20c491e2df5f`,
  128-frame playout; G boot verified. Fresh on-card pre-compact kernel/manifest
  and `private/v01/sd-backup-before-compact/` retain F `0276c26b…`.
  Earlier E rollback `8df83a5a…` is in
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
