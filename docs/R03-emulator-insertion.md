# R03 — pre-filter track insertion in the emulator

Status: **IN_PROGRESS — emulator runtime solved; insertion not yet verified.**
Owner: `codex-r03 @ 2026-10-26T19:39:03Z`. Target: insert known PCM into one
Digitakt track before its filter/amp, show that filter/amp changes affect the
output, and show stock tracks still work.

No firmware was flashed and no hardware audio was sent.

## What is established

### Insertion surface

The Digitakt mk1 render writes each track's 32-sample source block at:

```
0x80001A18 + 128 * track
```

The stock pipeline then runs that block through the track's filter, amp and
mixer [source: `deps/elekloader/elekloader/sdk/include/digitakt-mk1/core3.h`,
`ev_render_voices`; source: `deps/elekloader/examples/sine-machine/sine.c`].
The same source buffer exists in the 1.54 image (R01), whose render vector is
`0x40077420` and whose SSI1 receive ring starts at `0x80001000`.

The stock 1.53 MAIN OS used here has sha256
`4b47a9507758ca5669ca02ab2c0374d2c04c98aece445408295cc1dcb265c5df`
and is 2,475,584 bytes. Its direct references to `0x80001A18` include
`0x40071F68`, `0x40071F88`, `0x40071FCE`, `0x40072064`, `0x40072268`,
`0x4007616C`, `0x40077FC4`, `0x4007801A`, `0x4007806C` and `0x400782BA`.
Its render/transmit buffers are `0x4BA8F080` and `0x4BA8F180`.
These are static references, not a demonstrated injection.

### Emulator version scope

The pinned digiemu source (`f57c2f237e84b8ff91bc3adb2a2931bccf9928e9`) names
Digitakt mk1 OS **1.53** as the tested release. OS 1.54 “sets up and boots
once you confirm, but has not been tested further” [source:
`deps/digiemu/docs/releases/v0.4.1.md`, `deps/digiemu/README.md`].
R03 therefore uses 1.53 for execution evidence; 1.54 remains a static-address
result unless its emulator support is demonstrated.

## Runtime finding: not a firmware-version problem

Both 1.53 and 1.54 would be acceptable. The original failure happened before
any firmware was loaded:

- `bootstrap.first_run()` completed extract/card, then the cold-boot `ladder`
  exited **status 132 (SIGILL)** with no snapshot.
- `python -m emu.unicorn_compat` also exited **132**.
- The macOS crash report placed the trap at
  `init_cache_info -> machine_initialize -> uc_init_engine -> uc_mem_map`.

Root cause: this execution environment denies
`sysctlbyname("hw.cachelinesize")` with `EPERM`. Unicorn then falls back to
`mrs CTR_EL0`, which traps with SIGILL on this host. A bare
`Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN).mem_map(0, 0x1000)` reproduced it with
both available libraries.

### Host-only workaround that worked

A temporary copy of the alternative local `libunicorn` was made under `/tmp`
and its fallback `mrs CTR_EL0` was replaced with a fixed 128-byte cache-line
value. The copy was ad-hoc signed. With `PYTHONPATH` pointing at that copy:

- all five `emu.unicorn_compat` checks passed;
- a clean stock 1.53 boot completed: 10 tasks, live UI, first boot settled at
  750M instructions, accepted, and `gui.snap` written.

This workaround is **not tracked** and is not a substitute for the upstream
installer. Prefer running the emulator outside this sandbox, where
`hw.cachelinesize` works, or building the pinned patched Unicorn with
`deps/digiemu/tools/install-patched-unicorn.sh`.

Logs: [bootstrap attempt](../evidence/r03/bootstrap-attempt-2026-10-26.log),
[compatibility check](../evidence/r03/unicorn-compat-2026-10-26.log),
[minimal `mem_map` failure](../evidence/r03/unicorn-map-failure-2026-10-26.log).

## What the probes showed

Using the clean stock 1.53 `gui.snap` and the temporary runtime:

- Track 0's block is zero-filled by stock code at `0x40076204`/`0x40076208`
  and processed around `0x4007257C`/`0x400725FE`.
- A direct poke of a 32-sample sine into `0x80001A18` produced **zero audio**.
- A hook at `0x40077FBA` produced **no hits** in this stock session.
- A hook at `0x40077FCE` produced **no audio**.
- A narrow execution trace over `0x40076000..0x40078500` saw only
  `0x40077420` and `0x40076C80`, 30 times each; `0x40077FCE` was not entered.

The saved stock session had no active sample or source on track 1. The
injections were therefore reaching a silent/empty track path, not a
demonstrated pre-filter insertion. See
[emulator probe log](../evidence/r03/emulator-probes-2026-10-26.log).

## Remaining R03 work (exact next test)

Acceptance is **not** met. The next session should:

1. Load a sample onto track 1 through the stock UI (or a pre-populated card)
   and trigger it; first prove the stock track is audible.
2. Verify nonzero stock audio through `session.pcm`. If it is silent, stop and
   diagnose sample loading before changing insertion addresses.
3. Inject the known Q31 sine after the stock source write but before the
   filter/amp stage. Use the observed write/processing sites to select the
   actual hook point, not `0x40077FBA` blindly.
4. Change filter cutoff and amp level through the stock UI or
   `fw_set_param(value, track, slot)` and show measurable output changes.
5. Repeat with a second stock track playing and confirm no regression.
6. State explicitly whether 1.54 runs faithfully; do not substitute a 1.53
   result for a 1.54 claim.

No filter/amp sweep was completed, so R03 remains an open test. The insertion
surface is established, and the runtime obstacle is now reproducible and
bypassable outside this sandbox.
