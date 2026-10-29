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

**Native MIDI control component now runs:** integrated the inherited source6/
USB model work, corrected endpoint masks, residual bytes, descriptor chains,
prime ownership, OUT payload gating, W1C and controller RUN/IRQ masks. Native
submit→ISR209→ISR134 callbacks pass full/short/chained regressions. Replies now
use native OUT DMA, the receive job and compiled RX at IPL0; no ACK context
injection. [Evidence](evidence/v01/native-control-component.json).

**Fixed 500ms profiles:** eight early/steady-state button edges each; maximum
arrival→dispatch-pass completion3.242ms at assumed250MHz or7.323ms at200MHz.
Two complete250MHz reports match exactly. Gate1, one stable session, no observed
queue overflow/fatal flag, guard fault, artificial wait or decoder mismatch.
Exclusive estimated worker cycles ~0.85%→2.46%, USB IRQ209+134 ~0.12%→0.36% at
250MHz. These are zero-wait model costs, not hardware CPU measurements.
17 focused tests pass. **No physical lag reproduction or causal firmware fix.**

**Exact next work, offline:** native configuration enables USBINTR bit24
(recorded0x01000001), but the model generates only completion status1. Recover
and model the bit24 callback source (ISR134 at0x40007270), the status0x80 callback
source (ISR209 at0x400060f0), and native USB audio endpoint/descriptor traffic.
The current component seeds configured-controller/synchronized-route conditions;
it omits enumeration, USB audio/PCM and physical cache/DDR/bus costs. Establish
source-derived event semantics/cadence before adding them; do not fit delays.
[Continuation and commands](docs/V01-persistence-stress.md#native-midi-control-integration--continuation-2026-10-29).
Then repeat fixed profiles and isolate scheduling/ACK/USB/UI paths if lag appears.
Gate A remains IN PROGRESS/unaccepted; Gates B/C and startup FIFO issue remain
open. No new hardware candidate or flash requested.

**Physical state:** G pair installed/boot verified; last captured fault0/FIFO11,
muted/trace off. Unplug requested; recovery/disconnection confirmation pending.
Both watch captures are stopped. Boot→active FIFO0→11 remains an open startup
issue; no audible or dense G acceptance. User's `PLAN.md` edits were committed in `80b6618`.

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
Keep firmware/raw captures private. User `PLAN.md` edits were committed/pushed in `80b6618`; the
current continuation changes are local until separately published.
