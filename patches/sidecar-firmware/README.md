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
