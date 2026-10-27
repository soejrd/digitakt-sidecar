# Progress handoff

Current status/ownership: [TASKBOARD.md](TASKBOARD.md). Dependency pins: sources.json. Earlier sessions are preserved in Git history.

## Established

- Target: Digitakt mk1 OS 1.54, Pi 4B Rev 1.5 (4 GB); Teensy later.
- **R01/R07:** native ColdFire USB receive path, consumed/current half selection and dry-MAIN suppression traced. R07 focused OS 1.54 receive/mix tests pass. The old section-8-only/DSPI handoff inference is withdrawn. [R07 report](docs/R07-usb-coldfire-handoff.md).
- **R02/I01:** physical descriptors are alt-3 368-byte IN / 88-byte OUT. Directional host patch and regression tests pass. H01 found/fixed the static capture callback's instance-member compile error; apply patches 0001 then 0002. [I01](docs/I01-directional-usb-layout.md).
- **R03:** nine OS 1.53 emulator insertion/filter/amp/stock-track assertions pass with the source-built patched Unicorn. [Reproduction](docs/R03-REPRODUCTION.md).
- **D01/I02:** conditional GO; compiled one-track USB1 candidate boots on OS 1.54 and passes 13 full-render/function checks, including filter/amp/sends, both halves, dry suppression and guarded transitions. [Candidate/recovery and limits](docs/I02-one-track-routing.md). No Digitakt firmware flashed. USB must stay silent until render activation; real acknowledgement is I03 work.
- **H01 DONE:** physical Pi Linux and Circle streams each pass four minutes. Circle: zero USB gaps/errors and zero FIFO underruns. Linux: 48 kHz-consistent cadence, both return channels confirmed by analog loopback, all 478 pulses returned; host-observed round trip median 5.724 ms, max 6.029 ms. This is Linux/USB/analog-loop timing, not integrated-machine or Circle latency. [Report](docs/H01-stock-usb-stream.md), [Circle evidence](evidence/h01/circle-stability.json), [Linux evidence](evidence/h01/linux-stability.json).

## Hardware state and next work

The Pi is running the patched Circle host from the user-authorized spare SD, with wired console at `http://sidecar.local/`. The normal Linux card is kept separately; its boot files were not replaced. Digitakt remains stock 1.54. During testing the analog cable was moved to MAIN OUT R → INPUT L; keep input monitoring off while looped.

H02 now needs an agreed human trial of the I02 candidate/recovery procedure. Full integrated latency/jitter, render headroom and real reconnect detection remain unqualified. R04 (semantic controls) and R05 (TRX-B2/resampling) are READY independently. Set acceptable musical timing from the integrated trial before timing GO or scaling. Original portable Sidecar baseline and I01 host regressions pass; full synthesis dependency tests were not run.
