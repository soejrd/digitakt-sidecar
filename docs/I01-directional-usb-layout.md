# I01 — separate USB IN/OUT host layouts

Follow-up: [H01](H01-stock-usb-stream.md) builds and boots the full Circle kernel and measures the physical stock transport. The original host-test scope below is preserved; its static MIDI-capture compile error is fixed by patch 0002.

Status: **implementation complete for the host-side layout policy; Circle
kernel build and live stream remain for H01.** Owner: `codex-i01 @
2026-10-26T18:46:33Z`.

## Evidence

The physical R02 capture shows a directional Digitakt mk1 alt-3 layout:

| Direction | Endpoint | Channels | Packet |
|---|---|---:|---:|
| Digitakt → host | `0x83` | 12 | 368 B |
| Host → Digitakt | `0x03` | 2 | 88 B |

The Rytm MKII is 12/12 and 368/368. These are descriptor capacities, not a
measured audio stream; sample format and rate remain unmeasured.

## Implementation

The host codec now records a `Direction` per endpoint:

- `channels_for(mps)` derives channels from `32 + 7 * channels * 4`.
- `Layout` carries `in_bytes` and `out_bytes`, not one shared size.
- `find_layout` rejects non-block endpoints independently in each direction.

The Pi engine now uses those values:

- endpoint scan accepts any valid whole-block endpoint, then reads its size;
- IN and OUT get separate URB lengths and DMA buffers;
- OUT serialization writes only the advertised channel count (two on Digitakt,
  twelve on Rytm);
- incoming blocks are parsed with `m_nInChannels`, with missing channels
  zero-filled where the engine’s fixed 12-channel consumers expect them.

The Circle factory patch now routes both `ven1935-b48` and `ven1935-b2c` to
the Overbridge function driver. The driver still validates alternate 3 and
the endpoints; PID matching alone does not enable streaming.

## Tests and acceptance

`./scripts/test_i01.sh` passes:

- upstream `ob_block_test.cpp`, including the R02 fixture;
- `tests/test_usb_layout_policy.cpp` reads the actual R02 raw descriptor and
  asserts Digitakt 12/368 IN and 2/88 OUT while preserving Rytm 12/12;
- `tests/test_digitakt_layout.cpp` independently reads the decoded R02
  endpoints and rejects a collapsed single-size layout;
- the clean patched checkout replays the saved patch and passes the block
  test.

The upstream portable suite reaches its C/C++ tests successfully but its
localhost mock-web tests are denied in this sandbox. That is an environment
limitation, not an I01 failure.

## Limitations

- No Circle tree was available, so a full Pi kernel cross-build was not run.
  The Circle change is confined to the factory name match; the function
  driver’s descriptor-driven sizing is covered by host tests.
- No stream was started. H01 must still demonstrate 368-byte IN and 88-byte
  OUT packets, correct channel placement, sample format, rate, latency, and
  a zero-gap run.
- The Rytm path remains the default and its layout is regression-tested.
  Do not claim that the Digitakt audio channels are semantically the Rytm’s.

## Reproduction

> **Patch base note:** this reproduction uses the historical v0.2.0 series.
> The current v0.3.0 port and clean replay command are in
> [UPGRADE-v0.3.0.md](UPGRADE-v0.3.0.md).

```sh
./scripts/test_i01.sh
git -C deps/sidecar-firmware apply --check \
  ../../patches/sidecar-firmware/0001-directional-overbridge-layout.patch
```

The dependency patch is tracked in
`patches/sidecar-firmware/0001-directional-overbridge-layout.patch`.
