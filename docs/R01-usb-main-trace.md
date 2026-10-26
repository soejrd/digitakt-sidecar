# R01 — Digitakt mk1 OS 1.54 USB MAIN receive path

Follow-up evidence: [R02](R02-usb-descriptors.md) captured the physical USB descriptors; [R03](R03-REPRODUCTION.md) proved the destination insertion point on the 1.53 emulator. **R07 supersedes this report’s USB ownership/handoff assumptions:** MAIN contains a native USB receiver with separate stereo staging buffers. See [R07](R07-usb-coldfire-handoff.md) for traced addresses, OS 1.54 execution evidence and the remaining hardware qualification. The section-8-only architecture below is historical, not an established device topology.

Status: **research complete with a documented unresolved boundary; no hardware
flash and no audio streamed.** Owner: `codex-r01 @ 2026-10-26T18:29:14Z`.
Target: Digitakt mk1, OS 1.54. Firmware evidence below is from the local
1.54 container `Digitakt_OS1.54.syx`
(sha256 `f78ba80fa7b1da5fb0e1ff61ad61e9e71aafe79f4364fc49679f3651353e3cf6`)
and its extracted MAIN OS
(sha256 `5c58bf9e3949ef09977c5fc007a61e8d026931f67f1621238379dfb8ee4d31a2`).
The image and sections stay outside this repository. Reproduce the static
facts with `tools/dt154_trace.py`; the 1.53 addresses in inherited notes were
not assumed to be 1.54 addresses.

Evidence tags: **[1.54 code]** = decoded from the 1.54 MAIN OS image;
**[1.54 UI/data]** = string or table in that image; **[source]** = pinned
dependency source; **[Rytm]** = measured/derived on the Analog Rytm, not
transferred to Digitakt; **[hardware access]** = this session's read-only
USB inventory; **[hypothesis]** = explicitly unverified.

## 1. How USB return audio reaches the main CPU

### What the main CPU owns

The 1.54 MAIN OS runs the complete sample engine on the ColdFire. Its audio
setup `FUN_40000F64` configures SSI1 (`0xFC0C8000`), an eDMA receive channel
at TCD `0xFC045680` writing `0x80001000`, and a transmit channel at TCD
`0xFC0456C0` reading the render buffer `0x4BA9F080` [1.54 code]. The 1.54
render vector is still `0x40077420`; it reads the live receive DMA address,
subtracts `0x80001000` and `0x20`, and derives the safe input half; it reads
the transmit DMA address and compares against `0x4BA9F180` to choose the safe
output half [1.54 code]. This confirms the 1.54 receive plumbing but does not
by itself identify USB as its producer: SSI1 is the codec/serial audio port,
not a USB endpoint.

The per-track source buffer is `0x80001A18 + 128*track`, emitted before the
stock track processing [1.54 code; source: `deps/elekloader/elekloader/sdk/include/digitakt-mk1/core3.h`
describes the machine render hook as after playback has written each track's
block and before overdrive]. This is the insertion target for an external voice. The existing `ev_render_voices`
hook in the pinned SDK is the equivalent clean insertion point for a machine
that writes those 32 source words; the stock code path is independent of the
USB controller [source: `deps/elekloader/elekloader/sdk/include/digitakt-mk1/core3.h`;
source: `deps/elekloader/examples/sine-machine/sine.c`].

### Where the USB/Overbridge side lives

The `.syx` section 8 is a separate ARM/Thumb USB and MIDI interface controller,
with its own FreeRTOS and task names `USB dev task`, `MIDI task`,
`Computer/Host` [source: `deps/digiemu/docs/mk1/08-coprocessor.md` and
`deps/digiemu/docs/mk1/11-audio.md`]. That inherited finding was rechecked
against the 1.54 section 8: it is still 159,948 bytes and its sha256 is
`6943b2f27773f40f1e7857ea6fa7a121269e930bd403098e6b6c00002327dc20`, the same
hash recorded for the earlier image [1.54 data; source]. Its task/string table
is unchanged. This means **the USB endpoint owner is not the ColdFire render
code**: it is the section-8 controller. What remains unresolved is the exact
handoff of received PCM from that controller to ColdFire memory, and whether
it is read by the render routine, a mailbox task, or a shared-memory ring.

### What is actually established about the handoff

- DSPI0 (`0xFC05C000`) is the most-referenced ColdFire peripheral in the
  inherited 1.53 analysis and is not modelled [source:
  `deps/digiemu/docs/mk1/03-peripherals.md`]. A 1.54 static sweep still shows
  heavy DSPI0 use; the controller boundary is therefore at least consistent
  with a DSPI/mailbox link, but **not proven** to carry audio or to be the USB
  path [1.54 code; hypothesis].
- DSPI0 is also the flash/other-device SPI master in the emulator's boot model
  [source: `deps/digiemu/emu/bootrom.py`], so a reference alone is not enough.
- The 1.54 ColdFire image contains opcode-level references into `0x60xxxxxx`,
  the controller's code/address map, but the sweep also finds many
  data/instruction coincidences. Without a mapped shared window and a
  producer/consumer trace, those hits cannot be labelled a USB audio ring
  [1.54 code; unresolved].
- The emulator does not model DSPI0, GPIO, USB OTG or the section-8 controller
  [source: `deps/digiemu/docs/mk1/09-unknowns.md`]. Digiemu therefore **cannot
  currently demonstrate USB return reaching the renderer**; it can demonstrate
  insertion into the per-track source buffer if given known PCM (R03).

**Boundary statement:** for OS 1.54, the main CPU's audio receive/insertion
surface is identifiable (`0x80001A00`-range SSI1 receive ring,
`0x80001A18 + 128*track` source buffers, render vector `0x40077420`), and the
USB endpoint/control owner is section 8. The *USB PCM -> ColdFire* transfer
itself is not identified with source evidence in this task. That is the
specific unresolved boundary R01 must record; naming a DSPI or shared-memory
route without a mapped trace would be a hypothesis, not a trace.

## 2. Buffers, sample format, synchronization and MAIN mixing

| Item | Evidence | Status |
|---|---|---|
| SSI1 receive ring | DMA TCD `0xFC045680` writes `0x80001000`; render reads live DADDR and wraps by `0x200`; 32-byte guard offset [1.54 code] | established for serial/codec audio; USB producer unproven |
| Render buffer | `0x4BA9F080`, 512 bytes, half at `0x4BA9F180`; eDMA transmit TCD `0xFC0456C0`; 8-byte frames, 64 minor loops, CSR `6` [1.54 code; source: `deps/digiemu/emu/ssi.py`] | established |
| Per-track source | `0x80001A18 + 128*track`, 32 mono `int32_t` samples, written before the stock track processing [1.54 code; source: `deps/elekloader/elekloader/sdk/include/digitakt-mk1/core3.h`] | established as the machine/source insertion point |
| Engine rate | Digitakt mk1 engine/emulator description: 48 kHz, 24-bit sample words, 32-frame blocks [source: `deps/digiemu/devices/digitakt.toml`; source: `deps/digiemu/emu/ssi.py`] | code/emulator assumption, not a measured SSI clock |
| Internal sample width | 32-bit ColdFire words / EMAC fractional arithmetic; serial samples are 24-bit right-justified [1.54 code; source: `deps/digiemu/devices/digitakt.toml`] | established for the engine; exact USB conversion not traced |
| USB wire format | Not established on Digitakt. The inherited Rytm v2 format is a 32-byte header + 7 frames x N int32 big-endian [Rytm; source: `deps/sidecar-firmware/teensy/libraries/RytmHost/ob_block.h`] | **do not apply to Digitakt without R02** |
| Synchronization | Vector 191 is software-forced after SSI1 transmit DMA half/full; render derives input half from live DMA pointers [1.54 code] | established for audio; USB block pacing/clock not traced |
| MAIN mix | Stock track mixer/effects build MAIN; `INT TO MAIN` and `USB TO MAIN [dB]` labels exist in the 1.54 routing menu [1.54 UI/data]; release notes historically added "mute internal output from main" and "USB to main volume" under USB CONFIG [1.54 release notes, OS 1.10→1.11 section] | UI control names established; DSP gain/mute application not fully disassembled |
| USB destination selector | 1.54 image has **no** `PRE-FX`, `POST-FX`, or `SAMPLER ONLY` strings; `USB TO MAIN [dB]` is the only USB return routing label [1.54 UI/data] | Digitakt does not expose the Rytm's per-track USB destination menu in this image |

The most important negative result is the last row: **the Rytm's routing model
does not transfer.** The Rytm's incoming-USB menu (PRE-FX/POST-FX/TRACK/
SAMPLER ONLY) is documented from its manual and firmware [Rytm; source:
`deps/sidecar-firmware/docs/research-rytm-usb.md`]. The Digitakt 1.54 image
instead exposes a global return gain and an internal-to-MAIN mute/level path.
This is consistent with USB return being summed directly into MAIN, not with a
per-track USB input bus.

### MAIN dry-copy suppression

The stock 1.54 UI offers two relevant controls. Their labels and menu presence
are established; their DSP behavior is not:

- `INT TO MAIN` — most plausibly the release notes' "mute internal output
  from main" control [1.54 UI/data; 1.54 release notes; hypothesis]. The label
  suggests it suppresses the *internal instrument* contribution rather than
  the USB return, but that behavior has not been independently disassembled or
  measured here.
- `USB TO MAIN [dB]` — the "USB to main volume" setting [1.54 UI/data; 1.54
  release notes], with values visible around the menu as `PRE`, `+12`, `+18`
  [1.54 UI/data]. Setting this to its minimum is the stock candidate for
  removing the dry USB copy from MAIN.

Do **not** assume that `INT TO MAIN` mutes the USB return, and do not assume
that `USB TO MAIN` is a post-track gain or that minimum is true silence. Those
meanings are the next hardware experiment, not established facts. The Rytm has a related hazard: routing a
host pair back into the same return path can create a feedback/double-copy
loop [Rytm; source: `deps/sidecar-mods/mods/0030-fx-in/design.md`]. The
Digitakt experiment must therefore use a unique tone, a limited level, and
measure both MAIN and the USB stream rather than relying on the label alone.

## 3. Can one incoming mono stream be redirected into an existing track before filter/amp?

**Source-supported answer: not with stock 1.54 controls.** The stock path has
a per-track source buffer before filter/amp, but no Digitakt UI/documentation
evidence in the 1.54 image for selecting a USB channel as that track's source.
The only USB return label is a global MAIN gain. The Rytm's per-track USB
destination is a different implementation [Rytm].

**Candidate technical route (hypothesis, not yet proven):** a firmware mod can
copy one received USB channel into `0x80001A18 + 128*track` before the stock
track processing, while suppressing the stock MAIN copy with `USB TO MAIN`
at its minimum (or, if that control proves not to affect the same copy, by
overwriting/zeroing the return contribution at its MAIN mix point). The clean
insertion point for a generated voice already exists at `ev_render_voices`
(order 10-49, before overdrive) [source: `deps/elekloader/elekloader/sdk/include/digitakt-mk1/core3.h`].
That proves where a *machine's* generated samples enter the track; it does not
prove where *received USB samples* are available. The missing R01-R03 link is
the bounded copy from the USB receive owner in section 8 to the ColdFire
source buffer. Do not implement I02 until that copy's source buffer, format
and timing are measured or traced.

**What would falsify the route:** if section 8 only delivers a stereo MAIN
sum to the ColdFire (no pre-track USB word survives), or if the handoff is
not readable without changing the controller firmware/hardware. In that case
the one-voice goal needs a controller-side change or a different transport,
which is outside R01 and must be documented as a no-go for the stock path.

## 4. How to suppress the original dry MAIN copy

Candidate sequence, in increasing invasiveness:

1. **Stock controls first.** Set `USB TO MAIN [dB]` to its minimum and verify
   the return disappears from MAIN while the USB input stream still exists.
   Separately toggle `INT TO MAIN` and verify it affects only internal tracks.
   Record the actual values/positions and whether the setting persists. This
   is a read-only/stock experiment; no flashing.
2. **If the stock gain cannot reach zero:** locate the return gain's consumer
   in the 1.54 mixer and patch only that consumer for the test build. Evidence
   for the consumer is currently the UI/data label and the release note, not a
   disassembled gain site; do not guess an address.
3. **If the dry copy is summed in section 8:** a ColdFire patch cannot remove
   it. The controller needs its own change, or the host must avoid sending the
   dry signal on the MAIN pair (for example, use a private/extra return
   channel if the descriptors prove one exists).
4. **Never subtract MAIN from itself** as a default strategy: the Rytm work
   shows how easily return paths become feedback/double-copy loops [Rytm].

The experiment must use a quiet, unique test tone and confirm that the
original stock instrument tracks still sound with `INT TO MAIN` untouched.

## 5. Remaining unknowns and physical measurements

| Unknown | Why it matters | How to resolve |
|---|---|---|
| USB PCM owner and handoff buffer in section 8 | The central R01 gap: where received samples are before MAIN | Map the shared controller window and trace producer/consumer; needs a debugger/snapshot or a section-8 disassembly with symbols. Emulator currently lacks DSPI0/controller models |
| USB sample format/conversion on Digitakt | int32 vs int24 vs int16, endianness, scaling, clipping | Capture a known digital tone and compare bit patterns against a host reference; do not infer from Rytm |
| Whether `USB TO MAIN` attenuates the same copy that a track redirection would otherwise duplicate | Determines whether stock suppression is sufficient | Hardware A/B with a unique tone, minimum vs maximum setting, measure MAIN and USB OUT |
| Whether `INT TO MAIN` affects only internal sources | Prevents accidentally muting the intended return or internal tracks | Hardware A/B with one internal track and one USB tone |
| Clock ownership and drift between host and Digitakt | Required before a musical one-voice path | Measure USB block counter/packet cadence vs SSI1; no value is established here |
| Latency/jitter and CPU headroom | Feasibility of a real-time voice | R05 plus H01/H02 after the path exists; no hardware performance claim in R01 |

## Candidate routing experiment (stock, no flash)

This is the smallest experiment that distinguishes the remaining hypotheses.
It requires the human at the unit, speakers low, and a host that can send a
quiet tone while recording the Digitakt's USB MAIN.

1. Connect the Digitakt in **OVERBRIDGE** mode and leave all internal tracks
   empty/stopped. Set a low, unique tone (for example -30 dBFS, 1 kHz) on the
   host return, with a short fade-in and no DC offset.
2. With `INT TO MAIN` unchanged, record the Digitakt's MAIN over USB and its
   analog MAIN if available. Confirm the tone appears, then sweep `USB TO MAIN`
   through its visible values (minimum, `+6`, `+12`, `+18`, `PRE` if selectable).
   Record the exact menu positions and resulting levels. This establishes
   whether the control is a gain on the return and whether minimum is silence.
3. Put one stock sample on track 1 and trigger it. Toggle `INT TO MAIN` and
   confirm it mutes/attenuates the stock track while the host return behaves
   independently. Do not leave either source at a level that can feed back.
4. Use R02’s captured 88-byte host→device endpoint. An extra return pair has not been established. R03 already proves emulator insertion on 1.53; R07 must identify the actual received PCM before adapting it to the 1.54 hardware path.

Success for the stock experiment: a documented level table for `USB TO MAIN`,
a clear statement of what `INT TO MAIN` mutes, and either a confirmed
zero-dry-copy setting or a precise statement that the dry copy is outside
ColdFire control.

## Evidence and reproducibility

- [1.54 static facts](../evidence/r01/os154-static-facts.json): container/MAIN
  hashes, anchors, string presence and absolute-pointer hits.
- [Physical USB capture](R02-usb-descriptors.md): decoded endpoints and raw evidence.
- `tools/dt154_trace.py --help`: reproducible static checks; it never copies
  firmware into the repository.
- `tools/dt_usb_inspect.c`: read-only IORegistry + descriptor attempt; builds
  with `cc -O2 -framework IOKit -framework CoreFoundation`. It does not claim
  an interface or change a configuration.

## Decision

R01 does **not** justify D01 yet. The stock Digitakt 1.54 UI/release evidence
describes a global USB-return level and internal-to-MAIN mute, and contains no
Rytm-style per-track USB destination strings; the actual DSP sum is not fully
traced. A one-track external voice remains technically plausible only if the
section-8 handoff can be reached and a received channel copied into the
per-track source buffer without retaining the dry MAIN copy. That exact
handoff is the unresolved boundary; R02 must provide the descriptor facts, and
a focused controller/shared-memory trace (or a hardware A/B) must identify the
receive buffer before I02.
