# Task board

Status/ownership live here. Claim READY work with an owner and UTC timestamp; coordinate claims across separate clones. BLOCKED work needs its stated dependency or release condition. DONE requires linked evidence, not a plausible implementation.

## Ready

| ID | Work | Depends on | Owner | Acceptance |
| --- | --- | --- | --- | --- |
| R04 | Audit control-event ordering | — | — | Locate final lock/LFO values, trigger/retrigger, track identity and lock lifetime; record source/trace evidence and propose ordered semantic events. |
| R05 | Benchmark TRX-B2 + resampling | — | codex @ 2026-10-27T10:55:04Z | Preserve 44.1 kHz reference; test continuous conversion, block boundaries, retriggers, duration/pitch and CPU. No hardware performance claim. |

## Waiting

| ID | Work | Status | Release condition / dependencies | Acceptance |
| --- | --- | --- | --- | --- |
| H02 | Real track pipeline | BLOCKED | Agreed human trial of I02 candidate/recovery (H01 passed); silence until USB1 render activation | External tone responds to track filter/amp/sends; timing measured; ten-minute zero-gap/underrun run. |
| I03 | SRC + control link | BLOCKED | R04 (D01 conditional GO recorded) | Eight SRC controls; ordered trigger/lock values; silence-until-active handshake, congestion and reconnect tests; no dedicated MIDI sequencer track required. |
| I04 | External TRX-B2 voice | BLOCKED | R05, H02, I03 | One audio track controls Pi synthesis through SRC/locks/LFOs and local processing; explicit note/velocity policy and timing evidence. |
| V01 | Persistence and stress | BLOCKED | I04 | Save/load, copy/paste, undo, mutes, sound locks, restart/reconnect and dense modulation regressions. |
| X01 | More voices | DEFERRED | V01 + transport capacity decision | Establish independent-channel capacity and measured worst-case margin before scaling. |
| X02 | Teensy port | DEFERRED | V01 + proven architecture | Measure USB, conversion and DSP capacity; state supported voice count. |

## Completed

| ID | Result | Evidence |
| --- | --- | --- |
| H01 | Pi Linux tone/loopback and Circle both pass four-minute physical streams; Circle FIFO under 0; 48 kHz, 12/2 framing and Linux round-trip measured | [Report](docs/H01-stock-usb-stream.md), [Circle](evidence/h01/circle-stability.json), [Linux](evidence/h01/linux-stability.json), [build](evidence/h01/circle-build.json) |
| I02 | Compiled USB1 prototype boots on 1.54; 13 emulator checks pass, candidate/recovery prepared; hardware timing/reconnect remain H01/H02 | [Report](docs/I02-one-track-routing.md), [results](evidence/i02/routing-154.json), [build](evidence/i02/build.json) |
| D01 | Conditional GO for one-track native USB copy/clear; I02 released, hardware qualification pending | [Decision](docs/adr/0001-usb-return-to-track.md) |
| R01 | OS 1.54 source/render trace; USB ownership assumptions superseded by R07 | [Report](docs/R01-usb-main-trace.md), [facts](evidence/r01/os154-static-facts.json) |
| R02 | Physical endpoint capture and read-only inspector | [Report](docs/R02-usb-descriptors.md), [raw](evidence/r02-usb-descriptors-2026-10-26.txt), [decoded](evidence/r02-endpoints-2026-10-26.json) |
| I01 | Separate USB directional layouts; host tests pass | [Report](docs/I01-directional-usb-layout.md), [patch](patches/sidecar-firmware/0001-directional-overbridge-layout.patch), `scripts/test_i01.sh` |
| R07 | OS 1.54 native USB receive/boot path and dry suppression; focused execution passes | [Report](docs/R07-usb-coldfire-handoff.md), [anchors](evidence/r07/handoff-facts.json), [probe results](evidence/r07/receive-154.json) |
| R03 | Nine emulator assertions pass on OS 1.53 | [Reproduction](docs/R03-REPRODUCTION.md), [results](evidence/r03/insertion-153.json), [probe](tools/r03_probe.py) |

R06's inspector work is consolidated into R02. OS 1.54 candidate boot and track processing now pass emulator checks. Stock live streaming is qualified by H01. Full integrated timing and physical USB-to-track routing remain unproven.
