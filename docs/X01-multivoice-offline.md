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
- [Actual Circle glue](../host/x01_dt_link.cpp) plus an isolated Output patch.
  The historical v0.2 patch is [here](../patches/sidecar-firmware/experimental/0001-x01-stereo-return.patch);
  the current v0.3.0 port is [here](../patches/sidecar-firmware-v0.3.0/experimental/0001-x01-stereo-return.patch).
  The output stage preserves two separate MAIN L/R lanes instead of summing
  voices. Consumer epochs reject stale FIFO data on asynchronous loss.
  Each new session starts muted and requires `:trx=on`; both lanes retain fade,
  gain and limiting. The old one-track snapshot/tone/trace interfaces are
  unavailable in this experimental glue; remote V01 diagnostics remain isolated.

The default dependency checkout, mod manifest, one-voice worker, and V01 source
are unchanged. The historical experimental host tree is reproduced from the
v0.2 base plus the twelve normal patches and the nested X01 Output patch in a
disposable directory; the current v0.3.0 tree is reproduced with
`python3 scripts/prepare_v030_source.py --output <new-directory> --x01`.
Both copy only portable protocol/engine sources, never the native firmware
adapter. The X01 snapshot codec's fixed stack copy is now freestanding.

### New evidence

- [Compiled native adapter/harness](../evidence/x01/native-transport.json):
  ASan/UBSan pass for 100 simultaneous START pairs, 1,000 per-track control
  updates, ACK loss/replays, lost handshake replies, selection/map changes,
  third-track rejection, disconnect/reconnect, stalled TX and refused replies.
  2,465 frames and 2,569 worker passes in this model. Focused
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
  changes only the selected track. 17,811 render boundaries preserve the other
  six source buffers; both returned engine lanes are observed in their track
  buffers. 681 outgoing frames / 635 delivered replies include intentional loss
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
  --root private/x01-routes --stock private/i02-fixture/Digitakt_OS1.54.syx
PYTHONPATH=private/r03-runtime:deps/digiemu \
  deps/digiemu/.venv/bin/python tools/x01_integrated_probe.py \
  --candidate private/x01-routes
```

The build uses the existing hash-pinned private I02 stock fixture. Output is
`private/x01-routes/artifacts/I02.syx`, firmware label **X01E**, with hashes,
source provenance, map and private boot snapshot. It is emulator-only and is
not a flash candidate. The host preparation command is
`python3 scripts/x01_prepare_host.py --output <new-directory>`; no normal Pi
overlay, SD card or installed kernel is changed by it.

## Native modulation and lock probes

The separate `--lfo-probe` scenario seeds the disposable track 2 sound before
its first native trigger. Native final PTCH values vary; the actual host engine
observes 44 and 75 after the transport's integer quantization, while track 1
stays at 60. [Evidence](../evidence/x01/lfo-fixture.json) records the native and
host values, candidate and source hashes. This is a three-second emulator test,
with USB DMA/host timing modelled; it does not qualify dense modulation on hardware.

The earlier late-seeding experiment left both the native render values and the
host unchanged, despite an exact sound-slot readback. Moving the seed before the
first trigger matches the passing I03 fixture. The mechanism behind late-seeding
is still unproven; this does not establish native live-edit/persistence semantics
or fix V01-A.

```sh
PYTHONPATH=private/r03-runtime:deps/digiemu deps/digiemu/.venv/bin/python \
  tools/x01_integrated_probe.py --candidate private/x01-routes --lfo-probe
```

The `--locks-probe` scenario seeds four native sequencer steps on both tracks,
with different PTCH locks on step 3 (72 and 84). Both the native capture and
host engine START sequences contain baseline → distinct lock → baseline, and
all captured native/host START values match in the observed run. The base sound
slots stay unchanged and the session remains active without a host fault.
[Evidence](../evidence/x01/locks-154.json). This tests disposable RAM pattern
fixtures, not project persistence or panel lock entry.

```sh
PYTHONPATH=private/r03-runtime:deps/digiemu deps/digiemu/.venv/bin/python \
  tools/x01_integrated_probe.py --candidate private/x01-routes --locks-probe
```

The `--copy-probe` scenario changes all eight source SRC controls through the
native panel, then performs held SRC+RECORD on track 2 and held SRC+STOP on
track 1. All eight target controls and host-engine values match the source.
The source, other six sounds, and target's non-SRC slots stay unchanged; the
same negotiated session stays active without a host fault.
[Evidence](../evidence/x01/copy-src-154.json). This covers **SRC-page copy**,
not complete track/pattern copy or project persistence.

Panel actions here use the integrated five-millisecond scheduler throughout.
The older standalone helpers advanced native time without servicing host
replies, causing an artificial session timeout during a longer edit. The copy
probe also waits for native control smoothing to settle before comparing host
values. Neither adjustment changes device or host production code.

The `--sound-lock-probe` scenario seeds two disposable runtime sound-pool
entries and selects them on step 3 of their respective tracks, with **no
parameter locks**. Native playback and the actual host engines observe
60→72→60 on track 1 and 60→84→60 on track 2. Every captured host START matches
its native START; all eight base sounds remain unchanged, and the same session
stays active without a host fault.
[Evidence](../evidence/x01/sound-lock-154.json). This proves the native
sound-pool selection/restore path under a RAM fixture; it does not test UI pool
loading or saved-project persistence and does not pass V01-B.

```sh
PYTHONPATH=private/r03-runtime:deps/digiemu deps/digiemu/.venv/bin/python \
  tools/x01_integrated_probe.py --candidate private/x01-routes --copy-probe
PYTHONPATH=private/r03-runtime:deps/digiemu deps/digiemu/.venv/bin/python \
  tools/x01_integrated_probe.py --candidate private/x01-routes --sound-lock-probe
```

## Native route changes and worker wakeup

The `--route-probe` scenario uses the native machine chooser to move from
tracks1/2 to a single track2, then tracks2/3, an unsupported third external
track, back to tracks1/2, an empty route, and recovery. It verifies fresh
sessions for supported mappings, exact USB lane PCM at every selected source,
zero selected-source PCM behind a closed gate, and preservation of every
unselected source buffer. [Evidence](../evidence/x01/routes-154.json).
The bank's stopped status1 is expected while output is closed; every active
phase requires status0. Physical timing and sequencer playback during these
manual machine changes are not qualified.

This exposed an X01-only recovery bug: retirement of an unsupported/empty
route ends its capture traffic, so returning to a valid map could leave the
worker asleep until another UI/RX wake. The native test first remained gated
closed after removing the third external machine. Adding another panel event
could mask this, so the final probe waits for recovery without that extra
input. The compiled transport regression also disables all modelled UI
fallback wakes and reproduced failed recovery before the fix.
[Before/after record](../evidence/x01/route-wakeup-regression.json).

The opt-in renderer now posts its existing bounded worker semaphore once when
the lane map or route availability changes. It still performs no USB operation,
allocation or wait in render context. The regression covers both third-track
and empty-route recovery without UI polling. The normal one-voice renderer,
V01 worker and Pi host code are unchanged. This is not a V01-A freeze fix.

```sh
PYTHONPATH=private/r03-runtime:deps/digiemu deps/digiemu/.venv/bin/python \
  tools/x01_integrated_probe.py --candidate private/x01-routes --route-probe
bash scripts/test_x01_transport.sh
```

The fresh **X01E** fixture is `private/x01-routes`; only the opt-in renderer
input differs from the earlier `private/x01-control` fixture. The latter's
hash-checked sources are historical; use the fresh fixture with the current
checkout. Both remain emulator-only, never hardware flash candidates.

## Next work and remaining gates

Follow-up regression: `tests/x01_session.c` now also proves a single mapped
track on lane 0, timestamp wrap from `0xfffffffe` to zero, timeout retirement,
and the important stalled-delivery case where valid keepalive replies continue
but an outstanding snapshot never receives its ACK. The outstanding frame's
own lease still closes the device gate. `bash scripts/test_x01_transport.sh`
passes with these cases under ASan/UBSan and the ColdFire import checks.

The isolated Pi 4 AArch64 kernel now cross-builds successfully:
[evidence](../evidence/x01/pi-build.json). The target build caught two adapter
integration errors hidden by desktop include paths: portable headers needed the
`dt_control/` prefix, and Circle requires `<circle/util.h>` instead of the
standard C++ `<cstring>`. The opt-in adapter is corrected and the actual Circle
glue/Output host regression still passes under ASan/UBSan.

```sh
python3 scripts/x01_build_host.py --output /tmp/x01-pi-build
```

This requires the local Docker `rytm-circle` image and existing pinned Pi 4
Circle/library caches. It stages a new source tree, imports the local public
upstream base so upstream scripts resolve their root, copies caches, builds and
records hashes/sizes. It does not change the normal dependency, installed
kernel, SD card or device. Choose a new output path without spaces, as required
by upstream source-list scripts. The kernel image is2,482,984 bytes; ELF text
is2,478,216 bytes, data3,024, BSS1,530,433. The X01 peer's static symbol is68,032
bytes, including the bank and protocol state. These are static target sizes;
runtime heap, stack peaks, CPU margin and USB timing remain unmeasured. Cached
Circle libraries are reused; this is not a clean rebuild of every third-party
library. The image remains an experimental build, not a hardware candidate.

1. Offline: exercise full track/pattern copy; machine/route changes now pass;
   seeded sound locks and native SRC-page copy now pass in addition to
   manual triggers, panel edits, seeded LFO and per-track stored locks.
   Measure runtime/stack footprint and worst-case margin when hardware is available.
2. After V01-A's release condition, prepare a reviewed hardware candidate and
   recovery; measure Pi/device margin, USB stability, timing and independent
   physical routing. Do not flash X01E.

X01 remains in progress and X01a's worst-case acceptance remains open.
The [capacity study](X01a-multivoice-capacity.md) still limits the first physical
design to two conventional full-rate mono returns. Eight separate native tracks
remain transport research, regardless of the successful eight-engine host run.
