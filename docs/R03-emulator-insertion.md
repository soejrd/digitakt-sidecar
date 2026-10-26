# R03 — pre-filter track insertion in the emulator

Status: **BLOCKED on the emulator runtime; no audio execution result claimed.**
Owner: `codex-r03 @ 2026-10-26T19:39:03Z`. Target milestone: insert known PCM
into one Digitakt track before its filter/amp, show that filter/amp changes
affect the output, and show stock tracks still work.

## What is established

### The insertion surface

The Digitakt mk1 audio engine runs on the ColdFire and writes each track's
32-sample source block at:

```
0x80001A18 + 128 * track
```

This is the machine/source insertion point used by the stock track pipeline
before filter/amp and the mixer [source: `deps/elekloader/elekloader/sdk/include/digitakt-mk1/core3.h`,
`ev_render_voices`; source: `deps/elekloader/examples/sine-machine/sine.c`].
The corresponding per-track source buffer and render path are also present in
the 1.54 image (R01), with render vector `0x40077420` and the SSI1 receive
ring at `0x80001000` [1.54 code; `docs/R01-usb-main-trace.md`].

For OS 1.53, the extracted stock MAIN OS used for this task has sha256
`4b47a9507758ca5669ca02ab2c0374d2c04c98aece445408295cc1dcb265c5df`
and is 2,475,584 bytes. Its references to `0x80001A18` are at
`0x40071F68`, `0x40071F88`, `0x40071FCE`, `0x40072064`, `0x40072268`,
`0x4007616C`, `0x40077FC4`, `0x4007801A`, `0x4007806C` and
`0x400782BA`; the render/transmit buffer `0x4BA8F080` and its half
`0x4BA8F180` are present as well. These are static code/data references, not
a successful injection [source inspection].

`emu/symbols.py` resolves the production 1.53 addresses rather than the
hardcoded fallback constants in `emu/bootstrap.py`; use the profile rather
than copying a stale address.

### Emulator version scope

The pinned digiemu source (`f57c2f237e84b8ff91bc3adb2a2931bccf9928e9`) says:

- Digitakt mk1 OS **1.53** is the tested release.
- OS 1.54 “sets up and boots once you confirm, but has not been tested
  further” [source: `deps/digiemu/docs/releases/v0.4.1.md`,
  `deps/digiemu/README.md`; source: `deps/digiemu/devices/digitakt.toml`].

Therefore R03’s valid emulator evidence is **1.53**, with 1.54 support
explicitly missing/limited. A 1.54 static address result is not proof that
the emulator runs 1.54 faithfully.

## Attempted run and exact blocker

I created a clean stock 1.53 firmware folder in `/tmp/r03-fw` (not tracked),
using the pinned digiemu bootstrap:

```
python - <<PY
from emu import bootstrap
p = bootstrap.FirmwarePaths(root='/tmp/r03-fw', syx_name='Digitakt_OS1.53.syx',
                            devices_dir='.../deps/digiemu/devices')
os.environ.update(p.env())
bootstrap.first_run(p, progress=print)
PY
```

Observed result:

```
extract done in 1 s
card done in 0 s
ladder start ladder ...
process exit status 132 (SIGILL)
```

No `boot.snap`, `gui.snap` or audio was produced. The pinned emulator’s own
runtime check also exits `132`:

```
PYTHONPATH=deps/digiemu python -m emu.unicorn_compat
-> exit 132
```

This is not a track/filter finding. The emulator documentation is explicit:
“The emulator does not run without the patched Unicorn.” The six patches are
in `deps/digiemu/patches/`; `tools/install-patched-unicorn.sh` requires the
Unicorn 2.1.4 source at commit `8028ec436f2d9376525352dd38ed9ed6b9f6be10`
and `cmake`. No local Unicorn source checkout was found, and the network
clone failed DNS resolution in this environment. All three existing digiemu
virtual environments report the same `132` result, so the stock wheel cannot
be used for R03.

Exact logs: [bootstrap attempt](../evidence/r03/bootstrap-attempt-2026-10-26.log),
[compatibility check](../evidence/r03/unicorn-compat-2026-10-26.log), and the
[minimal `mem_map` failure](../evidence/r03/unicorn-map-failure-2026-10-26.log).
The `digitakt modding` digiemu venv contains a different 806 KB local-build
library, but it fails at the same `uc_mem_map` call before guest execution; it
is not a usable patched runtime on this host.

## The experiment that is ready to run

Once a compatible patched Unicorn is installed:

1. Build a clean stock 1.53 session with `scripts/r03_bootstrap.sh`
   (`DT1_SYX=... DIGIEMU_PYTHON=...`). Use the produced `gui.snap`.
2. Start `emu.Session(gui.snap, syx=..., audio=True)` with the 1.53 stock
   paths and a fresh `plusdrive.img`.
3. Select track 1 and a silent/stock source, then write known nonzero PCM
   (for example a quiet 1 kHz sine, 32 samples per render block) directly to
   `0x80001A18 + 128*0`.
4. Capture `session.pcm` with a neutral filter and amp. Confirm the injected
   tone appears, not just a constant offset.
5. Move the track's filter cutoff and amp level/sustain through the stock
   control path (preferably key/encoder events, or the documented parameter
   API), and compare:
   - cutoff change alters the spectrum/level of the injected tone;
   - amp level change alters its amplitude/envelope;
   - a second stock track still renders normally;
   - the injected samples enter the track buffer *before* filter/amp, rather
     than being added after them.
6. Record whether the same procedure runs unchanged on 1.54; if not, state
   the missing 1.54 emulator support rather than substituting 1.53 silently.

Acceptance requires **non-silent known PCM + measurable filter/amp effect +
stock-track regression pass**. A successful boot alone is not acceptance.

## Remaining unknowns

- The exact control API/addresses for changing filter/amp from a headless
  session on 1.53. Use the emulator's keyboard/encoder input or a symbol
  profile; do not guess RAM addresses from the Rytm.
- Whether writes to `0x80001A18` are overwritten by the stock source stage
  before filter/amp; the test must verify output, not just a memory write.
- Whether 1.54’s changed addresses and render layout run faithfully in this
  emulator beyond its documented boot support.
- Whether the injected 32 samples are interpreted as 24-bit/32-bit or require
  scaling; compare against the stock track format, not Rytm assumptions.

## Decision

**R03 remains BLOCKED.** The specific input needed is a working patched
Unicorn 2.1.4 m68k build for this host (or a prebuilt digiemu runtime that
includes it). The human/environment can supply it with
`deps/digiemu/tools/install-patched-unicorn.sh` once the Unicorn source is
available locally or the network permits the pinned clone. This environment
had neither: all local venvs use the stock wheel, no Unicorn source checkout
was found, and both `github.com` and `pypi.org` name resolution failed. After
that, `scripts/r03_bootstrap.sh` and the experiment above are ready.

This negative result does **not** invalidate the per-track insertion point or
the R01/R02 findings; it means the required execution evidence is absent.
