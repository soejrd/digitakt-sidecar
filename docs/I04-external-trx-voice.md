# I04 — external TRX-B2 voice

Status: **three physical hits audible; SRC traffic overruns measured; effective-
control firmware candidate I04Q checked, awaiting human-approved installation**
(2026-10-28). Owner: Codex. I04 is not DONE.

## What is installed and what is staged

- **Digitakt:** physical H02F OS 1.54 candidate, USB1 on audio track 1. Its sender
  still publishes every changed fractional 8.8 control snapshot.
- **Pi:** timing-trace kernel, native-rate real TRX-B2, generation fix and
  experimental 2,048-frame (42.667 ms) playout / 256 events. Audio is muted;
  trace is frozen at the first captured SRC failure. No new SD update is needed
  for I04Q; the 61-byte protocol is unchanged.
- **Staged only:** `private/i04/firmware-I04Q/` contains verified I04Q firmware,
  previous H02F and stock OS 1.54 recovery plus a source/hash manifest. Only the
  main OS section differs from stock; bootstrap/updater/coproc sections stay
  byte-identical. No hardware firmware installation has occurred this session.

## Measured diagnosis and targeted change

The [physical trace](../evidence/i04/timing-trace.json) retained 128 consecutive
SRC updates: **4,064 source frames (84.667 ms)** took **5,656 USB-requested frames
(117.833 ms)** to arrive. Generated frames advanced 5,652, only four fewer than
requested frames across this interval. Large delivery gaps occur at retained
indexes **1, 64 and 127**, matching the sender's **63-record worker budget**.
The final record was 128 frames late; subsequent status showed three faults.
The human also reported sluggish PTCH control. The trace supports a sustained
control-delivery deficit, not simply one buffer-size miss. It does not isolate
all native sender CPU, task-scheduling or USB costs. Raw control snapshots/logs
remain private; public evidence contains timing and change flags only.

**119 of those 128 records changed only fractional bits that the existing TRX
wrapper discards.** Nine effective state records (including the first retained state) suffice; no START
occurred in this retained window. I04Q (`usb-track` 0.4) applies the same signed
8.8 truncation/clamp to 0–127 **before comparison and queue publication**. Every
effective integer control change, START (including adjacent identical ones),
note/velocity change and periodic liveness snapshot remains ordered. This is
normalization for the current integer TRX-B2 API, not arbitrary update-rate
limiting or a claim to transport full fractional synthesis precision.

The production capture regression was RED before: 128 records instead of nine.
It is GREEN after: nine records; adjacent START and note/velocity preservation
and clamp endpoints pass. Real mdkit PCM is bit-identical for all fractional
updates versus effective-only updates at the same intended intervals, with
both STARTs consumed. Replaying the retained physical timing through the real
scheduler reproduces the original 128-frame late failure; retaining the nine
effective events admits them without rebasing deadlines. This proves the
retained-window correction and engine equivalence, **not future physical timing**.
Fast/deep modulation that changes integer values every block may still exceed
capacity and remains a stress/architecture question.

[Candidate and regression evidence](../evidence/i04/effective-control-candidate.json).

## Current voice/transport contract

One persistent real mdkit TRX-B2 at
`96157fe4fbda17bf4cae3168f52437d8232e6570` uses its public C API; synthesis
arithmetic is unchanged. SRC A–H map to PTCH, DEC, RAMP, HOLD, TICK, NOIS, DIRT,
DIST. Wire `voice` is a nonzero **hit generation**, not track ID. Track 1 is
implicit. PTCH owns pitch; note does not transpose this percussion prototype.
Host velocity gain is unity; physical local AMP velocity behavior is unverified.
Parameter-only updates keep the tail. Fixed 0.125 gain and ±4095 int16 ceiling
limit mono return channel 0 to about −18 dBFS before local processing; channels
1–7 are zero. Converter overshoot can clip at this ceiling.

TRX stays at 44.1 kHz. The causal 128-tap, 160-phase Blackman-windowed sinc
(20 kHz cutoff) supplies exact 160/147 conversion to 48 kHz without native
lookahead: 48,000 output frames consume 44,100 native frames. History persists
through STARTs and resets on invalidation. Coefficients initialize before
activation. Render never allocates, waits or calls USB. R05 remains historical.

The bounded scheduler owns immutable complete snapshots before ACTIVATE. ACK
means the queue owns the validated state; synthesis fires at its due output
sample. First START anchors sender block time to current render time plus
2,048 frames; later deadlines preserve sender deltas at 32 output frames/block.
Late/order/session/capacity/horizon failures close the gate; no silent coalescing,
late replay or rebasing. Native mdkit boundaries require at least 35 output
frames between STARTs; sender granularity makes the supported minimum two
blocks / 64 frames (1.333 ms). Pending/too-close STARTs fail visibly.

**42.667 ms is experimental added buffering, not accepted musical latency.**
It neither removes initial transport, engine quantization, filter/USB/FIFO delay
and drift nor aligns current Digitakt local locks with delayed audio. The human
explicitly noted that this is long and agreed only to testing. Worker/protocol
improvements or lookahead/compensation may still be needed for acceptable timing.

## Safety, trace and dependency replay

TRX defaults off. `:trx=on` requires an active session and fades over 200 ms;
loss/invalidation/reconnect disarms. `:trx=off` silences newly generated PCM.
`:trxstatus` reports queue/fault/START/consumed counters; last-fault metadata
survives reset. Per-session counts do not count all taps across invalidations.

`:trxtrace=on` forces mute and starts a fixed 128-record ring; enabling voice is
rejected while capture is enabled. Each install records source frame, generated
and transport-requested frame clocks, previous sender/deadline, FIFO/queue and
admission result. It freezes on first rejection and survives reconnect.
`:trxtrace` dumps oldest-first in command context; `:trxtrace=off` stops capture
and leaves mute. Output requested-frame counter includes silence/underflows;
it is not physical returned-audio or USB-completion timing. Clock fields wrap
at 32 bits. Dumping can stall output; exclude dump time from runtime conclusions.
Raw payloads stay private; `tools/i04_analyze_trace.py` exports timing/change
metadata only. Bounds/freeze/reset/mute interlock and analyzer tests pass.

Pi changes replay as patches 0001–0009 at sidecar-firmware
`6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457`, plus the tracked I04 overlay. mdkit
is pinned and unchanged; NOTICE retains licences. No firmware-derived code/data
enters the host overlay. Digitakt changes are tracked under `mods/usb-track/`;
elekloader remains pinned/unchanged. I04Q adds no hooks or firmware sites, only
normalization inside the existing render event. Kernels, firmware, full captures
and original files remain ignored/private. No publishing/push occurred.

## Evidence and executed checks

| Scope | Result |
| --- | --- |
| Initial silence | `voice == 1` rejected later hit generations; 37 faults. Fixed in both host layers; real worker regression retains generations 1–4. [Evidence](../evidence/i04/generation-regression.json) |
| Physical hits after generation fix | One muted tap consumed, then human heard three hits; zero faults, FIFO stayed at 15 startup underruns. [Evidence](../evidence/i04/physical-audible.json) |
| Initial PTCH failure | Code 3; parameter event 608 frames late; explicit mute. [Evidence](../evidence/i04/ptch-failure.json) |
| Larger-buffer physical failure | One muted tap + PTCH sweep: five code-3 faults, last event 20 frames late, FIFO 1→365. [Evidence](../evidence/i04/batch-fix-failure.json) |
| Trace baseline/result | Expected build, zero initial faults, 14 startup underruns; first failure retained privately and sanitized. [Baseline](../evidence/i04/timing-trace-baseline.json), [timing](../evidence/i04/timing-trace.json) |
| I04Q | Build/lint/patch, protected-section verification, emulator boot/assignment pass. All eight native encoder edits pass, including navigation off the read-only diagnostic page. [UI](../evidence/i04/effective-ui.json) |
| I04Q compiled exchange | Native serializer/RX bridge/render/worker, ACK/loss/reconnect, dense modulation with modelled completions pass. [Exchange](../evidence/i04/effective-exchange.json) |
| Host production-source tests | ASan/UBSan engine/queue/native/Circle, integer/fractional PCM equality, nine-record capture, adjacent START preservation and captured-timing replay pass. |
| Firmware render object | ColdFire compilation; no allocator/wait imports. No complete hardware render/headroom claim. |
| Filter | Sampled 18 kHz +0.000496 dB; 22 kHz −81.08 dB; impulse 64→139, designed delay 63.5 native samples (~1.440 ms). Finite-grid, host-only. [Evidence](../evidence/i04/filter.json) |

Reproduce from this repository:

```sh
bash scripts/test_i04.sh
bash scripts/test_i03_queue.sh
python3 scripts/test_i04_replay.py
python3 scripts/i04_prepare_firmware.py
# Private candidate generation (fresh directory; Unicorn JIT needs executor access):
PYTHONPATH="$PWD/private/r03-runtime:$PWD/deps/digiemu" \\
  ../digiemu/.venv/bin/python tools/i02_probe.py candidate \\
  --root private/i04-effective --stock private/i02-fixture/Digitakt_OS1.54.syx \\
  --fixture private/i02-fixture --diagnostic --label I04Q
```

The candidate directory already exists; use a fresh directory for rebuilding.
The preparation script checks candidate source pins, container checksums/layout,
main in-place expansion and byte-identical stock sections 2/4/5/8. Candidate
I04Q SHA-256 is `75f5b66ae5db95676eb767ece3239ec81a9281a42966fbca7689b22a8b14bb8b`.
Verified previous H02F and original stock recovery are staged alongside it.
No kernel SD swap is needed. Existing SD rollback evidence remains in Git and
`evidence/i04/*-sd-deployment.json`; settings/prior backups were preserved.

## Human installation gate and remaining acceptance

Before a firmware trial, obtain the human's confirmation that projects/sounds
are saved/backed up, the transfer connection/recovery path is available, and the
concrete I04Q candidate is approved. Follow the existing
[human recovery procedure](I02-one-track-routing.md#human-hardware-trial-and-recovery-gate):
stock recovery uses FUNC at power-on, TRIG 4 / OS UPGRADE and Transfer legacy
upgrade. Keep both previous H02F and stock 1.54 files available. No bootloader or
internal wiring change. Computer UI access remains prohibited; human handles
installation. The Pi is currently muted and trace-interlocked.

After I04Q installation, reconnect Digitakt to Pi. Start a fresh muted trace,
verify effective-update traffic and no fault during the same PTCH gesture.
Then stop trace, explicitly enable faded audio with speakers low, and verify
SRC A–H, locks/LFO/retriggers, local filter/AMP/sends, neighboring track and
note/velocity policy. Measure integrated latency/jitter/local-lock alignment,
Pi maximum render/queue margin, stability and reconnect. The old failed USB
MAIN pulse matching is not a valid latency reference; identify a usable capture
channel or analog loopback. Reduce/revise timing only against measured evidence.

No physical I04Q result exists yet. Fast/deep integer modulation remains a
capacity limit to test. I04 is incomplete, V01 depends on it, and there is no
independent READY task.

### Human approval recorded

Human confirmed the backup/installation gate for concrete I04Q; see
[approval](../evidence/i04/firmware-trial-approval.json). Candidate sources,
container/stock sections and both previous/stock recovery files reverified.
Pi explicit mute accepted. Firmware installation is still pending the human:
connect Digitakt USB to computer, use the normal Transfer OS-update workflow
with the staged I04Q file, wait for completion/reboot, verify I04Q then reconnect
to Pi. Leave trigs/SRC untouched for a fresh muted baseline/trace. No agent
flash or Pi SD change is claimed.
