# Progress handoff

Current status/ownership: [TASKBOARD.md](TASKBOARD.md). Dependency pins: sources.json. Earlier sessions are preserved in Git history.

## Established

- Target: Digitakt mk1 OS 1.54, Pi 4B Rev 1.5 (4 GB); Teensy later.
- **R01/R07:** native ColdFire USB receive path, consumed/current half selection and dry-MAIN suppression traced. R07 focused OS 1.54 receive/mix tests pass. The old section-8-only/DSPI handoff inference is withdrawn. [R07 report](docs/R07-usb-coldfire-handoff.md).
- **R02/I01:** physical descriptors are alt-3 368-byte IN / 88-byte OUT. Directional host patch and regression tests pass. H01 found/fixed the static capture callback's instance-member compile error; apply patches 0001 then 0002. [I01](docs/I01-directional-usb-layout.md).
- **R03:** nine OS 1.53 emulator insertion/filter/amp/stock-track assertions pass with the source-built patched Unicorn. [Reproduction](docs/R03-REPRODUCTION.md).
- **D01/I02:** conditional GO; compiled one-track USB1 candidate boots on OS 1.54 and passes 13 full-render/function checks, including filter/amp/sends, both halves, dry suppression and guarded transitions. [Candidate/recovery and limits](docs/I02-one-track-routing.md). H02 now verifies physical USB1 activation/routing with the corrected H02F candidate. USB must stay silent until render activation.
- **H01 DONE:** physical Pi Linux and Circle streams each pass four minutes. Circle: zero USB gaps/errors and zero FIFO underruns. Linux: 48 kHz-consistent cadence, both return channels confirmed by analog loopback, all 478 pulses returned; host-observed round trip median 5.724 ms, max 6.029 ms. This is Linux/USB/analog-loop timing, not integrated-machine or Circle latency. [Report](docs/H01-stock-usb-stream.md), [Circle evidence](evidence/h01/circle-stability.json), [Linux evidence](evidence/h01/linux-stability.json).
- **R04 DONE (static):** OS 1.54 sequencing reaches stored locks before engine apply/restore, then engine/LFO processing, then the render hook with final A-H, note, velocity and `fw_voice_start`. Ordered I03 event contract is in the report. I03 now supplies dynamic render lock/LFO evidence; hardware timing and distinct retrigger semantics remain unmeasured. [Report](docs/R04-control-ordering.md), [results](evidence/r04/ordering-154.json).

## Hardware state and next work

- **H02 DONE by explicit user acceptance.** H02F and the corrected Circle kernel are installed and physically booted. The session-gated tone passes human listening checks for filter, AMP volume/decay, overdrive, reverb, delay and an unaffected neighboring internal track. [Report](docs/H02-real-track-pipeline.md).
- **Stability:** 506.758 seconds across 101 successful reads; no new USB gaps/errors or FIFO underruns. FIFO counter stayed at its pre-run value of 10. The human powered Pi off and accepted this shortened run instead of ten minutes; later HTTP timeouts were shutdown. [Evidence](evidence/h02/physical-stability.json).
- **Limits:** the captured Pi source has twelve pulses, but USB MAIN has no matching pulse train, so no latency number is claimed. Full Digitakt render headroom and physical reconnect stress remain unqualified. User ended further tests. [Timing attempt](evidence/h02/timing.json).
- **Current hardware:** Pi off, ending the runtime tone. One Sidecar SD card; settings/credentials preserved. Previous H02 kernel backup: card `kernel8-rpi4.before-clock.img`, local `private/h02/sd-backup-clock/`; original H01 backup remains. H02F private firmware: `private/h02/trial/Digitakt-H02F-mode6.syx`. No computer UI access per user instruction.
- **Fixes:** physical Overbridge is USB mode 6, not the mode 2 forced by old emulator fixtures. Circle now uses an independent sample-based lease clock, not resettable statistics. Regression, compiled exchange, Pi build and five-patch replay pass. [Fix evidence](evidence/h02/mode6-fix.json).

**I04 host prototype checked; physical acceptance BLOCKED.** Real pinned TRX-B2 now renders through the Circle session gate at native 44.1 kHz with persistent 160/147 conversion. PTCH owns pitch; note is ignored; host leaves velocity to local AMP. ASan/UBSan engine and production-glue checks and six-patch replay pass; the Pi 4 Circle kernel cross-build succeeds. [Report](docs/I04-external-trx-voice.md), [evidence](evidence/i04/host.json).

**Next:** qualify timestamped playout and filter quality, then arrange a human hardware session with lowered speakers, silence-first/faded audio and recoverable private candidate. The prototype applies snapshots on arrival; burst STARTs can coalesce before rendering. Integrated timing, Pi worst-case margin and physical TRX-B2 SRC/lock/LFO/local processing remain unmeasured. R05's −26.3 dB stopband result is unchanged. No other independent READY task; V01 still depends on I04. No deployment or physical audio occurred in this session.

Earlier troubleshooting is retained in Git and linked evidence. Unrelated user PLAN.md edits remain local and excluded from task commits.
