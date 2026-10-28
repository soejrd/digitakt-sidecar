# I04 — external TRX-B2 voice

Status: **physical controls, locks/LFO, local processing and note/velocity pass
by listening; timing remains unacceptable/unqualified** (2026-10-28).
Owner: Codex. I04 is not DONE; V01 still depends on it.

## Hardware state and recovery

- Digitakt mk1 OS1.54 **I04Q**, human-installed, USB1 on audio track1. Firmware
  SHA256 `75f5b66ae5db95676eb767ece3239ec81a9281a42966fbca7689b22a8b14bb8b`.
- Pi4B combined final-SRC/velocity kernel **Oct28 2026 12:38:17** is physically
  booted. Kernel SHA256 `0c544fb5ddecb8aef61c0be6638ae608d296df688b639b18bd03daedbe1070ed`.
  Trace off; **explicitly muted after timing tests**. Recorder idle and original
  channel/bit-depth/prefix settings restored. Right output→left input loopback
  remains connected; human disabled external-input monitoring. Disposable test
  pattern has USB1 right/native track2 left; no original project was overwritten.
- SD update/read-back/manifests/backups and unchanged other files/settings pass.
  Card rollback: with Pi off restore `kernel8-rpi4.before-final-controls.img`
  as `kernel8-rpi4.img`, and `SHA256SUMS.before-final-controls` as `SHA256SUMS`.
  Local backup: `private/i04/sd-backup-final-controls/`. Prior I04 trace kernel
  and verified H02 fallback also retained privately.
- Firmware/stock recovery remain in `private/i04/firmware-I04Q/`. Only MAIN
  section3 changes; stock2/4/5/8 remain byte-identical. Human approval/backups
  recorded; human performed Transfer update. Follow the existing
  [human recovery gate](I02-one-track-routing.md#human-hardware-trial-and-recovery-gate).
  No agent flash, bootloader change, computer UI operation or publishing.

[Candidate/checks](../evidence/i04/final-controls-candidate.json),
[deployment](../evidence/i04/final-controls-sd-deployment.json),
[firmware approval](../evidence/i04/firmware-trial-approval.json).

## Physical acceptance evidence

| Check | Result and scope |
| --- | --- |
| SRC A–H | Human hears all controls respond, feels direct, audio continues through edits. [Initial I04Q results](../evidence/i04/i04q-physical.json) |
| Local processing | Human passes filter, AMP volume/decay, overdrive, delay/reverb sends and neighboring-track prompt. Local envelope starts before returned sound. |
| Locks/LFO | Alternating locks and shallow LFO audible. Initial sequenced SRC DIRT/DIST differed even with long AMP/open filter/dry sends; corrected kernel now passes DIRT/DIST and other locks by listening. [Correction retest](../evidence/i04/final-controls-physical.json) |
| Note/velocity | Notes leave pitch unchanged (intended). Initial sequenced VEL20/127 equally loud; corrected kernel now passes velocity by listening. Not a calibrated gain-curve measurement. |
| Corrected-kernel counters | Retest: 231/231 STARTs, zero faults, FIFO14 startup underruns unchanged over246s. Latest timing-session status: 325/325 STARTs, zero faults, FIFO still14 at uptime1665s. Mixed human tests/idle, not prescribed worst-case stress. |
| Return path | Five waveform matches: USB MAIN149frames/3.104ms; analog loopback median241frames/5.021ms, range239–242frames/0.063ms. Correlation >0.99. Recorder-relative; OUT reference alignment uncertainty up to112frames/2.333ms. [Results](../evidence/i04/physical-return-timing.json) |
| Sequenced relative timing | 21 native clicks every500ms: native-click→Pi-source median110.813ms, range110.438–111.104ms; native-click→analog onset median115.833ms, range115.417–116.104ms. Analog/native reference share the IN clock. This is relative audio-onset timing, not a calibrated trigger timestamp. [Results/method](../evidence/i04/physical-sequence-timing.json) |

Both timing takes have zero dropped blocks/inserted silence and no recorder
error; OUT reference shortages14/21 frames are reported separately. Five-channel
recording needed703KB/s; worst card write104/91ms, with no new engine FIFO
underruns. This does not establish worst-case Pi render/CPU margin. Raw WAVs,
full status/logs, firmware, emulator snapshots and personal assets stay private.

Return-path estimator matches200ms waveforms at integer-sample lags and rejects
weak/noise-only channels (<0.9 correlation). Sequenced onset estimator uses a
1ms trailing RMS crossing max(10% peak,3× pre-reference baseline), alongside
native-click first10%-peak sample. Different attack shapes, unknown native
sample leading silence and the short run limit absolute accuracy/jitter claims.
Synthetic known-lag, polarity/gain/DC, bleed, missing-middle-reference, silence
and dropped-recording rejection checks pass. No manual-key timing claim.

## Voice and scheduling contract

Pinned mdkit TRX-B2 `96157fe4fbda17bf4cae3168f52437d8232e6570` retains its
native44.1kHz synthesis, arithmetic and persistent DSP history. SRC A–H are
PTCH, DEC, RAMP, HOLD, TICK, NOIS, DIRT, DIST. Wire `voice` is a nonzero hit
generation; audio track1 is implicit. PTCH owns pitch; note does not transpose.

Digitakt supplies final lock/LFO/smoothed values. I04Q normalizes signed8.8 by
truncation/clamp to integer0..127 before comparison/publication. Every effective
change, START, note/velocity change and periodic64-block liveness record remains
ordered; the protocol stays61bytes. Parameter-only snapshots retain the tail.
The host synchronizes pinned public mdkit control-clock fields to these final
values, intentionally bypassing duplicate standalone knob smoothing. Render
phase, native clock phase, DSP history and pending-trigger rejection remain.

Velocity is clamped8.8/127 and latched on START only, applied once after the
fixed0.125 gain/±4095 int16 return limiter. Non-START metadata cannot alter the
hit's tail gain. Mono channel0 carries audio; channels1–7 remain zero. Local
Digitakt AMP/envelopes/filter/sends process the return afterward.

Causal128-tap/160-phase Blackman-windowed sinc resampling (20kHz cutoff) gives
exact160/147: 48,000 output frames consume44,100 native frames. History persists
through triggers, resets on invalidation. No allocation, USB or waits in render;
coefficients initialize before activation. Sampled18kHz +0.000496dB,22kHz
−81.08dB; designed delay63.5 native samples/~1.440ms. Host filter proof only.

Bounded256-snapshot scheduler owns complete snapshots before ACK/ACTIVATE.
First START anchors source block time to current render time+2048frames;
subsequent due times preserve32 output frames/block. Late/order/capacity/session/
horizon failures close the gate; no rebasing, coalescing or late replay. Native
32-sample engine boundaries require source STARTs at least2blocks/64outputframes
apart (1.333ms); closer STARTs fail explicitly. Session loss disarms.

**42.667ms is experimental added buffering, not accepted total latency.** The
physical relative offset is substantially larger. Current firmware worker runs
on the30Hz UI event; HELLO is staged before an early return, and first recovery
snapshot can age through handshake before the host anchors it. Those are source
facts; their exact shares of measured delay are not yet isolated. The native
software-timer task is about60Hz, so moving there alone is not a1ms solution.
Next investigation: bounded event-driven task service without USB in render,
then reduce buffer only against deadline/physical evidence and local alignment.

## Reproduced defects and checks

| Finding | Reproduction/fix evidence |
| --- | --- |
| Later hits muted | Host mistook hit generation for track ID. Production worker→real engine regression now retains generations1–4. [Evidence](../evidence/i04/generation-regression.json) |
| Fractional SRC overload | 128 records cover84.667ms source time but117.833ms requested time, worker gaps every63 records;119 were redundant fractional changes. I04Q suppresses them; physical sluggishness/faults disappear. [Trace](../evidence/i04/timing-trace.json), [candidate](../evidence/i04/effective-control-candidate.json) |
| Buffer-only attempt failed | Initial PTCH event608frames late; larger buffer still produced5faults/last20frames late. [Initial](../evidence/i04/ptch-failure.json), [larger-buffer failure](../evidence/i04/batch-fix-failure.json) |
| Duplicate smoothing | Production wrapper requested DIRT/DIST127/127 but used42/43.5 at consumed START. Final-control clock handoff now uses127/127, restores0/0 on tail, preserves DSP state. |
| Missing velocity | Unity gain failed20/127 PCM regression. New START-latched linear gain passes zero/negative/upper clamp, rejected START and tail retention; physical sequenced test passes. |
| I04Q native values | Compiled emulator PTCH lock72 at START, following unlocked60, LFO modulation and manual START ordering pass. Native seed fixtures, not physical timing. [Results](../evidence/i04/i04q-dynamic.json) |

Full ASan/UBSan real engine/scheduler/native worker/Circle/filter suite, captured
physical timing replay and nine-patch replay pass. Pi4 cross-build passes.
Host integration initially caught a stale overlay header/layout mismatch after
the gain field was added; test preparation now refreshes shared overlay before
compiling both C and C++, and the suite passes. Desktop benchmark is not a Pi
worst-case timing guarantee. I04Q build/lint/boot/protected-section/native encoder/
compiled exchange checks also pass; unchanged firmware since those checks.

Host changes replay as patches0001–0009 at sidecar-firmware
`6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457`, plus the tracked I04 overlay.
mdkit source remains unchanged; NOTICE retains licences. R05 is historical.

```sh
bash scripts/test_i04.sh
python3 scripts/test_i04_replay.py
# PY: interpreter with NumPy; raw timing takes are private.
"$PY" tests/i04_loopback.py
"$PY" tests/i04_sequence_timing.py
"$PY" tools/i04_analyze_loopback.py --folder private/i04/loop-wav/I04loop_001 \
  --capture private/i04/live/loop-end.json --output evidence/i04/physical-return-timing.json
"$PY" tools/i04_analyze_sequence_timing.py --folder private/i04/seq-wav/I04seq_001 \
  --capture private/i04/live/seq-end.json --output evidence/i04/physical-sequence-timing.json
```

## Checked I04R timing candidate — not installed

I04R (`usb-track`0.5) moves USB/control service into one native RTOS task,
priority14, with an aligned8KB stack and guards. Render publishes immutable
snapshots and issues only the bounded native semaphore post. RX capture signals
handshake replies immediately; active ACKs are processed on the next source/UI
watchdog wake. Four-record batches continue only when queue records can progress;
WAIT_ACTIVATE never busy-spins. HELLO flushes in the same pass. UI remains a
30Hz link/reconnect watchdog, not the normal control service clock. No hardware
timer, IRQ vector, bootloader or new code hook changes.

Generic RTOS exports are isolated in `core_candidate_rtos.*` for a future core
proposal. Native primitive ABI/addresses are OS1.54-specific and source-audited;
no dependency source change. IRQ/task publication is volatile. Standalone MIDI
traffic latency outside the integrated control path is not qualified.

Production-model negative control retains a48-block-old initial snapshot under
UI service and fails the age≤2 assertion; event service admits it0–1blocks old.
With128-frame scheduling,1000 dense integer updates,64STARTs and ACK loss/recovery
pass with1- and3-block worker-service cadence. Initial compiled native stress
failed with source queue full256/overflow1; removing redundant active-ACK wakeups
makes the unchanged every-block stress/recovery assertions pass.

Frozen I04R SHA256:
`6a04ae9dd7791c7b11489af0061ea0012464922d5b6ca6935450f2b712f7fa2b`.
Build/lint/link/protected sections, boot/assignment, all8encoders, native exchange,
loss/reconnect/congestion and genuine semaphore sleep/wake pass. Exchange observes
worker entries only at task IPL0;4441+ render signals at IPL5 in the optimized
prototype,4403 in the frozen final run; zero artificial semaphore satisfactions,
intact guards,1176/8192 bytes observed stack use. This is emulator scope, not a
hardware scheduling/CPU guarantee. Probes explicitly add the otherwise-missing
INTC2 source13 reschedule through the emulator's existing generic model.

Matching Pi kernel is privately staged with **128frames/2.667ms** added playout,
using replayable patch0010. Compiled first-anchor addition128 is verified in the
ELF; portable header default2048 remains for historical/legacy host regressions.
ASan/UBSan suite, ColdFire render no allocator/wait imports and ten-patch replay
pass. Current working I04Q kernel, previous I04Q firmware, H02F and stock1.54
recovery are retained. **Neither new firmware nor new SD kernel is installed.**

[Candidate/configuration/checks](../evidence/i04/event-worker-candidate.json),
[task runtime](../evidence/i04/event-worker-runtime.json),
[native exchange](../evidence/i04/event-worker-exchange.json),
[UI](../evidence/i04/event-worker-ui.json).

```sh
bash scripts/test_i04_event_worker.sh
# Expected failure control: UI timing still ages the initial snapshot.
bash scripts/test_i04_event_worker.sh legacy
# Exact private staged candidates (no hardware writes):
python3 scripts/i04_prepare_firmware.py --candidate private/i04-event-worker-final --label I04R
python3 scripts/i04_prepare_trial.py --evidence evidence/i04/event-worker-candidate.json --trial-name event-worker-final
```

## Remaining release conditions

Human installs the checked I04R/128-frame pair under the existing backup/recovery
gate. Start muted and inspect faults, then retest native controls and local
processing; repeat the same native-click/analog timing capture. Establish
acceptable measured latency/jitter and envelope alignment, and collect existing
Pi render/pass/CPU statistics. **2.667ms is a buffer configuration, not a measured
end-to-end result.** Preserve the working pair for rollback if deadlines fail.
Persistence/reconnect/dense stress qualification belongs to V01 after I04.
No other independent READY task currently exists.
