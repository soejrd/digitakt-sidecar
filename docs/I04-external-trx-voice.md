# I04 — external TRX-B2 voice

Status: **three physical hits audible; SRC edit failed; batching fix checked on
host, awaiting SD deployment and physical retest** (2026-10-28). Owner: Codex.
Physical Pi is explicitly muted. I04 is not DONE.

## Current implementation

One persistent real mdkit TRX-B2 uses the public C API at
`96157fe4fbda17bf4cae3168f52437d8232e6570`. Engine arithmetic is unchanged.
SRC A–H map to PTCH, DEC, RAMP, HOLD, TICK, NOIS, DIRT, DIST; signed 8.8 values
truncate and clamp to 0–127. Wire `voice` is a **nonzero hit generation**, not
track ID; the only supported audio track is implicit in the protocol. Every
START installs all eight values before triggering; parameter-only events keep
the tail. PTCH owns pitch; note does not transpose this percussion prototype.
Host velocity gain is unity, leaving Digitakt local AMP velocity behavior to be
verified physically. Mono channel 0 is returned; channels 1–7 are zero. Fixed
0.125 gain and ±4095 int16 ceiling limit output to about −18 dBFS before local
processing; converter overshoot can clip at that ceiling.

TRX-B2 remains at 44.1 kHz. `audio/src_44148.c` supplies causal 48 kHz output with
a 128-tap, 160-phase Blackman-windowed sinc, 20 kHz cutoff, exact 160/147 ratio,
and no native lookahead. Each 48,000 output frames consumes 44,100 native frames.
History persists through STARTs, resetting on session invalidation. Coefficients
initialize before activation. Render neither allocates, waits nor calls USB.
R05's original 32-tap benchmark remains historical and unchanged.

`audio/playout.c` owns **256 bounded snapshots**. First START anchors sender
block time to current render time plus **2,048 frames (42.667 ms)**. Subsequent
deadlines preserve sender block differences at 32 output frames/block, including
wrap and events falling within Circle's 12-frame render blocks. Admission copies
an immutable snapshot before ACTIVATE: ACK means the scheduled queue owns it;
synthesis fires only at its deadline. Late/order/session/capacity/horizon faults
invalidate and disarm; no silent coalescing, late replay or deadline rebasing.
The future horizon is one second. Native mdkit trigger boundaries require at
least 35 output frames; sender granularity makes the supported START minimum
64 frames / two blocks (1.333 ms). Pending triggers and adjacent-block STARTs
fail closed instead of merging hits.

**Timing is unqualified.** The increased buffer covers the modelled normal
50-block (~33.3 ms) control-worker batching with margin. It does not remove
initial transport, engine quantization, filter/USB/FIFO delay, clock drift, or
align the Digitakt's current local locks with delayed audio. Worst-case slower
workers can still fail. 42.667 ms is an experimental extra buffer, not an
accepted musical latency or a measured total. A faster firmware worker,
lookahead/compensation or a revised contract may be needed after measurements.

## Safety, replay and diagnostics

Apply patches **0001–0008** at sidecar-firmware
`6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457`, then use the I04 overlay. mdkit
inputs must match their pin and remain unchanged. NOTICE preserves licences;
no firmware-derived code/data enters the host overlay. The stock Rytm selection
path remains separate. Private firmware, captures, kernels and rollback files
stay ignored. The current fix is in tracked overlay sources, not a dirty-only
dependency edit. No new Digitakt firmware flash is required; H02F remains.

TRX audio defaults **off**. `:trx=on` requires an active USB1 session, binds to it
and fades over 200 ms. `:trx=off` silences newly rendered PCM. Loss/reconnect or
invalidation disarms; enabling never changes timestamp mapping. `:trxstatus`
reports arm, queue/peak, lifetime fault count, applied/START/consumed counts and
render frame. Last fault code/block/flags/delta/now/due survives session resets.
Codes: 1 invalid, 2 order/session, 3 late, 4 horizon, 5 capacity, 6 START spacing,
7 engine install/pending trigger, 8 missed render deadline. Fixed metadata is
captured at failure and printed only on status requests. Per-session counters
reset; they cannot count all taps across invalidations. Pi max-block timing
includes synthesis work.

## Physical findings and regression evidence

| Check | Result and scope |
| --- | --- |
| Initial corrected routing candidate | Expected build/USB streaming; human heard no taps, 37 playout faults, underruns rose 8→1,828. [Failure](../evidence/i04/physical-silence.json) |
| Diagnostic single muted tap | Code 1, START flag; invalid-generation rejection. [Baseline](../evidence/i04/diagnostic-baseline.json), [red/green generation regression](../evidence/i04/generation-regression.json) |
| Generation fix | Both host layers wrongly required `voice == 1`; real worker increments it each hit. Accepting nonzero generations fixes the host reproduction. Isolated old fixtures used constant 1 and missed the bug. |
| Corrected physical muted tap | START/consumed 1→2, zero faults, underruns stayed 15. Cable was changed near startup; this is not formal reconnect qualification. [Baseline](../evidence/i04/generation-fix-baseline.json), [tap](../evidence/i04/generation-fix-one-tap.json) |
| Three audible corrected hits | Human confirmed three hits; START/consumed 2→5, arm 1, zero faults; underruns stayed 15. This proves audible hits only. [Evidence](../evidence/i04/physical-audible.json) |
| PTCH change | Human reported sound stopped immediately on changing PTCH. Code 3 parameter-only event was 608 frames / 12.667 ms late; faults reached 33. Explicit mute accepted. [Evidence](../evidence/i04/ptch-failure.json) |
| Bounded control-window monitor | USB kept streaming with no new USB gaps/errors, but FIFO underruns increased. It overlaps failed qualification; no stable audio/headroom proof. Details are in PTCH evidence. |

The generation regression executes real firmware render/queue/worker/native
transport → portable host → scheduled real engine. Before its fix, native hit 2
reset the session; afterward generations 1–4 were retained and consumed.

The expanded test adds 1,000 per-block SRC edits with both slow initial service
(50 blocks) and fast startup followed by normal 50-block batches. With the old
1,024-frame delay it reproduces **code 3** in the fast-start case (now 36672,
due 36160). Slow startup additionally needs more queue space: a 1,024-delay
run with enlarged capacity peaked at 132 pending events. With delay 2,048 and
capacity 256, both cases retain the session and apply 1,008 / 1,007 snapshots
including periodic snapshots, queue peaks 164 / 66. No scheduling guard was
removed. This is a host correction to the reproduced batching fault; the new
buffer/capacity are not yet physically qualified.

## Other executed checks

- ASan/UBSan engine, scheduler, native bridge and actual Circle glue/Output tests
  pass: PCM block invariance, exact rate, generation handling, native START
  consumption, timing/order/wrap/capacity/late guards, arm/fade, loss/reconnect,
  congestion, last-fault preservation and default silence.
- [Filter](../evidence/i04/filter.json): 18 kHz gain +0.000496 dB, 22 kHz rejection
  −81.08 dB, worst sampled 22–22.05 kHz rejection −80.15 dB. Finite grids do not
  prove every frequency or final analog/int16 quality. DC/count checks pass.
  Input impulse 64 peaks at output 139; designed delay 63.5 native samples
  (~1.440 ms), excluding engine/transport/device delays.
- [Current dense host benchmark](../evidence/i04/playout-benchmark.json): optimized
  desktop ten-second synthesis/queue workload; no USB/Pi worst-case scheduling
  claim. Pi kernel build and eight-patch replay pass; candidate hashes identify
  exactly which configuration was built. Older candidate evidence is historical.

```sh
bash scripts/i04_prepare_circle.sh
bash scripts/test_i04.sh
python3 scripts/test_i04_replay.py
# Negative control; expected failure with old buffer:
I04_TEST_FLAGS=-DSC_PLAYOUT_DELAY=1024u bash scripts/test_i04_generation.sh
bash deps/sidecar-firmware/pi/engine/build.sh kernel
```

## Hardware state, release condition and recovery

The generation-fix kernel is currently on the physical Pi, explicitly muted
after PTCH failure. Existing SD updates were read-back verified, safely ejected,
and preserved settings and previous backups:
[initial](../evidence/i04/sd-deployment.json),
[diagnostic](../evidence/i04/diagnostic-sd-deployment.json),
[generation fix](../evidence/i04/generation-fix-sd-deployment.json).

Next: human powers Pi off and returns SD; verify current kernel/manifest, back
up both under fresh names, install/read-back the batch-fix candidate and safely
eject. Boot muted, capture baseline, test hits and SRC edits muted for faults,
then explicitly enable limited/faded audio with speakers lowered. Verify actual
SRC A–H, locks/LFOs/retriggers, note/velocity, local filter/AMP/sends and unaffected
neighboring track. Identify a usable capture channel before deriving latency;
H02's failed USB MAIN pulse matching is not a valid timing reference. Analog
loopback may be needed. Measure latency/jitter/local-lock alignment, queue margin,
Pi worst-case scheduling, stability and reconnect before timing acceptance.

For rollback, power Pi off and restore a matching backed-up kernel **and full
manifest** pair. Generation-fix deployment preserved diagnostic-1 as
`kernel8-rpi4.before-generation-fix.img` / `SHA256SUMS.before-generation-fix`.
Verified H02 kernel remains `private/h02/trial/kernel8-rpi4-clock.img`; stock
Digitakt recovery is `private/h02/trial/Digitakt-OS1.54-stock-recovery.syx` under
the existing human procedure. Do not restore an older full manifest blindly
with a newer kernel. No bootloader/internal wiring changes.

I04 remains incomplete. V01 depends on it; no independent READY task exists.
