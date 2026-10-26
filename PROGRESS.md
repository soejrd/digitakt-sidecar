# Progress handoff

Current task status and ownership: [TASKBOARD.md](TASKBOARD.md). Earlier sessions and superseded troubleshooting are preserved in Git history.

## Established

- Target confirmed: Digitakt mk1 OS 1.54, Raspberry Pi 4B; Teensy available for later evaluation. Sources are pinned in sources.json.
- **R01:** 1.54 ColdFire source/render addresses traced. Exact USB-controller → ColdFire PCM handoff remains unresolved. [Report](docs/R01-usb-main-trace.md), [static evidence](evidence/r01/os154-static-facts.json).
- **R02:** read-only physical descriptors captured outside the sandbox. Alt 3: 368-byte IN / 88-byte OUT. Consistent with source-described 12/2 channels; actual stream format/rate unmeasured. Error 0xE00002BE was NoResources, not exclusive access; no driver changes were needed. [Report](docs/R02-usb-descriptors.md).
- **I01:** directional host layouts implemented as a replayable patch. Host regression tests pass; no Circle kernel build or live stream yet. [Report](docs/I01-directional-usb-layout.md).
- **R03:** fresh 1.53 fixture and source-built patched Unicorn pass nine assertions: stock playback, 3 kHz insertion before track processing, >70 dB filter attenuation, amp mute, zero-source replacement and byte-identical second-track regression. [Reproduction](docs/R03-REPRODUCTION.md), [results](evidence/r03/insertion-153.json), [runtime provenance](evidence/r03/runtime-build.txt). This is emulator evidence, not 1.54/hardware qualification.
- Original Sidecar portable baseline passed, including 22 Node tests; localhost tests require execution access outside the restrictive sandbox. [Baseline log](evidence/baseline/sidecar-v0.2.0-portable-unsandboxed.log). Full synthesis-core dependency tests were not run.

## Next

R07 must establish the actual USB PCM handoff and dry-MAIN suppression before D01 can approve a routing architecture. H01 (physical stream), R04 (control hooks) and R05 (engine/resampling) can proceed independently.

## Latest maintenance

Removed superseded reports, failed-attempt logs, a copied upstream guide, duplicate bootstrap and generated R03 scratch/card assets. Kept final evidence, the source-built private runtime, useful references and all implementation/tests. R06 was redundant with the delivered R02 inspector and is folded into R02. See the cleanup commit for the removed history.

Cleanup checks: local links resolve, I01 tests pass, saved R03 assertions remain intact and patch replay paths validate. Generated fixture removed; rebuild using the R03 guide.
