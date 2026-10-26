# R07 — OS 1.54 USB receive path

**Takeover result:** a native ColdFire USB path is identified and its receive/mix
functions pass focused OS 1.54 execution tests. The previous DSPI-only/no-DMA
verdict is withdrawn. R07 is complete at the static/function-emulator level; physical transport and
full-device integration remain H01/H02 work; no firmware was flashed and no audio was sent to hardware.

## What changed

The inherited report treated section 8 as the sole USB owner and inferred a
serial mailbox from absent ColdFire address literals. Neither inference was
justified: addresses can be computed, DMA mappings can differ, and code presence
does not establish which hardware variant executes it. Section 8 also configures
SAI1/DMA at `0x6004DC14..0x6004DCD6`, consistent with the
[NXP RT1011 register map](https://github.com/nxp-mcuxpresso/mcux-sdk/blob/main/devices/MIMXRT1011/MIMXRT1011.h).
Its role on this physical Digitakt is still unestablished; its mere presence
does not supersede the native driver traced below.

The more useful finding is in MAIN itself: a USB device driver using
`0xFC0B0000`, descriptor queues, an 88-byte stereo OUT format, and a renderer
that consumes the received PCM. It is separate from the SSI1 codec input ring.
The register interpretation follows the pinned emulator's
`tools/mk1doc_mmio.py` and the native driver's endpoint queue/prime operations;
this is code evidence, not a hardware bus measurement.

## Native receive chain — static OS 1.54 evidence

Hash-pinned [facts and instruction anchors](../evidence/r07/handoff-facts.json),
reproduced by [r07_handoff_map.py](../tools/r07_handoff_map.py):

| Stage | Address / behavior |
| --- | --- |
| OUT allocation | `0x4000315C`: 16 descriptors at `0x80008800`, stride 64; payloads at `0x8000BA00`, stride 88 |
| OUT submission | `0x40003222`: internal endpoint index 3 passed to `0x40005CD4`; queue-offset table maps index 3 to `0x180`, EP3 OUT |
| Hardware handoff | `0x40005CD4` links descriptors under an interrupt mask and writes USB ENDPTPRIME at `0xFC0B01B0` |
| Consumer | `0x400035F4`: current descriptor `0x4025C264`, completion/status byte at descriptor `+7`, payload pointer at `+0x30`, next descriptor at `+0x34` |
| PCM framing | Header 32 bytes; frame count in header byte 0; stereo frame 8 bytes, copied without conversion. Seven frames fit the 88-byte payload. Matches R02's OUT capacity. |
| Partial packets | Cursor `0x4025C018` survives calls; remaining packet frames are consumed on the next block |
| Packet release | Clear payload byte 0, call `0x4000315C(0)` to queue a replacement, advance to next descriptor |
| Underflow | Clear unfilled output frames through `0x400E8514`; never wait for a packet |
| Render destination | `0x4007758E` requests 32 frames into `0x80001F60 + 256 * (selector XOR 1)` |
| MAIN consumer | `0x40072194..0x4007221E` reads `0x80001F60 + 256 * selector`; selector at `0x4199F134` toggles at `0x40078352` |

The boot dispatcher loads the native initializer `0x40006E0C` into A2 at
`0x400692A0`. It calls it in each traced USB-selection branch: mode 4 in the
board-flag branch, otherwise setting byte `0x40649124` selects mode 6 (value 1),
mode 5 (value 2), or mode 2 (default). The initializer stores that mode at
`0x406104F4`, returned by `0x40006E04`. These are static branch facts, not an
observed settings read from the user's unit. Mode 2 uses the native 88-byte
Overbridge-style receiver; the physical endpoint capacity agrees with it.

The render call selects `0x4000413C` for USB mode 5 and `0x400035F4` otherwise;
mode is returned by `0x40006E04`. The native Overbridge-style receive routine
above has been executed in isolation. The alternate routine, startup alignment,
mode transitions and the complete boot sequence are not executed here.

**Ownership:** USB owns a submitted descriptor until completion; the audio
consumer uses completed payloads, retains a partial-packet cursor, then releases
and replenishes them. Rendering fills the opposite PCM half to the one MAIN
currently reads. An integration must use the half MAIN is about to consume,
not blindly use the half just filled by the receive call. This is a block of
software staging; it is not a measured end-to-end latency.

The independently checked clock writes are now in the JSON: eDMA17, SSI0 47,
SSI1 50, DSPI0 23, DSPI1 15 and USB OTG44. In particular SSI1 is enabled at
`0x40000FA0`. Clock requests establish neither wiring nor USB ownership.

## OS 1.54 execution evidence

[Results](../evidence/r07/receive-154.json),
[probe](../tools/r07_receive_probe.py). Uses the R03 source-built patched Unicorn,
with synthetic completed descriptors and test doubles for libc copy/zero and
USB requeueing. It does not boot the complete device or emulate USB DMA.

- Empty queue: all 32 frames become zero; output guards stay intact.
- Two packets: 14 exact stereo frames followed by 18 zero frames.
- Five packets: 32 exact frames, four packets released, cursor 4 into packet 5.
- Ten packets over two calls: 64 exact frames, nine releases, cursor 1 into
  packet 10. No skipped/repeated PCM across the block boundary.
- Stock MAIN mix: synthetic USB input produces nonzero output. Copying its left
  Q31 words into track 0 and clearing **both** dry channels produces exact zero
  USB contribution. A separate internal stereo signal survives unchanged.

This demonstrates a **firmware-level dry-copy suppression mechanism**, independent
of whether the UI gain minimum mutes. The copy/clear operation is a host-side
experiment, not an installed mod. The source copy remains intact through the
mix; filter/amp behavior is covered separately by R03 on 1.53. Full 1.54 track
integration and runtime budget remain I02/H02 work.

## Reproduce

Keep firmware/sections private. Use the Python environment described in R03:

```sh
"$PY" tools/r07_handoff_map.py --sections "$SECTIONS154" \
  --syx "$DT154_SYX" --json evidence/r07/handoff-facts.json
PYTHONPATH="$PWD/private/r03-runtime:$PWD/deps/digiemu" \
  "$PY" tools/r07_receive_probe.py --sections "$SECTIONS154" \
  --json evidence/r07/receive-154.json
```

Both refuse a mismatched MAIN hash; the static tool also checks section 8 and
an optionally supplied container. Unicorn runs successfully outside the sandbox
that denies executable memory. That restriction is not a project-wide emulator
blocker. No R03 fixture rebuild or R03 rerun was necessary for these checks.

## Gate result and next work

**R07 DONE:** native USB buffer ownership, completion/cursor handling, render
half selection, boot dispatch and a deterministic dry-MAIN suppression mechanism
are identified on OS 1.54, with focused receive/mix execution evidence. This is
not a hardware timing or full-device qualification.

D01 is ready to assess a one-track **ColdFire copy/clear** architecture. At the
pre-processing insertion point, preserve the left channel of the half MAIN will
consume, then clear both dry channels. Do not clear the next half just filled
by the receiver. Changing section 8 is not an evidenced prerequisite for this
native path. I02 must verify hook ordering, sample scaling/polarity, zero-source
replacement and full-track behavior on 1.54; H01/H02 must qualify the physical
stream, mode selection, clocks, latency and recovery before any production GO.

The stock UI gain minimum and its dB law remain unmeasured, but they are no longer
a dependency for dry-copy suppression. No internal wiring or logic-analyser
modification is currently warranted.
