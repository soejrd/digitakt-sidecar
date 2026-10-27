# I02 — One-track native USB return prototype

Target: Digitakt mk1 OS 1.54. Prototype source:
[usb-track](../mods/usb-track/usb_track.c),
[manifest](../mods/usb-track/mod.json). This consumes the stereo USB return
for one mono source on track 1 (zero-based track 0), through stock track
processing. It adds experimental machine **USB1**, ID 121; the SRC knobs are
blank pending I03. Select it only on track 1. No hardware was flashed.

## Implementation

The mod subscribes to `ev_render_voices`, order 20, via core 4.1 and
machine-pages 1.5. It owns no stock hook site. The pinned SDK's verified
1.54 port uses `0x40077FBA`; its displaced instruction differs from 1.53.
The event wrapper preserves the integer registers and restores that instruction.
USB addresses absent from the SDK remain isolated in this experimental adapter;
they are pinned to 1.54 and [R07](R07-usb-coldfire-handoff.md).

Once the renderer activates USB1 on track 0, the handler copies 32 signed left
words from `0x80001F60 + 256*selector` into the 128-byte track source, clearing
both dry channels as it goes. It leaves the next half untouched in normal
operation. No conversion, allocation, firmware call or USB descriptor access
occurs in the callback. Normal source polarity/scale is a direct word copy;
signed extrema are preserved exactly in the compiled-function test.

First activation, invalid selector, non-native USB mode or loss of the sync
flag produces a zero track block and flushes both stereo halves. Re-entry
requires a flush before passing audio. With another render machine selected,
the adapter resets its armed state and leaves ordinary playback/USB monitoring
alone. The stock receiver still owns packets and supplies zeroes on underflow;
[R07's receive tests](../evidence/r07/receive-154.json) cover that separate step.

**Host contract:** send silence until the renderer has activated USB1. UI
selection and render activation are distinct; activation was exercised through
a real panel trigger. Audio sent earlier can take the normal dry MAIN path.
The synthetic host obeys this contract; a real handshake is I03 work. The
prototype does not yet provide a host acknowledgement or an eight-control link.

## Evidence and acceptance

[Full results](../evidence/i02/routing-154.json),
[build provenance](../evidence/i02/build.json),
[reproduction tool](../tools/i02_probe.py).

The actual linked candidate boots from its own patched MAIN and reaches the
UI in a fresh emulator run. A generated sample is loaded into its project
and assigned to two tracks. The compiled event handler runs during complete
OS 1.54 render cycles; Python supplies synthetic PCM in the consumed USB half
and synthetic mode/sync flags, rather than performing the copy/clear.

Checks include:

- Both half selectors and the order `render → source hook → MAIN → toggle`.
- A 3 kHz replacement reaches output through the track; filter closure gives
  over 70 dB attenuation, AMP volume zero gives exact silence, and zero PCM
  replaces the stock sample. A nonzero right input checks stereo dry suppression.
- Raising the track's reverb send leaves a tail after the USB tone stops.
- All seven neighboring source blocks are byte-identical immediately before
  and after the handler in full-render tests. Track 2 remains audible while
  routing runs. Its output level is compared with a diagnostic bypass within
  3%; complete output PCM identity is not claimed.
- The compiled function preserves source/USB guards and unrelated tracks with
  signed extrema, both halves, inactive mode, startup, disconnect flags,
  invalid selector and re-entry. Normal copy takes 312 guest instructions;
  the largest tested path takes 515. These counts exclude the SDK wrapper and
  are **not** a CPU-cycle or full-render deadline measurement.

The linker/linter check imports, shared sites, conflicts and memory budgets.
The generated container is reread and verified; sections 2, 4, 5 and 8 stay
byte-identical to stock. Firmware, mods, maps, cards, snapshots and PCM stay
private; only source, hashes and measurements are tracked.

## Reproduce

Use the R03 patched runtime and the emulator's compatible Python environment.
Executable-memory restrictions require running Unicorn outside this sandbox.
Each preparation root must be new; the commands do not overwrite fixtures.
`ELEKLOADER_CROSS` defaults to `m68k-elf-` for the installed Mac toolchain.

```sh
export PYTHONPATH="$PWD/private/r03-runtime:$PWD/deps/digiemu"
"$PY" tools/i02_probe.py prepare --root private/i02-fixture \
  --syx "$DT154_SYX"
"$PY" tools/i02_probe.py candidate --root private/i02-final \
  --stock "$DT154_SYX" --fixture private/i02-fixture
"$PY" tools/i02_probe.py check --root private/i02-fixture \
  --candidate private/i02-final --output evidence/i02/routing-154.json
```

The first fixture checks the exact stock container/MAIN hashes. The supported
upstream release-overlay mechanism registers that hash without editing shipped
emulator profiles. Its boot acceptance checks the card; OS 1.54 first-boot RAM
acceptance addresses remain unmapped. Successful sample playback is an
additional functional check, not a claim of full strict hardware emulation.

## Human hardware trial and recovery gate

The local candidate is `private/i02-final/artifacts/I02.syx`, label I02A.
Its tested hash is in the evidence. **It is not yet a hardware-qualified build.**
H02 remains blocked on H01 and a separately agreed human trial. Before that
trial, confirm the live native receive path, wire format/rate, clock behavior,
quiet start, and the host-silence contract. Measure complete render headroom,
onset latency/jitter and a ten-minute zero-gap run; agree the musical timing
limit. Real disconnect detection, startup alignment and reconnect freshness
must be observed: synthetic flag transitions do not establish those behaviors.

For the eventual human trial:

1. Back up projects/sounds and retain the user's original stock 1.54 file and
   its verified hash. Confirm access to the mk1 recovery menu and a working
   transfer connection before attempting a modified OS installation.
2. Follow the pinned elekloader recovery instructions: hold FUNC at power-on,
   select TRIG 4 / OS UPGRADE, and restore the original stock file using
   Transfer's legacy OS upgrade mode. This recovery procedure is documented
   upstream; it has not been rehearsed on this unit in I02.
3. Only after approval of the concrete candidate and recovery setup, have the
   human install it. Lower speakers/headphones first; use silence followed by
   a quiet, limited/faded-in tone. Activate USB1 on track 1 before sending PCM.
4. Verify filter/amp/send control and absence of a dry bypass, with an internal
   second track. Stop and restore stock if behavior differs or transport fails.

No bootloader or internal wiring change is part of this candidate. The Pi
limiter/fade-in must remain active; this track adapter does not replace them.
