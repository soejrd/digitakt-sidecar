# Progress handoff

Current status/ownership: [TASKBOARD.md](TASKBOARD.md). Dependency pins: sources.json. Earlier sessions are preserved in Git history.

## Established

- Target: Digitakt mk1 OS 1.54, Pi 4B Rev 1.5 (4 GB); Teensy later.
- **R01/R07:** native ColdFire USB receive path, consumed/current half selection and dry-MAIN suppression traced. R07 focused OS 1.54 receive/mix tests pass. The old section-8-only/DSPI handoff inference is withdrawn. [R07 report](docs/R07-usb-coldfire-handoff.md).
- **R02/I01:** physical descriptors are alt-3 368-byte IN / 88-byte OUT. Directional host patch and regression tests pass. H01 found/fixed the static capture callback's instance-member compile error; apply patches 0001 then 0002. [I01](docs/I01-directional-usb-layout.md).
- **R03:** nine OS 1.53 emulator insertion/filter/amp/stock-track assertions pass with the source-built patched Unicorn. [Reproduction](docs/R03-REPRODUCTION.md).
- **D01/I02:** conditional GO; compiled one-track USB1 candidate boots on OS 1.54 and passes 13 full-render/function checks, including filter/amp/sends, both halves, dry suppression and guarded transitions. [Candidate/recovery and limits](docs/I02-one-track-routing.md). No Digitakt firmware flashed. USB must stay silent until render activation; the I03 emulator now verifies matching activation; physical acknowledgement delivery remains H02.
- **H01 DONE:** physical Pi Linux and Circle streams each pass four minutes. Circle: zero USB gaps/errors and zero FIFO underruns. Linux: 48 kHz-consistent cadence, both return channels confirmed by analog loopback, all 478 pulses returned; host-observed round trip median 5.724 ms, max 6.029 ms. This is Linux/USB/analog-loop timing, not integrated-machine or Circle latency. [Report](docs/H01-stock-usb-stream.md), [Circle evidence](evidence/h01/circle-stability.json), [Linux evidence](evidence/h01/linux-stability.json).
- **R04 DONE (static):** OS 1.54 sequencing reaches stored locks before engine apply/restore, then engine/LFO processing, then the render hook with final A-H, note, velocity and `fw_voice_start`. Ordered I03 event contract is in the report. I03 now supplies dynamic render lock/LFO evidence; hardware timing and distinct retrigger semantics remain unmeasured. [Report](docs/R04-control-ordering.md), [results](evidence/r04/ordering-154.json).

## Hardware state and next work

- **One Sidecar/Circle SD card.** H02 test kernel installed, read-back verified and card safely ejected. Wi-Fi configuration, credentials and all other existing files are unchanged. Card backup: `kernel8-rpi4.before-h02.img`; local backup: `private/h02/sd-backup/`. [Deployment evidence](evidence/h02/sd-deployment.json).
- **Next:** put the card back in the Pi, leave Digitakt on/USB connected, boot and verify the new build and packet progress. Digitakt remains stock 1.54. Agree the pinned I03P candidate and stock recovery setup before the human firmware install. Headphone volume down; remove analog loopback for listening, input monitoring off.
- **H02 remains open:** gated 3 kHz Circle test tone passes actual-glue ASan/UBSan and Pi 4 cross-build; defaults off, explicit enable after handshake, 200 ms fade, <= −42 dBFS on MAIN, disarms on loss. Physical filter/amp/send routing, timing/headroom, reconnect and ten-minute stability are unqualified. [Report](docs/H02-real-track-pipeline.md).
- **Transport finding:** earlier H01 host stalled with a stale streaming flag; Digitakt restart did not recover, Pi reboot did. Subsequent 15-second check passed with zero gaps/errors/underruns. No root-cause or automatic-reconnect claim.

**R05 DONE at host-benchmark level:** real 44.1 kHz TRX-B2 + exact 160/147 converter passes block-boundary invariance, retrigger, pitch endpoints and 60 s drift. Host CPU ≈800× realtime; 22 kHz rejection −26.3 dB. No Pi/headroom or integrated-latency claim. [Report](docs/R05-engine-resampling.md).

**I03 DONE at host/compiled-emulator level:** native SRC controls, ordered START/lock/LFO snapshots, USB-MIDI worker and session/ACK gate pass compiled exchange and congestion/reconnect checks. Pi 4 build passes; H02 now deploys that integration with patch 0004's diagnostic tone. Private firmware candidate: `private/i03-release3`, label I03P, source/hash-pinned and not flashed. [Report](docs/I03-src-control-link.md). Dynamic fixtures do not qualify panel lock editing or distinct retrigger semantics.

I04 still depends on H02. User-owned PLAN.md edits remain excluded from task commits.
