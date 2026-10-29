# X01 — Offline multivoice implementation

2026-10-29. Baseline `5e1d8a2`; dependency revisions remain in
[sources.json](../sources.json). This is an offline implementation/proof,
not an installed two-voice machine or a hardware-qualified candidate.
The second milestone below adds the actual opt-in worker and Pi glue and a
booted native-serializer exchange. The default one-voice build stays unchanged.

## Delivered

- [Track snapshot codec](../control/track_frame.h): experimental SysEx v4,
  62 bytes, retaining the exact signed/fractional v1 snapshot plus a separate
  zero-based track ID. Hit generation remains per track; sequence is global
  to the session. Existing v1/v3 receivers reject it, rather than interpreting
  a second track as a new hit on track 0. No production worker negotiates v4 yet.
- [Track playback bank](../audio/track_playout.h): eight independent real
  TRX-B2/converter states, common timestamp anchor, equal timestamps across
  tracks, strictly advancing timestamps within each track, bounded 1,024-event
  queue, per-track generations and trigger-spacing checks. All due tracks apply
  before synthesis of that output sample. A fault/stop silences the bank until
  reinitialization. Caller still owns handshake, session freshness and leases.
- [Two-lane routing helper](../mods/usb-track/return_lanes.h): maps left/right
  to distinct chosen mono track buffers, preserves both before clearing the
  consumed stereo half, leaves the next half and other tracks untouched.
  Disarmed/invalid-selector calls silence mapped tracks and flush both halves;
  invalid maps flush USB without modifying track buffers. The helper is not
  installed in the production render hook or mod manifest.
- The existing [TRX wrapper](../audio/trx_voice.c) now exposes its limited,
  velocity-scaled scalar sample renderer. The original eight-column one-voice
  API still clears unused columns and writes column 0. Both banks use the same
  44.1 kHz engine and continuous 160/147 conversion, with no allocation/USB/waits
  in their render paths. Eight host columns do not add USB return channels.

## Evidence and practical meaning

**Functional host tests pass** under ASan/UBSan: v4 exact round-trip and malformed
input rejection; independent audio compared sample-for-sample with isolated
engines; staggered arrivals sharing the same anchor; simultaneous hits; block
wrap; ragged callbacks; per-hit zero-velocity isolation; generation/session/
sequence/late/horizon/queue/spacing faults; stop/reset and lane routing.
[Tests](../tests/x01_tracks.c), [runner](../scripts/test_x01.sh).

**1/2/8 real engines run successfully on the desktop host**, each with an
independent converter, over two ten-second synthetic profiles. All voices
produce exactly 441,000 native samples. The eight-engine dense run applies
120,000 snapshots and consumes 7,500 STARTs per voice (60,000 total), with no
playback faults. Queue peak is 40 of 1,024.
[Recorded benchmark](../evidence/x01/host-benchmark.json),
[benchmark source](../tools/x01_bench.c).

The eight-engine dense run processes ten seconds in 0.509 seconds (~19.66×
realtime) on the recorded Darwin/arm64 host. Median/p99 render duration is
9/45 µs, **maximum observed 7,670 µs**, including host scheduling effects.
Timer resolution is 1 µs. Keep that maximum: this is not a 250 µs deadline pass.
The playback clock advances by rendered samples, so host descheduling does not
itself create a simulated late event. Aggregate speed supports further Pi
benchmarking, not a Pi capacity or physical worst-case guarantee. Codec/enqueue
batch timing is recorded separately; USB/ACK/device worker costs are absent.
The bank uses 65,632 bytes on this 64-bit host, including all eight engines and
the shared queue; target ABI footprint is not measured.

**Actual compiled ColdFire routing passes nine function-level cases** in
Unicorn: both halves, arbitrary track assignment, discarded lane, disarm,
invalid selector and invalid maps. Exact PCM, adjacent memory guards and
unrelated tracks survive; the compiled helper has no undefined symbols.
Instruction counts are not cycle/timing measurements.
[Evidence](../evidence/x01/coldfire-routing.json),
[probe](../tools/x01_route_probe.py).

**Full stock OS 1.54 render passes with the compiled helper inserted in RAM**
at the previously verified pre-filter site. A register/SR-preserving test
gateway replays the displaced instruction. Synthetic 750 Hz/3 kHz return lanes
go into tracks 0/1. Four cases cover 8,802 exact routed blocks, both halves,
preserved source buffers on the other six tracks, and zero dry USB contribution
at the MAIN consumer. Native AMP volume zero on either track suppresses its own
tone by >99.9% while retaining the other tone within ~2% of baseline. Closing
track 0's native filter suppresses its tone by >99.9% while the other persists.
[Evidence](../evidence/x01/native-routing-154.json),
[probe](../tools/x01_native_probe.py),
[emulator-only gateway](../tools/x01_route_gateway.s).

That full-render result uses HLE and synthetic already-received PCM, bypasses
machine selection/session gating, and installs no firmware file. It proves
compiled insertion plus native downstream isolation, not USB DMA, physical
timing, firmware control transport or complete multitrack machine behavior.

**Existing one-voice regressions pass:** real-engine PCM/rate/bounds, scheduler,
final controls, velocity, firmware-worker generations at two service cadences,
render queue and cross-compiled render/queue import checks. No dependency source
or firmware package changed. No hardware was accessed.

## Reproduce

From the repository root:

```sh
bash scripts/test_x01.sh
bash scripts/test_i04_generation.sh
bash scripts/test_i03_queue.sh
PYTHONPATH=private/r03-runtime:deps/digiemu \
  deps/digiemu/.venv/bin/python tools/x01_route_probe.py
PYTHONPATH=private/r03-runtime:deps/digiemu \
  deps/digiemu/.venv/bin/python tools/x01_native_probe.py \
  --fixture private/i02-fixture
```

The last two commands need the existing patched Unicorn runtime and executable
memory access; the full native probe also needs the user's hash-pinned private
[I02 stock fixture](I02-one-track-routing.md#reproduce). It verifies the stock
MAIN hash and displaced instruction before injection. Fixture/firmware, compiled
blobs and audio remain ignored. The test gateway address is deliberately
emulator-only and makes no physical memory-placement claim.

## Second milestone — negotiated control and real native worker

Implementation baseline `873fd89`, 2026-10-29. The experimental path now includes:

- [v5 management/device session](../control/track_session.h), explicitly binding
  two 48 kHz mono lanes and their track IDs to v4 snapshots. A matching HELLO_ACK
  is required before snapshots; both mapped tracks' first STARTs must be ACKed
  before the device gate opens. There is no implicit v1/v3 fallback.
- [Pi peer](../control/track_host.h), with a shared playback bank, exact replay
  comparison, 33 retained tuples for a 32-frame outstanding window, bounded
  recent-session rejection and lease expiry. ACTIVATE is cumulative but must
  name an actually outstanding sequence/block. Duplicate snapshots do not
  reinstall STARTs. Overtaken keepalive echoes cannot invalidate or renew a
  newer device state; a lost HELLO_ACK has a bounded-rate exact retry.
- [Opt-in native capture](../mods/usb-track/x01_render.c) and
  [worker](../mods/usb-track/x01_worker.c), using final SRC values, the existing
  two-argument native MIDI adapter and native RTOS worker. Two per-lane producer
  queues merge by device timestamp, preserving equal-time starts. A map change
  retires the session; a third selected external source is silent/unsupported.
  Render and RX callbacks do no protocol/USB servicing. RX only appends bytes
  and posts a wake; ACK interpretation is task-level. Native send deferral is
  bounded by a worker TX-progress timeout, independently of received liveness.
- [Actual Circle glue](../host/x01_dt_link.cpp) plus an isolated
  [Output patch](../patches/sidecar-firmware/experimental/0001-x01-stereo-return.patch).
  The output stage preserves two separate MAIN L/R lanes instead of summing
  voices. Consumer epochs reject stale FIFO data on asynchronous loss.
  Each new session starts muted and requires `:trx=on`; both lanes retain fade,
  gain and limiting. The old one-track snapshot/tone/trace interfaces are
  unavailable in this experimental glue; remote V01 diagnostics remain isolated.

The default dependency checkout, mod manifest, one-voice worker, and V01 source
are unchanged. The experimental host tree is reproduced from the pinned base
plus the twelve normal patches and the nested X01 Output patch in a disposable
directory. It copies only portable protocol/engine sources, never the native
firmware adapter. The X01 snapshot codec's fixed stack copy is now freestanding.

### New evidence

- [Compiled native adapter/harness](../evidence/x01/native-transport.json):
  ASan/UBSan pass for 100 simultaneous START pairs, 1,000 per-track control
  updates, ACK loss/replays, lost handshake replies, selection/map changes,
  third-track rejection, disconnect/reconnect, stalled TX and refused replies.
  2,436 frames and 2,540 worker passes in this model. Focused
  [session tests](../tests/x01_session.c) also cover wrong maps, unsupported
  versions, conflicting replays, retired IDs, full credit windows and overtaken
  keepalive echoes. ColdFire objects have no allocator/wait/libc imports.
- [Pi glue/Output](../evidence/x01/host-integration.json): actual opt-in C++ glue
  and patched Output compile/run on the desktop with ASan/UBSan after clean
  patch replay. Tests cover negotiation, explicit arming, real dual-engine PCM,
  independent output polarity/ratio, stale FIFO epochs, retired sessions and
  failed ACK publication. This is not an ARM kernel cross-build.
- [Full OS 1.54 exchange](../evidence/x01/integrated-154.json): emulator-only
  X01E boots and both USB1 machines are selected through the native UI. Both
  engines consume simultaneous native manual triggers. A PTCH panel edit
  changes only the selected track. 17,815 render boundaries preserve the other
  six source buffers; both returned engine lanes are observed in their track
  buffers. 667 outgoing frames / 621 delivered replies include intentional loss
  tests. ACK loss, unplug/reconnect and stalled native submission all close the
  gate and recover. The native worker runs at IPL0, its semaphore wait is not
  faked, sender calls are masked, and stack-guard fault count stays zero.

The full exchange models USB submission/completion and received PCM and uses
the portable real-engine peer; the actual C++ Output/epoch path is verified in
the separate host harness. It does not measure physical USB/DMA, Pi scheduling,
latency, drift, UI responsiveness or worst-case margin. Passing these tests does
not resolve V01-A's physical dense-modulation freeze.

Reproduce without hardware:

```sh
bash scripts/test_x01_transport.sh
python3 scripts/test_x01_host.py
# New private destination required; this builds/boots but never flashes.
PYTHONPATH=private/r03-runtime:deps/digiemu \
  deps/digiemu/.venv/bin/python tools/x01_build_candidate.py \
  --root private/x01-control --stock private/i02-fixture/Digitakt_OS1.54.syx
PYTHONPATH=private/r03-runtime:deps/digiemu \
  deps/digiemu/.venv/bin/python tools/x01_integrated_probe.py \
  --candidate private/x01-control
```

The build uses the existing hash-pinned private I02 stock fixture. Output is
`private/x01-control/artifacts/I02.syx`, firmware label **X01E**, with hashes,
source provenance, map and private boot snapshot. It is emulator-only and is
not a flash candidate. The host preparation command is
`python3 scripts/x01_prepare_host.py --output <new-directory>`; no normal Pi
overlay, SD card or installed kernel is changed by it.

## Next work and remaining gates

Follow-up regression: `tests/x01_session.c` now also proves a single mapped
track on lane 0, timestamp wrap from `0xfffffffe` to zero, timeout retirement,
and the important stalled-delivery case where valid keepalive replies continue
but an outstanding snapshot never receives its ACK. The outstanding frame's
own lease still closes the device gate. `bash scripts/test_x01_transport.sh`
passes with these cases under ASan/UBSan and the ColdFire import checks.

Pi cross-build preflight is currently blocked: the local Docker socket is
absent and `docker image inspect rytm-circle` cannot reach a running daemon.
Release condition: start the local Docker daemon, then build the isolated X01
host tree; no ARM kernel build result is claimed. Native lock/LFO coverage can
proceed independently.

1. Offline: add two-track native p-lock/LFO/sound-lock/copy semantics coverage;
   the current new native controls test covers manual triggers and a panel edit.
   Cross-build the isolated Pi kernel and inspect target memory/stack footprint.
2. After V01-A's release condition, prepare a reviewed hardware candidate and
   recovery; measure Pi/device margin, USB stability, timing and independent
   physical routing. Do not flash X01E.

X01 remains in progress and X01a's worst-case acceptance remains open.
The [capacity study](X01a-multivoice-capacity.md) still limits the first physical
design to two conventional full-rate mono returns. Eight separate native tracks
remain transport research, regardless of the successful eight-engine host run.
