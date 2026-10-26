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
