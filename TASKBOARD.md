# Task board

Authoritative status and ownership. Priority runs from top to bottom among eligible tasks.

States: READY (can start), IN_PROGRESS (claimed), BLOCKED (external input or unresolved prerequisite; specify release), DONE (acceptance met with evidence), DEFERRED (deliberately outside current milestone).

Claim format: `agent-name @ YYYY-MM-DDTHH:MM:SSZ`. A coordinator resolves concurrent claims, assigns tasks across separate clones and integrates board updates. Dependency-blocked work becomes READY only after its listed prerequisites pass. A negative feasibility result can complete a research task; it cannot complete a dependent implementation task.

| ID | Task | Status | Owner | Depends on | Blocker / release condition | Acceptance / evidence |
| --- | --- | --- | --- | --- | --- | --- |
| S00 | Create standalone handoff repo | DONE | setup agent | — | — | Plan, prompt, board, baseline, source pins and bootstrap available; docs/link and bootstrap checks recorded in PROGRESS |
| R01 | Trace Digitakt 1.54 MAIN USB receive path | DONE | codex-r01 @ 2026-10-26T18:29:14Z | — | — | [Report](docs/R01-usb-main-trace.md) traces 1.54 ColdFire audio buffers/render and section-8 USB owner; documents unresolved USB→ColdFire handoff, stock USB-to-MAIN controls, candidate routing/suppression experiment and hardware unknowns; [static facts](evidence/r01/os154-static-facts.json) and [USB access record](evidence/r01/usb-access-2026-10-26.txt) |
| R02 | Capture physical USB descriptors | DONE | codex-r02 | — | — | [Report](docs/R02-usb-descriptors.md), [raw capture](evidence/r02-usb-descriptors-2026-10-26.txt), [decoded endpoints](evidence/r02-endpoints-2026-10-26.json). Unsandboxed read-only capture succeeded; alt 3 is 368 bytes IN / 88 OUT. Earlier error was NoResources, not exclusive access; driver unchanged. |
| R03 | Reproduce pre-filter insertion in emulator | IN_PROGRESS | codex-r03 @ 2026-10-26T19:39:03Z | — | — | [Report](docs/R03-emulator-insertion.md#2026-10-26-continuation-runtime-sample-fixture-and-stock-load-result): patched runtime passes all compatibility checks (hash-recorded artifact); a known 48 kHz WAV was loaded through `emu.samples` and mounted after cold rebuild; live audio rendered 5.17 s, but peak PCM was 0. UI load reaches `LOAD 1/2 SAMPLES?`; firmware hooks show `FUN_400d1680` returns `FFFFFFFF` and `FUN_400ec6d2` receives an invalid-file reference despite the marked row. Remaining: resolve the browser-selection/reference handoff or use a pre-loaded project, then prove nonzero stock PCM, inject post-source/pre-filter, sweep filter/amp and retain a second stock track. 1.53 evidence only; 1.54 remains unproven. [Evidence](evidence/r03/stock-sample-attempt-2026-10-26.log). |
| R04 | Audit control-event hook ordering | READY | — | — | — | Identify final p-lock/LFO parameter values, trigger/retrigger, track identity and lock lifetime; timing trace or clearly marked source evidence; propose versioned semantic events |
| R05 | Benchmark TRX-B2 + resampling on desktop | READY | — | — | — | Native 44.1 kHz reference preserved, continuous 48 kHz adapter if needed; block-boundary, retrigger, duration/pitch and CPU evidence; no hardware performance claim |
| R06 | Prepare read-only descriptor capture tool | READY | — | — | — | Tool enumerates/dumps descriptors without changing streaming settings or claiming audio interfaces; no-device behavior tested; output usable by R02 |
| D01 | Decide one-voice audio-return architecture | BLOCKED | — | R01, R02, R03 | Receive path + descriptors + insertion evidence, or explicit negative research result for a documented no-go | Written go/no-go decision: proposed MAIN-to-track route, dry MAIN suppression, required firmware/controller changes, clocks and timing budget; no-go explicitly blocks implementation |
| I01 | Implement separate USB IN/OUT host layouts | DONE | codex-i01 @ 2026-10-26T18:46:33Z | R02 | — | [Report](docs/I01-directional-usb-layout.md); [replay patch](patches/sidecar-firmware/0001-directional-overbridge-layout.patch); `scripts/test_i01.sh` passes upstream block test, R02 raw-fixture policy test and decoded-endpoint regression. Rytm 12/12 preserved; Digitakt 12/368 IN, 2/88 OUT; invalid/non-block layouts refused. Circle kernel build and live stream remain H01. |
| H01 | Prove stock USB streaming on Mac/Pi | READY | — | I01 | — | Silence then quiet tone; bidirectional stats; measured rate/latency and ten minutes zero gaps/underruns; MAIN only is a transport diagnostic |
| I02 | Implement one-track receive-buffer routing | BLOCKED | — | D01 | D01 must conclude GO with a credible main-CPU route | Minimal emulator-verified mod; bounded buffer copy, no duplicate dry MAIN, underrun handling and stock-track regression evidence; candidate prepared for human hardware trial |
| H02 | Prove tone through real track pipeline | BLOCKED | — | H01, I02 | Validated firmware candidate/recovery procedure plus human hardware trial | Real filter/amp/send response; one-track external tone; measured latency/jitter and ten-minute gap/underrun report |
| I03 | Add integrated SRC + semantic control link | BLOCKED | — | R04, D01 | GO architecture and validated hook design | Eight SRC controls; trigger + lock values ordered; congestion/reconnect tested; internal SysEx allowed without dedicated MIDI sequencer tracks |
| I04 | Integrate external TRX-B2 voice | BLOCKED | — | R05, H02, I03 | Proven control and audio paths | One Digitakt audio track plays Pi TRX-B2 using SRC/locks/LFOs through local filter/amp/sends; note/velocity policy and timing evidence explicit |
| V01 | Persistence + dense-pattern verification | BLOCKED | — | I04 | Integrated voice works | Save/load, copy/paste, undo, mute, sound locks, restart/reconnect and dense modulation tests; regressions documented |
| X01 | Expand to two/eight voices | DEFERRED | — | V01 | Transport capacity decision; two source-described return channels do not support eight conventional independent streams | Capacity proposal and measured worst-case margin before implementation |
| X02 | Port proven architecture to Teensy | DEFERRED | — | V01 | Pi implementation and transport requirements known | Measured USB, resampling and DSP capacity; explicit supported voice count |

Next recommended assignment: the existing **R03 owner** should resume at the
[stock-load finding](docs/R03-emulator-insertion.md#2026-10-26-continuation-runtime-sample-fixture-and-stock-load-result).
The runtime now reruns here and the card fixture is valid, but the stock browser
hands `FUN_400ec6d2` an `FFFFFFFF` reference even when the UI reports samples
selected. Resolve that handoff (or use a pre-loaded project/snapshot) and prove
nonzero stock PCM before any injection. **H01** or **R04/R05** can proceed
independently. R01/R02/I01 are complete; the USB→ColdFire handoff remains
unresolved, so D01/I02 stay blocked.
