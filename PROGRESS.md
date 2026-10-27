# Progress handoff

Current task status and ownership: [TASKBOARD.md](TASKBOARD.md). Earlier sessions and superseded troubleshooting are preserved in Git history.

## Established

- Target confirmed: Digitakt mk1 OS 1.54, Raspberry Pi 4B; Teensy available for later evaluation. Sources are pinned in sources.json.
- **R01:** 1.54 ColdFire source/render addresses traced. The original controller-ownership assumption is superseded by R07’s native USB findings. [Report](docs/R01-usb-main-trace.md), [static evidence](evidence/r01/os154-static-facts.json).
- **R02:** read-only physical descriptors captured outside the sandbox. Alt 3: 368-byte IN / 88-byte OUT. Consistent with source-described 12/2 channels; actual stream format/rate unmeasured. Error 0xE00002BE was NoResources, not exclusive access; no driver changes were needed. [Report](docs/R02-usb-descriptors.md).
- **I01:** directional host layouts implemented as a replayable patch. Host regression tests pass; no Circle kernel build or live stream yet. [Report](docs/I01-directional-usb-layout.md).
- **R03:** fresh 1.53 fixture and source-built patched Unicorn pass nine assertions: stock playback, 3 kHz insertion before track processing, >70 dB filter attenuation, amp mute, zero-source replacement and byte-identical second-track regression. [Reproduction](docs/R03-REPRODUCTION.md), [results](evidence/r03/insertion-153.json), [runtime provenance](evidence/r03/runtime-build.txt). This is emulator evidence, not 1.54/hardware qualification.
- Original Sidecar portable baseline passed, including 22 Node tests; localhost tests require execution access outside the restrictive sandbox. [Baseline log](evidence/baseline/sidecar-v0.2.0-portable-unsandboxed.log). Full synthesis-core dependency tests were not run.

## R07 takeover

Native OS 1.54 USB receive path identified: EP3 OUT descriptors → `0x400035F4` → stereo halves `0x80001F60/0x80002060` → stock MAIN mix. Synthetic function-level execution passes empty/partial queues, two-block packet continuity, bounds and copy/clear dry suppression while retaining internal audio. [Report](docs/R07-usb-coldfire-handoff.md), [execution](evidence/r07/receive-154.json), [anchors](evidence/r07/handoff-facts.json).

The inherited DSPI-only/no-DMA verdict was unsupported and is withdrawn. Boot dispatch to the native USB initializer is traced; R07 is complete at the static/function-emulator level. Section 8’s role on the physical unit remains unknown and is not an evidenced prerequisite. No live USB audio or firmware flash occurred. The patched runtime works outside the restrictive sandbox; R03 was not rerun.

## I02 result and next work

**I02 DONE at the emulator prototype level:** compiled USB1 machine, linked through core/machine-pages, boots on OS 1.54 and passes 13 acceptance checks: both halves/order, direct signed-word copy, >70 dB filter attenuation, exact AMP/dry silence, zero replacement, reverb send tail, unchanged neighboring source buffers and guarded lifecycle paths. Normal copy is 312 guest instructions; maximum tested flush is 515, excluding SDK overhead. [Report/recovery](docs/I02-one-track-routing.md), [results](evidence/i02/routing-154.json), [build](evidence/i02/build.json).

[D01's conditional GO](docs/adr/0001-usb-return-to-track.md) remains conditional on hardware qualification. The local I02A candidate is private; no flash or physical audio test occurred. USB must remain silent until the renderer activates USB1; the real acknowledgement belongs to I03. Synthetic mode/sync transitions do not qualify physical reconnect detection. Source-buffer regression is exact; complete other-track output PCM identity is not claimed.

H01 (physical stream), R04 (control hooks) and R05 (engine/resampling) remain READY. H02 now waits on H01 and an agreed human trial of the prepared candidate/recovery. Hardware clock/rate, full latency/jitter and total render headroom remain unmeasured. Set the musical timing tolerance from the trial before timing qualification.
