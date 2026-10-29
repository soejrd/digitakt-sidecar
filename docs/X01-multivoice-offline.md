# X01 — First offline multivoice milestone

2026-10-29. Baseline `5e1d8a2`; dependency revisions remain in
[sources.json](../sources.json). This is an offline implementation/proof,
not an installed two-voice machine or a hardware-qualified candidate.

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

## Next work and remaining gates

1. Add explicit two-track mode/capability negotiation before enabling v4. Define
   stable lane→track mapping per session, independent machine selection and
   invalidation rules. An eight-engine host bank must never imply eight returns.
2. Extend actual device capture/worker and Pi glue to that contract. Preserve
   simultaneous events and ordered STARTs, bound aggregate queue/ACK service,
   and prove reconnect, deselection and lease behavior end-to-end in the
   compiled transport harness. The present firmware path is still singleton.
3. Integrate the tested lane helper with the real pre-filter hook and lifecycle
   gate; select both machines through the native UI in the full emulator.
4. After V01-A's release condition, prepare a reviewed candidate/recovery and
   measure Pi/device margin, USB stability and physical independent routing.

X01 remains in progress and X01a's worst-case acceptance remains open.
The [capacity study](X01a-multivoice-capacity.md) still limits the first physical
design to two conventional full-rate mono returns. Eight separate native tracks
remain transport research, regardless of the successful eight-engine host run.
