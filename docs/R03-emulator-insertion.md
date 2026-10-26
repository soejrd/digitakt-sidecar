# R03 — pre-filter track insertion in the emulator

Status: **IN_PROGRESS — runtime rerunnable; stock sample load blocked before
nonzero PCM.** Insertion still not attempted.
Owner: `codex-r03 @ 2026-10-26T19:39:03Z`. Target: insert known PCM into one
Digitakt track before its filter/amp, show that filter/amp changes affect the
output, and show stock tracks still work.

No firmware was flashed and no hardware audio was sent.

## 2026-10-26 continuation: runtime, sample fixture and stock-load result

This section supersedes the runtime-only stop point below. It records what was
actually run after the reference triage, including a negative result: stock
audio was **not** reached, so no injection, filter/amp sweep or second-track
regression was started.

### Runtime: rerunnable here, not rebuilt from tracked sources

- The restrictive sandbox is still in force. `sysctl -n hw.cachelinesize`
  returns `Operation not permitted`, and an unsandboxed escalation request was
  refused by the approval policy before execution. No claim is made that the
  emulator ran outside the sandbox.
- The local patched Unicorn fallback in a private runtime directory is
  rerunnable on this host. `emu.unicorn_compat` passes all five semantic checks
  (`compatible: true`). The active library is
  `libunicorn.2.dylib`, 819,904 bytes, SHA-256
  `c4c0b258030be6ae03cb1700762188af5d75d7583945f47678df8d3b80bd8ecc`.
  It exports the digikit symbols and names a tag-2.1.4 build tree, but that
  source tree is gone. This is therefore an exact, hash-recorded **runtime
  artifact**, not a from-scratch reproducible build. Rebuilding still requires
  the pinned installer and network access, or an accessible local Unicorn
  2.1.4 clone.

### Fixture: known WAV placed through the emulator's own loader

- Private fixture root: `<private-runtime>/r03-run-20261026/fw` (untracked; contains
  no tracked firmware or card data).
- Stock 1.53 input: `Digitakt_OS1.53.syx`, SHA-256
  `9bdd44bb6102fb25c143cfab97bc92b7a89c463f795d3112dce89771e29bcc92`;
  extracted MAIN OS SHA-256
  `4b47a9507758ca5669ca02ab2c0374d2c04c98aece445408295cc1dcb265c5df`.
- Known WAV: 48 kHz, mono, 16-bit, 48,000 frames, SHA-256
  `653f942dc514d30783b5037bb6cdf7439b7f28d1e9a9087f48b4da3e021d045a`.
- `emu.samples.plan` accepted it as `sine`; `emu.samples.write` created
  `/incoming/sine` (inode 4). A duplicate root entry (inode 5) was later added
  only to work around the browser navigation problem. The on-card sample is
  96,080 bytes: 64-byte header + 96,000 bytes of big-endian mono PCM + 16-byte
  trailer, rate 48,000, hash `0x7167c7e1`. The inode hash, hash-table word and
  recomputed content hash all agree, and the parent directory is valid.
- A cold rebuild after each card change succeeded. The last rebuild reported:
  extract up to date; card valid; ladder parked at ~160M instructions with 10
  tasks; intro reached `USER INTERFACE IS LIVE`; settle accepted the already
  initialised +Drive; `gui.snap` written. This proves the firmware mounted the
  updated card, not that it loaded a sample into the project.

### Stock playback attempt: live audio, zero PCM

`tools/livecheck.py` ran through the GUI's live 48 kHz model with the sample
selection/trigger sequence. The audio path itself was live:

```
[gui] audio: LIVE at 48000 Hz (emulated CPU 64M instructions/s)
1.2 s: audio 1.16 s  real 100%  buffered 220 ms  dropouts 1
5.2 s: audio 5.17 s  real 100%  buffered 230 ms  dropouts 1
recording 5.17 s, peak 0
```

So the emulator rendered 5.17 s of audio, but the captured PCM peak was zero.
This is a negative stock-audio result, not evidence about an injection point.

### Where the stock sample load fails

The stock UI does reach the load flow: the side-menu prompt showed
`LOAD 1 SAMPLE? CONFIRM Y/N`, and after `SELECT ALL` it showed
`LOAD 2 SAMPLES?`. The result was still `SAMPLE MEMORY FULL`, which upstream
documents as the zero-count done message, not literal memory exhaustion.

Hooking the firmware's loader and reference builder located the failure:

- `FUN_400d1680` returns `0xFFFFFFFF`.
- `FUN_400ec6d2` is entered with stack argument 0 pointing at a reference
  buffer whose first word is `FFFFFFFF` (`ff ff ff ff 00 00 00 00 ...`).
- This happens both with a single marked row and with `SELECT ALL`, although
  the UI reports 1 or 2 samples selected.

Thus the UI selection state and the reference passed to the loader disagree.
The on-card inode and hash are valid, so the immediate blocker is the
UI-to-reference bridge or the browser row state, not the sample encoding,
directory index or hash table. The exact browser key mapping was not resolved:
inside the browser, raw code 12 entered `Factory` from the root in one
observation, while code 15 moved the cursor; on `incoming`, code 12 behaved as
a checkbox/mark action. Those observations are inconsistent enough that they
are not offered as a general key map.

### Version boundary

All execution above is **OS 1.53**. OS 1.54 is still the target, but only its
static addresses are established (R01); no 1.54 emulator fidelity claim is
made here. In particular, the sample-load failure is a 1.53 emulator/session
finding and must not be presented as a 1.54 execution result.

### Exact next step

Do not inject yet. First resolve the stock-load failure by either:

1. using a pre-loaded sample image/snapshot with a proven project slot 1, or
2. tracing the firmware worker that builds the file reference from the
   browser selection and fixing the selection/reference handoff in the
   emulator (or documenting the correct stock key sequence).

Only after `emu.Session.pcm` or the live-audio recording has nonzero peak/RMS
should the post-source/pre-filter trace restart. Evidence summary:
[stock sample attempt](../evidence/r03/stock-sample-attempt-2026-10-26.log).

No tracked tool or emulator source was modified in this attempt.

## Stop-point record (2026-10-26T19:59Z)

Work stopped at the request of the human before the insertion experiment was
run. This section is the handoff snapshot; the detailed evidence and limitations
follow below.

### Settled findings

- **R03 is not complete.** The acceptance test is a known PCM signal entering
  one track before filter/amp, measurable filter/amp response, and a second
  stock track still working. None of those has been demonstrated.
- **The blocker was the host Unicorn runtime, not OS 1.53 versus 1.54.** Both
  firmware versions are acceptable inputs. The failure occurred at the first
  `uc_mem_map`, before any guest instruction or firmware-specific result.
  `hw.cachelinesize` is denied with `EPERM` here; Unicorn's fallback
  `mrs CTR_EL0` traps with SIGILL.
- **A temporary host workaround boots stock 1.53.** A `/tmp` copy with the
  cache-probe fallback replaced by a fixed 128-byte line size passed all five
  `emu.unicorn_compat` checks and completed a clean stock 1.53 first boot
  (10 tasks, live UI, settled at 750M instructions, `gui.snap` written). This
  is not a tracked dependency and is not proof of the pinned patched runtime.
  The intended fix is to run the pinned patched Unicorn outside this sandbox,
  or build it with `deps/digiemu/tools/install-patched-unicorn.sh`.

### Insertion findings and the unresolved test

- The per-track source block is `0x80001A18 + 128*track`, 32 Q31 samples,
  before the stock filter/amp/mixer [source:
  `deps/elekloader/elekloader/sdk/include/digitakt-mk1/core3.h:18`].
  `fw_set_param` is `0x400771e8` on 1.53 (`set_param(value 8.8, track, slot)`)
  [source: `deps/elekloader/mods/core/fw.s:25`]. Filter parameter ids are
  30-43 and amp ids 44-51 [source:
  `deps/elekloader/elekloader/sdk/include/digitakt-mk1/core3.h:111-113`].
  AMP VOL is sound slot 15 [source: `deps/elekloader/docs/ADAPTING.md:2348`].
  The exact cutoff/amp slot-to-id mapping was not tested in this session.
- The stock session used for the probes had **no active sample or source on
  track 1**. A direct poke into `0x80001A18` therefore produced zero audio,
  and hooks at `0x40077FBA`/`0x40077FCE` did not establish an insertion point
  (`0x40077FBA` had no hits; `0x40077FCE` produced no audio). These are
  negative results from a silent track path, not evidence against the source
  buffer as the eventual insertion surface.
- **The exact post-source, pre-filter hook has not been identified.** A narrow
  trace over `0x40076000..0x40078500` saw only `0x40077420` and `0x40076C80`,
  30 times each. `0x40077420` is the established 1.53 render vector; that
  alone does not locate the loudest/valid point to overwrite a track block.

### Version boundary

OS 1.54 is the target and its static addresses are recorded in R01, but the
pinned emulator names 1.53 as its tested release and documents 1.54 as
boot-only/not further tested [source: `deps/digiemu/docs/releases/v0.4.1.md`,
`deps/digiemu/README.md`]. No R03 injection, filter/amp sweep or stock-track
regression was executed on 1.54. A successful 1.53 emulator run would not be a
1.54 fidelity claim.

### Exact state to resume from

1. Establish a reproducible runtime from the pinned patched Unicorn, preferably
   outside the restrictive sandbox, and retain the compatibility log and
   library provenance.
2. Load a small known WAV through `emu/samples.py` / LOAD SAMPLES, rebuild so
   the +Drive index refreshes, assign it to track 1, and trigger it. Do not
   proceed until `emu.Session.pcm` proves nonzero stock audio.
3. Trace writes to `0x80001A18` in that audible session; choose the actual
   final source-producing write and inject after it, before filter/amp.
4. Sweep filter cutoff and amp level and show measurable output changes; keep
   a second stock track playing as a regression control.

No tool or firmware patch was added in this stop record.

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
