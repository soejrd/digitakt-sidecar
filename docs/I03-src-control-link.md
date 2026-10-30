# I03 — SRC controls and control link

Status: **DONE at host/compiled-emulator level** (2026-10-27).

Target: Digitakt mk1 OS 1.54, Pi 4B/Circle. The compiled native control link,
portable protocol, production Circle glue, PCM gates and kernel build pass.
Dynamic ordering evidence is recorded in [dynamic-154.json](../evidence/i03/dynamic-154.json).
No firmware was flashed, SD deployed, or hardware audio played.

## Implementation

USB1 0.3 exposes PTCH, DEC, RAMP, HOLD, TICK, NOIS, DIRT, DIST on one audio
track, with ranges 0–127 and defaults `60,65,41,62,91,44,56,58`. Numeric
widgets avoid the sample/loop displays; knob D does not open the sample
browser. The extended machine-pages ABI applies native sound defaults.
The compiled panel probe exercises all eight encoders; no MIDI sequencer
track is allocated. [UI evidence](../evidence/i03/transport-154.json).

`mods/usb-track/usb_track.c` reads final A-H, note, velocity and native START
at the post-voice render hook. Its fixed 256-record queue never waits,
allocates, or calls USB. It preserves every changed snapshot and every START,
including adjacent identical starts. Identical non-START blocks are suppressed,
with a snapshot at least every 64 blocks. Published records are immutable;
a producer-owned comparison copy makes this safe across consumer preemption.
Original block timestamps and contiguous transmitted sequences are retained.
A full queue or ordering fault closes the gate; no changed value is coalesced.

This removes the previous idle backlog without enlarging the queue. The
production-code integration test also runs 12,000 blocks at a deliberately
slow 300-block worker interval without session churn. Dense modulation is
separately sustained at the normal 50-block cadence and through the compiled
native sender. A slower consumer under continuous changes still fails closed;
queue capacity does not constitute a throughput guarantee.

`mods/usb-track/worker.c` runs only from `ev_tick`. It opens a fresh session,
waits for HELLO_ACK, sends an initial recovery START, then streams ordered
snapshots through the native USB-MIDI adapter. The Pi installs values before
queuing ACTIVATE; the Digitakt accepts only the matching session, sequence and
block before opening its PCM gate. Both stale USB halves are cleared before
rearming. Machine change, loss, overflow and lease expiry close the gate.
See [handshake](I03-handshake.md) and [native transport](I03-transport.md).

The unchanged 61-byte v1 snapshot and separate 24-byte v2 control frames use
explicit canonical 7-bit encoding. Track identity is implicitly audio track 1;
additional tracks require a protocol revision. A reconnect uses a fresh nonzero
session. Physical detection latency remains unmeasured.

The replayable Circle integration is
`patches/sidecar-firmware/0003-dtsc-control-link.patch` in the historical v0.2.0
series, after patches 0001 and 0002 at that base. The current v0.3.0 port is in
[UPGRADE-v0.3.0.md](UPGRADE-v0.3.0.md); do not mix the two patch sets.
`scripts/i03_prepare_circle.sh` overlays the portable protocol sources. The real
`dt_link.cpp`, MIDI reply queue and Output gate are executed by a host harness.
Integrated mode installs control state and outputs silence until I04 supplies
synthesis; control-link completion is not an external-voice/audio claim.

## Acceptance evidence

- [Compiled exchange](../evidence/i03/integrated-154.json): OS 1.54 candidate
  boots; the real native serializer, compiled two-argument RX bridge, worker
  and render gate exchange messages with the portable Circle peer. SRC
  changes and repeated starts arrive; pre-activation PCM is zero; matching
  ACTIVATE opens the gate after a stale-half flush. ACK loss, link loss,
  stalled completions and recovery pass. Every-block modulation sustains
  delivery with modelled completion progress. USB DMA and incoming PCM are
  explicitly modelled, not physically measured.
- [Queue evidence](../evidence/i03/src-queue.json): host ASan/UBSan tests,
  unchanged-block suppression, adjacent starts, exact overflow boundary,
  machine transition and ColdFire object checks pass.
- [Integration/build evidence](../evidence/i03/integration.json): actual
  production render/worker/native adapter and actual Circle glue/Output
  harnesses pass; the Pi 4 kernel cross-build succeeds. Dependency changes
  remain replayable patches; no private firmware artifacts are committed.
- [Dynamic ordering](../evidence/i03/dynamic-154.json): native sequencer and
  LFO scenarios record final render values and start order. Read the scenario
  scope; fixtures and emulator observations do not establish hardware timing
  or a separate retrigger event.

The compiled probe must give injected callbacks a fresh instruction budget:
Session's persistent native block-budget hook otherwise stops them before
execution. It asserts return to the sentinel. An ACK-loss check must exceed
the 750-block lease (500 ms at 48 kHz/32), so it waits 650 emulated ms.

## Reproduce

From this repository, with the private OS fixture, pinned patched Unicorn and
compatible Python 3.12 (`PY=../digiemu/.venv/bin/python`):

```sh
bash scripts/test_i03.sh
bash scripts/test_i03_queue.sh
bash scripts/test_i03_transport.sh
bash scripts/test_i03_handshake.sh
bash scripts/test_i03_dynamic.sh
bash scripts/test_i03_integration.sh
export PYTHONPATH="$PWD/private/r03-runtime:$PWD/deps/digiemu"
"$PY" tools/i02_probe.py candidate --root private/i03-release3 \
  --stock private/i02-fixture/Digitakt_OS1.54.syx --fixture private/i02-fixture
"$PY" tools/i03_probe.py --sections private/i02-fixture/sections \
  --candidate private/i03-release3 --output evidence/i03/transport-154.json
"$PY" tools/i03_dynamic_probe.py --candidate private/i03-release3 \
  --output evidence/i03/dynamic-154.json --timeout 600
"$PY" tools/i03_integrated_probe.py --candidate private/i03-release3 \
  --output evidence/i03/integrated-154.json
bash scripts/i03_prepare_circle.sh
bash deps/sidecar-firmware/pi/engine/build.sh
```

Candidate creation needs a fresh directory and execution outside the restricted
sandbox for Unicorn JIT memory. Metadata pins all firmware-side sources and
refuses stale candidates. The private candidate is labelled I03P and is not a
hardware-qualified replacement for I02A. H02 retains physical routing, latency,
headroom, ten-minute stability and cable reconnect qualification; I04 retains
TRX-B2 integration and synthesis policy.
