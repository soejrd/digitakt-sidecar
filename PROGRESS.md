# Progress log

TASKBOARD.md owns current status. Append a session entry; preserve earlier evidence and corrections.

## 2026-10-26 — standalone handoff setup (S00)

- Established a fresh local repository with durable agent instructions, an execution plan, task ownership/dependencies, source pins and a bootstrap tool. The previous exploration remains intact.
- Target confirmed by human: Digitakt mk1 OS 1.54, Raspberry Pi 4B; Teensy available as a later target.
- Imported sanitized historical Sidecar portable-test evidence: all portable tests passed, including 22 Node tests. Initial sandbox run failed on localhost restrictions; unchanged tests passed with local networking access. Full synthesis-core dependency tests were not run. Historical TRX-B2 native tests had zero failures.
- Exact upstream Digitakt guide was compared with the supplied copy during the preceding investigation. The updated guide specifies public source and v0.2.0; it retains a stock-only experiment scope. This project pursues a separate, deeper firmware-integration workstream.
- Open feasibility issue: Overwitch lists two host→Digitakt channels and twelve Digitakt→host channels. These are source facts, not measurements of this OS 1.54 unit. MAIN-to-pre-filter routing is still hypothetical.
- Setup verification passed: `python3 scripts/bootstrap.py --source-root ..` created all five dependency clones at the manifest pins; a second invocation preserved them without checkout/reset. All maintained Markdown links resolve, all five dependency HEADs match their pins, and historical evidence logs contain no `/Users/` paths. The imported upstream guide is a source snapshot; its website-relative links are not part of the maintained-link check.
- Next: R01 receive-path research; R03–R06 are independently ready. R02 awaits hardware visibility.

## Session entry template

Date/time and owner; task IDs; artifacts/commit; commands and actual results; measured versus inferred findings; blockers with release conditions; next action.

## 2026-10-26 — R01 USB MAIN receive trace (codex-r01)

- **R01 claimed** at `2026-10-26T18:29:14Z` and completed as a research task with a documented unresolved boundary, not a proven USB-to-track route.
- Used the local lawful OS 1.54 container (sha256 `f78ba80fa7b1da5fb0e1ff61ad61e9e71aafe79f4364fc49679f3651353e3cf6`) and extracted MAIN OS (sha256 `5c58bf9e3949ef09977c5fc007a61e8d026931f67f1621238379dfb8ee4d31a2`); no firmware is tracked. Original 1.53 addresses were checked rather than assumed, and 1.54 equivalents were used.
- **1.54 code evidence:** SSI1 receive eDMA `0xFC045680` -> `0x80001000`; render/transmit buffer `0x4BA9F080` (half `0x4BA9F180`); render vector `0x40077420` reads the live receive/transmit DMA pointers; per-track source buffers at `0x80001A18 + 128*track`.
- **1.54 UI/data evidence:** `INT TO MAIN`, `USB TO MAIN [dB]`, and `PRE/POST FADER` occur in the routing menu; Rytm strings `PRE-FX`, `POST-FX`, and `SAMPLER ONLY` are absent. The OS 1.10->1.11 release notes describe the historical "mute internal output from main" and "USB to main volume" USB CONFIG features; these are context, not a 1.54-only change.
- **Rytm evidence kept separate:** the Rytm's per-voice USB inputs and per-track incoming-USB destination menu are documented in `deps/sidecar-mods/mods/0021-ext/design.md`, `0030-fx-in/design.md` and `deps/sidecar-firmware/docs/research-rytm-usb.md`. None is treated as Digitakt behavior.
- **Unresolved boundary:** section 8 remains the USB/MIDI controller owner (same 159,948-byte image sha256 `6943b2f27773f40f1e7857ea6fa7a121269e930bd403098e6b6c00002327dc20`), but the exact USB PCM handoff to ColdFire memory was not traced. DSPI0 is heavily referenced but not proven to carry the audio; the emulator does not model DSPI0, GPIO, USB OTG, or section 8. No shared-window mapping or producer/consumer trace exists yet, so naming a DSPI/shared-memory ring would be a hypothesis.
- **Hardware access:** user-connected Digitakt was visible in IORegistry as `1935:0B2C`; six interfaces observed (class `255` interfaces 0-3, AudioControl-like class 1/1 interface 4, MIDIStreaming-like class 1/3 interface 5). Raw configuration descriptor access failed with `0xE00002BE` (exclusive access) because the Elektron Overbridge DriverKit service owns it. No interface claimed, no configuration changed, no audio streamed, no descriptor sizes/intervals claimed.
- **Artifacts:** `docs/R01-usb-main-trace.md`, `evidence/r01/os154-static-facts.json`, `evidence/r01/usb-access-2026-10-26.txt`, `tools/dt154_trace.py`, `tools/dt_usb_inspect.c`.
- **Checks:** `python3 tools/dt154_trace.py --help`; trace assertions on the 1.54 hashes and string presence passed; `cc -O2 -Wall -framework IOKit -framework CoreFoundation tools/dt_usb_inspect.c` built; repeat inspector run observed the device and the same exclusive-access failure. Report-relative links exist.
- **Board:** R01 DONE; R02 remains BLOCKED with the new exact release condition (close/disable Overbridge host/dext, or capture from a second host); D01 remains BLOCKED pending R01/R02/R03.
- **Next action:** R02 descriptor capture when the human can free the device from the Overbridge dext, then a focused section-8/shared-window trace or hardware A/B for the stock `USB TO MAIN`/`INT TO MAIN` behavior. R03 can proceed independently on per-track insertion in the emulator.

## 2026-10-26 — R02 descriptor capture and access-diagnosis correction

- R02 DONE: compiled the existing read-only inspector and ran it outside the execution sandbox. Full 263-byte configuration capture succeeded without changing the Overbridge driver, claiming interfaces or streaming audio.
- Corrected inherited error interpretation: Apple SDK defines 0xE00002BE as kIOReturnNoResources, while kIOReturnExclusiveAccess is 0xE00002C5. Driver ownership was not proven as the cause. Earlier entries and raw logs remain historical observations; this entry supersedes their diagnosis.
- Physical alt-3 audio endpoint capacities: 368-byte IN (Digitakt→host), 88-byte OUT (host→Digitakt), interrupt endpoints, bInterval 1. These fit source-described 12/2 framing; sample format/rate and actual streaming remain unmeasured.
- Evidence: docs/R02-usb-descriptors.md, evidence/r02-usb-descriptors-2026-10-26.txt and evidence/r02-endpoints-2026-10-26.json. Descriptor length/bounds and alt-3 sizes validated.
- I01 is READY for directional host layout work; R03 and the USB→ColdFire handoff still gate integrated routing. No firmware or hardware configuration changed.

## 2026-10-26 — I01 directional USB layouts (codex-i01)

- **I01 claimed** at `2026-10-26T18:46:33Z` and completed against R02's physical Digitakt fixture.
- **Directional layout:** R02 proves alt 3 is 368 B / 12 channels device→host and 88 B / 2 channels host→device. The Rytm's 368/368 remains a separate, preserved layout.
- **Host codec:** `ob_block.h` now exposes `in_bytes`/`out_bytes`, derives each from its endpoint's `wMaxPacketSize`, and refuses non-block endpoints independently. `find_layout` no longer assumes equal channel counts.
- **Pi/Circle transport:** endpoint scan accepts any whole-block interrupt endpoint, stores the actual IN/OUT channels and bytes, allocates separate URB lengths (buffers remain sized for the 368-byte maximum), serializes only the advertised OUT channels, and parses IN blocks with `m_nInChannels`. Zero-fills missing channels for fixed 12-channel engine consumers. The existing Circle factory patch now routes both `ven1935-b48` and `ven1935-b2c`; PID matching is not treated as sufficient, since the function driver still validates alt 3 and endpoints.
- **UAC2 gadget:** `FromRytmBlock` accepts the incoming channel count (default 12), preserving Rytm behavior.
- **Artifacts:** [I01 report](docs/I01-directional-usb-layout.md), [replay patch](patches/sidecar-firmware/0001-directional-overbridge-layout.patch), [patch instructions](patches/sidecar-firmware/README.md), `scripts/test_i01.sh`, `tests/test_usb_layout_policy.cpp`, `tests/test_digitakt_layout.cpp`.
- **Checks (actual):**
  - `./scripts/test_i01.sh` -> upstream `ob_block: all checks passed`; `USB layout policy: PASS (Digitakt 12/368 IN, 2/88 OUT; Rytm 12/12)`; `Digitakt layout: IN 12 ch / 368 B, OUT 2 ch / 88 B`; `I01 checks pass`.
  - Clean checkout at the pinned sidecar revision + `git apply` of the saved patch -> `ob_block: all checks passed`; patch applies with `--check`.
  - `make -C deps/sidecar-firmware/pi/engine/tests` -> `ALL PI PORTABLE TESTS PASS`.
  - Full upstream `tools/run_portable_tests.sh` fails only its localhost mock-web tests in the sandbox (`mock_web answers`); the C/C++ portions and the new I01 tests pass. An escalated localhost retry was rejected by policy, so this is recorded as an environment limitation.
- **Limits:** no Circle tree was available for a full Pi kernel cross-build; no USB stream or hardware test was performed. The Circle factory change is verified as a clean patch against a padded synthetic factory, not a compiled kernel. H01 must prove packet sizes/channel placement/format/rate/latency and a zero-gap run.
- **Board:** I01 DONE; H01 READY (its I01 dependency is complete and the hardware is visible); D01/I02 remain blocked by the unresolved R01 USB→ColdFire handoff and R03.
- **Next action:** H01 build/stream test (or R03 independently). Do not claim MAIN-to-track routing from I01; this task only establishes the directional USB transport layout and tests.

## 2026-10-26 — R03 emulator insertion attempt (codex-r03)

- **R03 claimed** at `2026-10-26T19:39:03Z`. No acceptance result claimed: the task is **BLOCKED** on the emulator runtime.
- **Established:** the 1.53 track-source insertion surface is `0x80001A18 + 128*track`, before the stock filter/amp path, and the 1.53 stock MAIN image (sha256 `4b47a9507758ca5669ca02ab2c0374d2c04c98aece445408295cc1dcb265c5df`, 2,475,584 bytes) references it at the expected render sites. The pinned emulator supports 1.53 as its tested release; 1.54 is documented as boot-only/not further tested.
- **Prepared:** a clean stock 1.53 firmware folder in `/tmp/r03-fw` (not tracked), `scripts/r03_bootstrap.sh`, and a documented experiment: write known nonzero PCM into track 1's 32-sample source block, capture audio with neutral filter/amp, sweep cutoff and amp controls, and regress a stock track.
- **Actual blocker:** `bootstrap.first_run()` completed extract/card, then the cold-boot `ladder` exited **status 132 (SIGILL)** with no snapshot. `PYTHONPATH=deps/digiemu python -m emu.unicorn_compat` also exits **132**. The emulator docs require the patched Unicorn; all three local digiemu venvs contain the stock 2.1.4 wheel. `install-patched-unicorn.sh` needs Unicorn source commit `8028ec436f2d9376525352dd38ed9ed6b9f6be10`; no local source checkout was found and `github.com`/`pypi.org` DNS resolution failed.
- **Evidence:** [R03 report](docs/R03-emulator-insertion.md), [bootstrap attempt log](evidence/r03/bootstrap-attempt-2026-10-26.log), [compatibility log](evidence/r03/unicorn-compat-2026-10-26.log).
- **Board:** R03 BLOCKED with release condition: install a compatible patched Unicorn 2.1.4 m68k runtime, then run `scripts/r03_bootstrap.sh` and the documented probe. D01 remains BLOCKED (R03 incomplete and the R01 USB→ColdFire handoff unresolved). H01 and R04–R06 are independently available.
- **Next action:** install/build the patched Unicorn, re-run the 1.53 bootstrap, then execute the insertion/filter/amp probe. Do not treat the static address evidence as a successful emulator test.

- **Additional check:** the alternative `libunicorn.2.dylib` under
  `digitakt modding/digiemu/.venv` (sha256
  `a0db6bebc7506245be341971a4b7a67d012aa37ee3e46ebc39679b88137e503f`) also
  crashes. Its macOS crash report places SIGILL in
  `init_cache_info -> machine_initialize -> uc_init_engine -> uc_mem_map`,
  before any guest instruction. This reinforces that the blocker is the
  runtime build, not the insertion point.

## 2026-10-26 — R03 runtime diagnosis and probe findings (codex-r03)

- **Correction to the earlier R03 entry:** the blocker was not simply “stock Unicorn vs patched Unicorn.” This environment denies `sysctlbyname("hw.cachelinesize")`; Unicorn falls back to `mrs CTR_EL0`, which traps with SIGILL at `init_cache_info -> machine_initialize -> uc_init_engine -> uc_mem_map`. Both 1.53 and 1.54 are acceptable; the failure occurs before firmware load.
- **Host-only workaround:** a temporary `/tmp` copy of an alternative local `libunicorn` with the fallback cache probe replaced by a fixed 128-byte line size passed all five `emu.unicorn_compat` checks. A clean stock 1.53 bootstrap then produced `gui.snap` (live UI, settled at 750M, first boot accepted). The workaround is not tracked; outside this sandbox, the sysctl path should work, or use the pinned patched-Unicorn installer.
- **Probe results:** track 0's block is zero-filled at `0x40076204/08` and processed near `0x4007257C/FE`; direct poke to `0x80001A18` produced zero audio; hooks at `0x40077FBA` and `0x40077FCE` produced no audio (the former had no hits). The stock session had no active sample/source on track 1, so the test did not reach an audible track path.
- **Status:** R03 remains IN_PROGRESS. No nonzero inject/filter/amp acceptance evidence exists; R03’s exact next step is to load and trigger a stock sample, then inject after the source write and sweep filter/amp. 1.54 emulator fidelity remains unproven.
- **Evidence:** [R03 report](docs/R03-emulator-insertion.md), [compatibility log](evidence/r03/unicorn-compat-2026-10-26.log), [probe log](evidence/r03/emulator-probes-2026-10-26.log). Personal paths were removed from the tracked logs.

## 2026-10-26 — reference triage and R03 handoff clarification

- Reviewed newly supplied Digitakt/Digitone MCP README, Elektroid README and current elk-herd landing page. Saved applicability and scope in docs/REFERENCE-TRIAGE.md; no new dependencies installed. These references support MIDI automation/transfers rather than proving pre-filter audio injection.
- Corrected the stale TASKBOARD footer: the R03 report already describes a runtime workaround and successful boot; the immediate experiment needs an assigned, triggered sample and nonzero stock PCM before another injection attempt. Temporary runtime workaround remains a reproducibility limitation. R03 ownership and acceptance criteria are unchanged.
- Recommended the existing emulator sample-loader flow instead of introducing a new transfer/MCP stack. H01 remains independent; USB→ColdFire handoff remains unresolved.
- Validation: checked new local reference links and reviewed the documentation-only diff. No emulator or hardware test was performed in this triage session.

## 2026-10-26 — R03 stop record (codex-r03, work halted at human request)

- **Task:** R03 remains **IN_PROGRESS**; acceptance is not met. Work was stopped before loading/triggering a stock sample or attempting another injection. No firmware was flashed and no hardware audio was sent.
- **Recorded findings:** added a [stop-point record](docs/R03-emulator-insertion.md#stop-point-record-2026-10-26t1959z) to the R03 step document. It separates the host Unicorn failure (`hw.cachelinesize` denied, fallback `mrs CTR_EL0` SIGILL at `uc_mem_map`), the temporary 1.53 boot workaround, and the guest-audio result (no active source; zero-audio probes at `0x80001A18`, `0x40077FBA`, `0x40077FCE`).
- **Boundary:** the exact post-source/pre-filter hook is unresolved. The 1.53 source block `0x80001A18 + 128*track`, `fw_set_param` at `0x400771e8`, filter ids 30-43, amp ids 44-51 and AMP VOL slot 15 are source facts; no filter/amp sweep or second-track regression was run. OS 1.54 remains the target, but its emulator fidelity is unproven and no 1.53 execution result is presented as 1.54 evidence.
- **Artifacts:** [R03 report](docs/R03-emulator-insertion.md); existing logs in `evidence/r03/`. No tool or dependency patch was added. The temporary `/tmp` runtime and firmware assets are untracked and not a reproducible deliverable.
- **Checks/commit:** documentation-only change reviewed with `git diff --check`; local commit recorded in the handoff. Board footer now points to the stop record.

## 2026-10-26 — project-wide reference index

- Added docs/REFERENCES.md mapping all supplied references to project-wide transport, controls, automation, persistence and sample-transfer work. Review status is explicit; listed resources are not automatically dependencies.
- Linked the index from README.md and AGENTS.md so future task owners discover it when relevant. R03-specific continuation remains in REFERENCE-TRIAGE.md.
- Validation: local reference links resolve; documentation diff checked. No implementation or hardware changes.
