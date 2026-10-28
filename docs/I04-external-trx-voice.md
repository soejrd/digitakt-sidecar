# I04 — external TRX-B2 voice

Status: **generation-rejection bug fixed on host; BLOCKED on corrected physical retest** (2026-10-28).
Owner: Codex; scheduling/filter follow-up claimed 07:11 UTC.
The SD candidate is installed/read-back verified and physically booted; explicit
TRX enable was accepted, but the human heard no taps; 37 playout faults disarmed
the session. TRX is explicitly muted. Listening/timing acceptance has failed so far.
No Digitakt firmware was flashed.

## Candidate behavior

`audio/trx_voice.c` owns one persistent real mdkit TRX-B2 at
`96157fe4fbda17bf4cae3168f52437d8232e6570`, using its public C API without
changing synthesis arithmetic. SRC A–H map to PTCH, DEC, RAMP, HOLD, TICK, NOIS,
DIRT, DIST. Signed 8.8 controls truncate and clamp to 0–127. Parameter-only
snapshots preserve the tail. START installs all controls before calling trigger.

**Note/velocity policy:** SRC PTCH owns pitch; note does not transpose this
percussion prototype. Host velocity gain is unity; Digitakt's existing local
AMP owns velocity. Host PCM independence is tested; the physical velocity curve
remains unverified. Mono return is channel 0, with channels 1–7 zero. Fixed
0.125 gain and a hard ±4095 int16 ceiling limit output to about −18 dBFS before
local processing. The ceiling can clip converter overshoot.

TRX-B2 stays at 44.1 kHz. The new `audio/src_44148.c` is a causal 128-tap,
160-phase, Blackman-windowed sinc converter with a 20 kHz cutoff and exact
160/147 conversion. It consumes 44,100 native frames per 48,000 output frames;
no native lookahead or R05's 31 extra priming frames are generated. History
persists across STARTs and resets on session invalidation. Coefficients are
initialized before activation. Render never allocates, waits, or calls USB.
R05 remains an unchanged historical benchmark of its earlier 32-tap converter.

## Timestamped playout and limits

`audio/playout.c` owns a bounded 64-snapshot queue. The first START anchors the
sender's block clock to the current render frame plus **1,024 output frames
(21.333 ms)**. Later deadlines preserve the exact sender block differences at
32 output frames per block. Enqueue copies complete immutable snapshots before
ACTIVATE; ACK means state is validated and owned by the scheduled queue, not
that synthesis has already fired. Each event applies at its due output sample,
including when a 12-frame Circle block straddles the deadline. Parameter-only
snapshots retain the same schedule. Clock wrap is extended using positive
32-bit deltas; sequence/session changes are rejected unless the session resets.

Late arrival, queue overflow, invalid ordering/session, more than one second
of future events, or a trigger still pending at the next START fail closed.
A fault closes the output gate and requires a fresh session. There is no late
replay, silent event coalescing, or dynamic rebasing to conceal missed deadlines.
No event allocation or USB wait occurs during rendering.

**Native trigger limit:** mdkit consumes triggers on 32-native-sample boundaries.
At least 35 output frames between STARTs guarantee consumption opportunity;
the sender's block granularity makes the supported minimum **two blocks / 64
output frames (1.333 ms)**. Adjacent-block STARTs explicitly invalidate the
session. The scheduler also checks actual pending-trigger state. This preserves
native engine behavior rather than changing its internal block clock. Native
trigger consumption can lag its scheduled application by up to about 0.73 ms;
filter delay is additional. Dense tests check consumed triggers, not only calls.

The 21.333 ms buffer is an **experimental, uncalibrated** starting value, not an
accepted latency target. It absorbs arrival variation within its available
margin, measured from the first START's arrival on the render timeline. It
does not remove initial transport latency, output FIFO latency, clock drift,
or align local Digitakt filter/AMP locks with delayed remote audio. A late/drift
fault is deliberately visible. Hardware measurements must decide whether a
smaller buffer, lookahead or device-side compensation is required. No musical
timing acceptance is claimed from host tests.

## Safety, dependency replay and diagnostics

Patches **0001–0007**, in order at sidecar-firmware
`6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457`, plus the I04 overlay reproduce the
candidate. The overlay verifies the pinned, unchanged mdkit inputs; NOTICE
retains licences. No firmware-derived code or data enters the host overlay.
The stock Rytm selection path is unchanged. The H02 tone regression API remains;
production integrated rendering uses TRX-B2.

**TRX audio defaults off.** Core-3 console command `:trx=on` requires a valid
USB1 session, binds enable to that session and fades in over 200 ms. `:trx=off`
silences subsequent generated PCM. Loss, timeout, invalidation or reconnect
removes enable; a new session needs a new explicit enable. The synthesis and
scheduler continue while muted so enabling does not reset timestamp mapping.
`:trxstatus` logs armed state, queue occupancy/peak, lifetime playout-fault count,
applied snapshots, STARTs, consumed STARTs and low 32 bits of render frame.
Per-session counters reset on invalidation. The existing maximum-block timing
now includes TRX rendering, so later Pi measurements do not omit its work.

## Executed evidence

[Host/build evidence](../evidence/i04/host.json),
[filter measurements](../evidence/i04/filter.json),
[dense host benchmark](../evidence/i04/playout-benchmark.json).

- ASan/UBSan: whole/ragged engine PCM equality, note/velocity policy, exact rate,
  clipping bounds, native START consumption, reset and zero-generation checks.
- Timestamp tests: exact output-sample application, burst versus delayed-arrival
  PCM equality, timestamp wrap, 64 queued snapshots with 32 separately consumed
  STARTs, and fail-closed capacity/late/order/session/horizon/too-close cases.
- Actual Circle glue/Output/native bridge: handshake, explicit arm/default
  silence/fade, loss/timeout silence, reconnect disarm, ACK congestion and
  scheduler rejection closing the session. These are production-source host
  tests, not USB DMA or hardware audio measurements.
- Filter: 1 kHz RMS 0.070710648 for amplitude 0.1; 18 kHz gain +0.000496 dB;
  22 kHz rejection −81.08 dB. Worst rejection over sampled 22,000–22,050 Hz
  (10 Hz grid) is −80.15 dB; sampled first-image rejection exceeds 115 dB for
  1–18 kHz tones (1 kHz grid). These finite-grid float tests do not establish
  every-frequency or final int16/analog performance. DC phases and count pass.
- Causal impulse: input frame 64 peaks at output frame 139; designed group
  delay is 63.5 native samples (~1.440 ms), excluding engine/USB/FIFO/device.
- Optimized desktop dense run: ten seconds of synthetic audio; 14,968 snapshots
  applied, 7,484 STARTs consumed, queue peak 33/64, zero faults. Wall time and
  observed maximum block time are recorded as **host-only** results, not Pi
  worst-case scheduling/thermal/headroom evidence.
- Pi 4 AArch64 Circle cross-build and seven-patch replay pass. Kernel hash/size
  and source hashes are recorded. The kernel was subsequently deployed to the SD as recorded below.

Reproduce from this repository after applying the patches at the pinned base:

```sh
bash scripts/i04_prepare_circle.sh
bash scripts/test_i04.sh
python3 scripts/test_i04_replay.py
bash scripts/test_i03.sh
bash scripts/test_i03_handshake.sh
bash deps/sidecar-firmware/pi/engine/build.sh kernel
python3 scripts/i04_prepare_trial.py
```

The staging command requires matching current build/source evidence and refuses
to overwrite a different trial. It creates only ignored `private/i04/trial/`
kernel, checksum, manifest and verified H02 kernel rollback. It checks that the
existing H02F firmware and stock recovery exist; I04 needs no new Digitakt flash.

## Physical release condition and next action

Software work is ready for a **human-supervised hardware qualification session**.
Make the Pi/Digitakt available with speakers lowered. Keep the Pi off for any SD
replacement. The current working SD kernel must be backed up and hashed before
install; retain settings/credentials, boot files and H01/H02 backups. Verify
read-back and update only the kernel's entry in the existing SD checksum
manifest. The staged rollback matches the physically booted H02 kernel hash
recorded in `evidence/h02/sd-clock-deployment.json`.

Boot with TRX muted. Confirm handshake/status and zero playout faults, then
explicitly enable the limited/faded voice. Demonstrate native SRC edits,
sequencer locks, LFOs, retriggers, note/velocity policy, local filter/AMP/sends
and an unaffected neighboring internal track. Capture source/render counters
and identify a usable return capture channel before deriving latency; H02's
failed USB MAIN pulse matching is not a valid latency reference. Use an analog
loopback if needed and available. Measure integrated latency/jitter, local-lock
alignment, Pi maximum render/queue margin, stability and reconnect. Choose an
acceptable timing budget and calibrate the playout buffer from those results.

If testing fails, disarm, stop the stream, power Pi off and restore the verified
H02 kernel as `kernel8-rpi4.img`, restoring its checksum entry. Keep the pre-I04 SD manifest backup and verify all retained entries; the older
`private/h02/sd-backup-clock/SHA256SUMS` belongs to the earlier kernel and must
not be restored blindly with the newer rollback. The staged rollback is copied
from `private/h02/trial/kernel8-rpi4-clock.img` and verified against H02
deployment evidence. Existing H02F firmware remains in place.
Stock firmware recovery remains `private/h02/trial/Digitakt-OS1.54-stock-recovery.syx`
under the existing human recovery procedure. No bootloader/wiring changes.

I04 is not DONE until this physical behavior and measured acceptable timing pass.
V01 remains dependent; no other independent READY task exists. Persistence,
calibrated semitone mapping, broad stress/reconnect and production audio quality
remain outside the host proof.

## SD deployment — 2026-10-28

[Deployment evidence](../evidence/i04/sd-deployment.json): human confirmed Pi off
and speakers lowered. The card's current kernel matched the physically booted
H02 rollback. All existing manifest entries passed before replacement. Current
kernel and manifest were backed up locally in `private/i04/sd-backup/` and on
card as `kernel8-rpi4.before-i04.img` / `SHA256SUMS.before-i04`. Candidate and
updated manifest passed read-back; all other regular card files, including
settings, retained their hashes. The card was safely ejected. Boot and physical
audio are not yet tested. Restore the pre-I04 card kernel and complete pre-I04
manifest together for exact rollback. Next: human inserts SD into powered-off
Pi, connects Digitakt as before and powers up; inspect silent session before
explicitly enabling audio.

## Physical boot and first enable — 2026-10-28

[Boot evidence](../evidence/i04/physical-boot.json): the human installed the SD,
connected/powered the devices and confirmed speakers lowered. HTTP status reports
expected build `Oct 28 2026 07:20:19`, USB streaming and MAIN output. Muted TRX
status had arm 0, queue 1, peak 2, zero playout faults, and one START consumed.
Explicit `trx=on` was accepted; subsequent arm 1 / zero-fault status confirms
session-bound enable. FIFO underruns stayed at startup baseline 8 through enable.
This is not a zero-underrun boot or stress proof. Human was asked to select the
existing USB1 track and tap trigs; audible behavior is not yet confirmed. Leave
speakers low; audio is currently enabled, level-limited and subject to loss disarm.

## Physical silence and diagnostic follow-up — 2026-10-28

[Failure evidence](../evidence/i04/physical-silence.json): human reported many
manual taps with no audible sound. Focused live counter check was RED:
`armed=0, faults=37, starts=1, consumed=1`. FIFO underruns increased from startup
8 to 1,828. START counters are for the latest session only; invalidations reset
them, so they do not count all human taps. Explicit `trx=off` was accepted.
No audible success or physical timing result is claimed.

Ranked hypotheses, not findings: late events relative to fixed deadlines;
adjacent-block START rejection; host scheduling stalls contributing both late
events and FIFO starvation. The installed counter has no reason field, so none
can be selected as the root cause yet. The private live assertion reproduces
physical invalidation; it is not an unattended acoustic measurement.

Patch 0008 adds bounded last-fault metadata, retained separately across resets.
`:trxstatus` reports code, sender block/flags/delta and low 32 bits of render
clock/deadline. Codes 1–8 are invalid frame, order/session, late, future horizon,
capacity, START spacing, engine install/pending-trigger, missed render deadline.
Metadata storage occurs at failure; printing occurs only on the status command.
No extra audio-callback logging is added. Host tests assert exact reason codes
and preservation across actual Circle invalidation/reconnect. This is diagnostic
instrumentation, **not a fix**. Eight-patch replay and Pi build pass.

[Diagnostic candidate](../evidence/i04/diagnostic-candidate.json) pins sources and
kernel. It is staged in ignored `private/i04/diagnostic-1/` with verified H02
rollback. Stage/recheck it with:

```sh
python3 scripts/i04_prepare_trial.py \
  --evidence evidence/i04/diagnostic-candidate.json --trial-name diagnostic-1
```

The original trial and host source-hash evidence remain historical; their staging
check deliberately refuses current changed sources. The diagnostic supports a
separate named private trial without overwriting that original candidate.

Release condition: human powers Pi off and returns SD; back up its current
kernel/manifest, deploy/read-back diagnostic-1, eject and boot muted. Capture
fault metadata during one tap, then repeated taps. Turn that observed failing
scenario into a correct host regression, fix the identified cause, and repeat
physical listening/transport checks. No other independent READY task exists.

## Diagnostic SD deployment — 2026-10-28

[Diagnostic deployment](../evidence/i04/diagnostic-sd-deployment.json): returned
SD current kernel matched the original I04 candidate. Existing manifest passed.
Current kernel/full manifest were backed up locally and on card with
`before-diagnostic-1` names; all earlier backups/settings retained their hashes.
Diagnostic kernel and updated manifest passed read-back; card safely ejected.
No diagnostic boot, root-cause fix or audio success is claimed. Next: reinsert
into powered-off Pi, boot muted and inspect counters before controlled taps.
Rollback the kernel and full manifest from the matching `before-diagnostic-1`
backup pair to restore the immediately previous I04 state; verified H02 rollback
remains available separately.

## Single-tap diagnosis and generation fix — 2026-10-28

[Baseline](../evidence/i04/diagnostic-baseline.json): diagnostic build booted muted,
zero playout faults, 16 startup FIFO underruns. One human tap while stopped
produced exactly one fault: code 1, START flags 1, sender block 106307, delta 1,
render frame 2644884. USB stayed streaming; underruns became 17. The fault was
an invalid-frame rejection, not the deadline or minimum-START-spacing guard.

[Regression evidence](../evidence/i04/generation-regression.json): the production
firmware worker increments wire `voice` for each START. It is a **hit generation,
not a track ID**; track identity is implicit in this single-track protocol. I04
incorrectly required `voice == 1` in both queue admission and engine installation.
The first recovery START worked, but the next native hit advanced the generation
and invalidated/disarmed the session. This explains the silent physical path;
per-session START counts reset during that invalidation.

The new regression invokes the real firmware render/queue/worker and native
transport with the scheduled real engine as its host installer. Before the fix,
`bash scripts/test_i04_generation.sh` failed on hit 2 with session reset. After
accepting nonzero generations in both layers, hits 2, 3 and 4 retain the same
session and each is consumed by TRX-B2. Zero remains rejected. Wire field
semantics are now explicit in `control/link.h`; scheduler tests advance generation
instead of freezing it at 1. The prior isolated engine/queue tests used constant
1 and therefore missed this integration error. Full host suite/eight-patch replay
pass; physical correction remains to be tested.

No timing, spacing or filter policy was relaxed. Diagnostic metadata is retained
for the physical retest; it is bounded operational fault evidence, not temporary
per-render logging. Next: corrected SD installation/read-back with fresh backup,
muted boot and controlled taps, then explicitly enable faded audio and confirm
human listening and modulation/local processing before any acceptance claim.
