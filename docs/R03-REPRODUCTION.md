# R03 passing reproduction

26 October 2026. **PASS on stock Digitakt mk1 OS 1.53 in the emulator.**
No hardware firmware update, MIDI/USB transfer or sound-card output occurred.
OS 1.54 execution and the USB-controller handoff remain unproven.

## What released the blocker

1. Rebuilt Unicorn from official commit `8028ec436f2d9376525352dd38ed9ed6b9f6be10` using all six hash-verified patches in the pinned emulator's installer. All five semantic compatibility checks passed. Running outside the restrictive sandbox lets the normal host cache query work; the temporary fixed-cache-line binary patch is no longer required. See [build provenance](../evidence/r03/runtime-build.txt).
2. Made the firmware, sections, +Drive and boot snapshot paths explicit through `FirmwarePaths.env()`. A fresh private card was booted, given a generated 440 Hz WAV through the existing ekFS sample writer, then cold-rebuilt so its cached file index matched the card. The earlier invalid-reference finding did not reproduce under this setup; its precise original cause is not established.
3. Loaded the sample **into the project and assigned slot 1 to the audio tracks**. Those are distinct operations. An unassigned track still referenced missing factory material and rendered zero PCM. The assigned track rendered nonzero PCM without changing the emulator source.
4. Used paced encoder movements. Large unpaced turns did not reliably reach the intended parameter value; in one probe cutoff remained 127 after a requested closing sweep. Batches of at most four detents with 25 ms between them reliably swept cutoff and volume. The final test uses the actual panel path rather than writing filter/amp state directly.

The one-command silent-stock reproduction was run and failed its nonzero-PCM assertion: 45,632 frames, peak 0. The loaded/assigned snapshot passed the same stock-playback check. Then the complete tracked probe passed on a freshly prepared fixture with the freshly built runtime, not the inherited temporary snapshot.

## Measured result

Full artifact: [insertion-153.json](../evidence/r03/insertion-153.json).

| Case | Peak, PCM16 | RMS, PCM16 | Interpretation |
| --- | ---: | ---: | --- |
| Unassigned source | 0 | 0 | Negative control |
| Assigned stock 440 Hz source | 1508 | 697.94 | Ordinary playback works |
| Injected 3 kHz, filter open | 1993 | 931.57 | Known replacement signal reaches output |
| Injected 3 kHz, filter closed | 1 | 0.257 | More than 70 dB RMS attenuation |
| Injected 3 kHz, AMP volume zero | 0 | 0 | Track amp controls the return |
| Zero-valued replacement block | 0 | 0 | Hook replaces the source rather than adding an unrelated output |
| Second stock track, injection off/on | 1415 / 1415 | 713.85 / 713.85 | Captured PCM identical byte for byte |

The 3 kHz coherent amplitude was 1246.57 PCM16 units for the injected signal versus 0.040 in stock playback. This distinguishes source replacement from merely hearing the existing 440 Hz sample. The hook executed thousands of times. All nine assertions passed; all captures completed without a guest halt.

## Established insertion point

For the pinned OS 1.53 MAIN image, the host code hook at **`0x40077FBA`** writes only track 0's **32 Q31 samples at `0x80001A18`** each render block. This is the machine-pages `ev_render_voices` site documented in the pinned SDK. Unlike the earlier static reasoning, the filter/amp and second-track results above now demonstrate its behavior in execution.

This is an emulator host hook. No Digitakt mod was compiled/flashed and no external USB audio was injected. A real mod would consume ready queued PCM at the equivalent site; it must not run the Python callback or wait for the host. Runtime speed/timing margins from a code-hook experiment are not hardware latency evidence.

## Repeat it

The tracked [probe](../tools/r03_probe.py) refuses firmware images whose hashes differ from its pinned 1.53 inputs. It creates private firmware/card/snapshots and emits only metrics to the evidence file. The private fixture root must be a **new directory**. A second `check` can reuse that fixture.

Use a Python 3.12+ environment containing the pinned emulator's dependencies. On a new checkout, `uv sync --project deps/digiemu` prepares that environment. The upstream installer builds its six patches from pinned public source:

```sh
PYTHON="$PWD/deps/digiemu/.venv/bin/python" \
PYTHONPATH="$PWD/deps/digiemu" \
bash deps/digiemu/tools/install-patched-unicorn.sh
```

The actual run isolated the installed library in ignored `private/r03-runtime/unicorn`, copied from the Python environment's Unicorn package before invoking the same installer with that package first on PYTHONPATH. This preserves the original environment. [r03_build_runtime.py](../tools/r03_build_runtime.py) automates those same operations for a **new** runtime directory:

```sh
"$PY" tools/r03_build_runtime.py --root private/r03-runtime
```

Its source commit, patch hashes, compiler version and output hash are retained in the provenance artifact. The library itself and firmware assets are not tracked. The isolated builder refuses to overwrite an existing directory.

With the patched library selected (and outside a sandbox that denies the host cache query):

```sh
# PY is the compatible Python interpreter. DT1_SYX is your own stock OS 1.53.
PYTHONPATH="$PWD/private/r03-runtime:$PWD/deps/digiemu" \
  "$PY" tools/r03_probe.py prepare \
  --root private/r03-fresh --syx "$DT1_SYX"

PYTHONPATH="$PWD/private/r03-runtime:$PWD/deps/digiemu" \
  "$PY" tools/r03_probe.py check \
  --root private/r03-fresh --output evidence/r03/insertion-153.json
```

If the patched library is installed directly into the environment, use only `deps/digiemu` on PYTHONPATH. No dependency source modifications are needed. Do not resume a snapshot against a different card or silently substitute OS 1.54 into this address-pinned probe.

## Remaining project boundary

R03's emulator insertion/filter/amp/regression acceptance is met. This does not resolve where USB PCM enters ColdFire memory on the actual Digitakt, how to suppress its original MAIN copy, transport latency, or OS 1.54 execution equivalence. Those are the next research/hardware gates; the passing insertion test must not be used as evidence that a complete USB sidecar already works.
