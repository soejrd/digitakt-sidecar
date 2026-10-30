# sidecar-firmware v0.3.0 DTSC patches

Base: `047873e630cade798f6b605ca0a46b5f86df5fb3` (`v0.3.0`).
The old `patches/sidecar-firmware/` series remains the historical v0.2
baseline. Do not apply the two series to the same tree.

`python3 scripts/prepare_v030_source.py --output /tmp/dtsc-v030` archives the
new base, applies 0001-0012 in order, and copies the portable protocol/audio
overlay into `pi/engine/dt_control`. The script never edits `deps/`, a disk, a
kernel or hardware. Add `--x01` for the independent two-lane host tree; that
also applies `experimental/0001-x01-stereo-return.patch` and replaces
`dt_link.cpp` with the X01 adapter.

The numbered patches are state-to-state diffs against the preceding patch, not
a replacement for the historical v0.2 evidence. They preserve directional USB
framing, descriptor-selected Digitakt support, DTSC sessions, explicit arm,
TRX-B2 scheduling/resampling, bounded diagnostics, and the v0.3.0 mixer. The
X01 patch is opt-in and uses the new Output only for its two lanes.

Local checks:

```sh
python3 scripts/test_v030_replay.py
python3 scripts/test_x01_host.py --v030
```

A desktop replay is not a Pi kernel build, hardware qualification, recovery
candidate or V01 fix. The v0.3.0 Output restart/playback counters remain a
diagnostic, not evidence that the captured late frame or dense freeze is fixed.

Pi handoff:

```sh
python3 scripts/v030_build_pi.py --output /tmp/dtsc-v030-pi-build
python3 scripts/v030_prepare_candidate.py
```

The build helper needs Docker and the cached `rytm-circle` image; the staging
helper writes only `private/u01/` with a rollback. Neither writes an SD card or
touches hardware.
