# sidecar-firmware patches

Base: `6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457` (pinned in `sources.json`).

## 0001-directional-overbridge-layout.patch

I01: keep the Overbridge input and output layouts independent.

- `teensy/libraries/RytmHost/ob_block.h` adds `Direction`/`in_bytes`/`out_bytes`;
  `find_layout` derives the packet size from each endpoint rather than one
  shared 12-channel constant.
- The block test covers the R02 Digitakt fixture (12 channels / 368 B in,
  2 channels / 88 B out) and the unchanged Rytm 12/12 layout.
- `pi/engine/ob_usb.cpp` scans any whole-block interrupt endpoint, keeps
  separate IN/OUT lengths and buffers, and parses incoming blocks with the
  actual channel count. OUT only writes the channels the device exposes.
- `pi/engine/sidecar_gadget.*` accepts the incoming channel count (default
  12, so Rytm behavior is unchanged).
- The existing Circle factory patch is extended to match both
  `ven1935-b48` and `ven1935-b2c`; the function driver still validates the
  endpoint layout.

Replay from a clean checkout at the pinned revision:

```sh
git -C deps/sidecar-firmware apply --check \
  ../../patches/sidecar-firmware/0001-directional-overbridge-layout.patch
git -C deps/sidecar-firmware apply \
  ../../patches/sidecar-firmware/0001-directional-overbridge-layout.patch
```

Checks: `./scripts/test_i01.sh` (no hardware, Circle, or network). For the
upstream suite, `deps/sidecar-firmware/tools/run_portable_tests.sh` fails only
its localhost web mocks in this sandbox; the C/C++ portions pass.

Not tested here: a Circle kernel cross-build (the Circle tree is not present)
and an actual Digitakt USB stream (H01).

## 0003-dtsc-control-link.patch

I03: core-3 DTSC receiver/ACK worker, single-track control-state installation,
consumer-side PCM gate with FIFO epochs, and descriptor-based selection of the
12-IN/2-OUT integrated device. MIDI loss/removal closes the gate. The host
stores snapshots; TRX-B2 synthesis remains I04, so integrated mode outputs zero.
The normal Rytm path remains enabled unless the integrated device is selected.

Apply after **0001 and 0002** at the same pinned base, then run
`bash scripts/i03_prepare_circle.sh` to overlay the device-neutral protocol
sources from this repository. No firmware-derived adapter code is copied into
the dependency. `bash scripts/test_i03_integration.sh` checks the real Output
consumer gate and the portable device/host exchange. The patched Pi 4 kernel
cross-builds with the existing `rytm-circle` toolchain image; no SD was written.

## 0006-i04-trx-voice.patch

I04 initial host prototype: gated real TRX-B2, native-rate converter, loss reset
and third-party NOTICE. This is followed by **0007**; apply all seven patches
then `bash scripts/i04_prepare_circle.sh` (supersedes the I03-only overlay).
Run `bash scripts/test_i04.sh` and `python3 scripts/test_i04_replay.py`.
The overlay verifies mdkit's pin and refuses changes to copied tracked sources.
See [I04](../../docs/I04-external-trx-voice.md) for timing, filter-quality and
hardware acceptance limits. No firmware assets or SD image are deliverables.

## 0007-i04-scheduled-playout.patch

I04 follow-up: timestamped bounded playout, 128-tap native-rate conversion,
explicit session-bound `:trx=on/off` with fade/default silence, `:trxstatus`,
and inclusion of synthesis in the Pi maximum-block timer. Apply after **0001–0006**
then run `bash scripts/i04_prepare_circle.sh`. This supersedes 0006's immediate
trigger/R05-converter behavior. `python3 scripts/test_i04_replay.py` checks all
seven patches; the I04 suite checks scheduled deadlines and separately consumed
native STARTs. The 1,024-frame buffer is experimental; adjacent-block STARTs
fail closed. Hardware timing remains unqualified; see the current I04 report.

## 0008-i04-playout-fault-reason.patch

Diagnostic follow-up to physical silence: 37 playout faults disarmed the initial
candidate. Retain exact last rejection metadata across session resets and report
it with `:trxstatus`, without logging in render/install. Apply after 0001–0007
and use the current I04 overlay. Host tests assert reason/reset behavior; eight
patches replay and Pi kernel build pass. This is instrumentation, not a root-cause
fix or an audio acceptance result. Diagnostic-1 remains to be deployed.

## 0009-i04-bounded-timing-trace.patch

Physical 42.667 ms candidate still fails PTCH timing. Add a bounded 128-record
ring frozen on first rejection, requested-frame counter in Output, muted-only
capture and explicit command-context dump. Retain metadata across reset. Raw
payloads remain private; analyzer emits timing-only fields. Core/native/ASan
trace bounds/freeze/reset tests, nine-patch replay and Pi build pass. This is
diagnostic instrumentation, not a qualified timing fix; buffer stays unchanged.
