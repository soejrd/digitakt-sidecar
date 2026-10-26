# Audio return investigation — first pass

26 October 2026. Target confirmed by user: Digitakt mk1, OS 1.54, Raspberry Pi 4B. No firmware built/flashed and no test audio sent to hardware.

## Exact guide comparison

Fetched the [exact upstream guide](https://github.com/trigcondition/sidecar-expander/blob/main/src/content/docs/developers/digitakt.md) and compared it with [current upstream snapshot](DIGITAKT-GUIDE.md).

The substantive updates are public engine-source availability, working from tag `v0.2.0` rather than private branch `release-020`, a link to public `sidecar-mods`, and updated contribution instructions. The stock-only scope and unverified Digitakt claims remain. The local file has not been silently replaced. Our full-integration plan intentionally extends beyond that guide's scope, in a separate Digitakt firmware workstream.

## Reproducible source baseline

| Repository | Revision | Local state |
| --- | --- | --- |
| [sidecar-firmware](https://github.com/trigcondition/sidecar-firmware) | `6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457` | Local branch `digitakt`, based on fetched `v0.2.0` tag; no source edits |
| [sidecar-mods](https://github.com/trigcondition/sidecar-mods) | `c5a80c7fbf12caadfe1ab17b7db202a0f0674490` | Rytm integration reference; no source edits |

Ran `bash sidecar-firmware/tools/run_portable_tests.sh`. Native Pi portable tests, plugin support tests, Overbridge block-codec test and plugin ABI test passed. The sandboxed web run failed because localhost connections were denied (`EPERM`). Reran the unchanged suite with local networking enabled: **ALL PORTABLE TESTS PASS**, including **22/22 Node tests**.

Logs: [sandboxed](../evidence/baseline/sidecar-v0.2.0-portable.log), [successful complete run](../evidence/baseline/sidecar-v0.2.0-portable-unsandboxed.log). Full synthesis-core tests have not run: these require fetching the separate engine dependencies. These results are desktop baseline evidence, not USB or Pi hardware evidence.

## What the Rytm actually relies on

[The host codec](../deps/sidecar-firmware/teensy/libraries/RytmHost/ob_block.h) describes 12 channels in both directions, with eight dedicated voice-return destinations in host-to-Rytm channels 4–11. [Output::Pull](../deps/sidecar-firmware/pi/engine/output.cpp) maps the eight synth voices to those channels.

[The external-machine patch design](../deps/sidecar-mods/mods/0021-ext/design.md) reports that the existing Rytm mixer adds these USB inputs into each voice's digital layer before its analog processing. It uses a silent local source and reports trigs and modulated synthesis parameters over USB MIDI SysEx. Thus MIDI/SysEx is also used by the deeply integrated Rytm implementation; the crucial distinction is that patched audio-track machines generate these messages, rather than the player dedicating MIDI sequencer tracks to external instruments.

This is not principally a raw-button-forwarding mechanism. The separate remote-page implementation adds display/input features; synthesis integration depends on semantic voice events, modulated values and audio routing.

## Significant Digitakt difference

Read [Overwitch's device table](https://github.com/dagargo/overwitch/blob/master/res/devices.json) directly, without copying implementation code. At retrieval it describes Digitakt mk1 as:

| Property | Source-table value, not a measurement of this unit |
| --- | --- |
| Product ID | 2860 decimal / `0x0B2C` |
| Protocol | Version 2 |
| Host → Digitakt | **2 channels:** Main L Input, Main R Input, four bytes each |
| Digitakt → host | **12 channels:** Main L/R, Tracks 1–8, Input L/R, four bytes each |

Direction matters: the twelve channels recorded **from** Digitakt are not twelve independent return channels **into** it.

With the source-described 32-byte header and seven frames per block, the computed block sizes are **88 bytes host→Digitakt** and **368 bytes Digitakt→host**. This calculation is not a descriptor capture. OS 1.54 values still need physical confirmation; no device constants have been added to the host driver.

The Pi driver currently fixes both directions at 368 bytes in [ob_usb.cpp](../deps/sidecar-firmware/pi/engine/ob_usb.cpp). Its descriptor scan and buffers therefore need separate directional layouts before Digitakt streaming. The Teensy driver already separates directional sizes but still assumes Rytm interfaces/endpoints and a maximum of 12 channels; that is partial reuse, not Digitakt support.

### Consequences for the plan

One external mono voice could use an existing MAIN return channel **if** the Digitakt's main CPU can access it and redirect it into a track before processing. This remains a hypothesis: no receive-buffer location, timing, or working redirection has been established.

Under the current source-described layout, there are only two simultaneous independent mono return channels. Eight conventional independent mono voices cannot simply be mapped to Rytm-style destinations. Expanding channels, using another transport, or designing a different multiplexing scheme would require additional engineering and evidence. Do not infer that changing only a host-side channel count solves the controller/firmware path.

The immediate research priority is consequently **find the Digitakt MAIN USB return at the main CPU, establish its ownership/timing, and demonstrate one-track redirection without also retaining an unwanted dry MAIN copy**. Verify enough CPU margin and compatible sample representation before building the full machine UI.

## Hardware observation and next actions

The available macOS USB inventory currently reports no attached devices; the USB registry shows controllers but no attached Digitakt. This establishes lack of visibility in this session, not proof that the physical device is disconnected. No hardware descriptor capture is claimed.

Next independent work: audit the Digitakt OS 1.54 receive/MAIN-mix path using the existing local research and emulator, keeping findings separate from the Sidecar engine repository. Emulator injection can validate track processing but does not test the separate USB controller.

Next hardware action: connect the Digitakt to this Mac in Overbridge mode and capture device/configuration descriptors read-only. Then compare the observed directions with the source table before adapting the Mac test host. Do not run the unchanged Rytm host against Digitakt: it assumes the wrong output block size and voice mapping.

## Handoff provenance

This report describes the preceding exploration session. The current snapshot of the exact upstream guide is DIGITAKT-GUIDE.md. Historical logs were copied and personal workspace paths were redacted; no new baseline test run is implied. Current status is maintained in ../TASKBOARD.md.
