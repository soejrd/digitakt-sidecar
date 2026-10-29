# Task board

Status/ownership live here. Claim READY work with an owner and UTC timestamp; coordinate claims across separate clones. BLOCKED work needs its stated dependency or release condition. DONE requires linked evidence, not a plausible implementation.

## Ready

| ID | Work | Status | Dependencies / first action | Acceptance |
| --- | --- | --- | --- | --- |
| V01-B | Persistence and edit semantics (Gate B) | READY — Codex (2026-10-29T21:30:00Z) | [Runbook](docs/V01-gate-b-c-runbook.md). Run on **I04R**, not V01K: B needs a stable device, not dense modulation. Disposable project only. | Before/action/after restored state and output per case in [Gate B](docs/V01-persistence-stress.md#gate-b--native-persistence-and-edit-semantics), including one verification without the Pi. |
| V01-C | Recovery and soak (Gate C) | READY — Codex (2026-10-29T21:30:00Z) | [Runbook](docs/V01-gate-b-c-runbook.md). Supported workload only (slow/shallow LFO); the dense profile stays parked under V01-A. Repeats land after B so the image is not swapped twice. | Three cycles each for pull/replug, Pi reboot, Digitakt restart; one soak; zero new faults/underruns; UI thresholds measured. |
| V01-A | Dense-modulation freeze (Gate A) | PARKED — Codex (2026-10-29T21:30:00Z) | Paused by owner to unblock B/C and X01. Resume condition: RX/ACK hypothesis reproduced or refuted in the emulator **and** the deferral regression bounded. No flash before that. [State](docs/V01-persistence-stress.md#current-next-work--offline-only-no-flash). | [Gate A](docs/V01-persistence-stress.md#gate-a--responsive-dense-modulation). |
| X01a | Independent-channel capacity study | READY (desk work only) — Codex (2026-10-29T21:30:00Z) | Does not depend on the V01-A freeze. Derive headroom from [R02 descriptors](evidence/r02-endpoints-2026-10-26.json) and [H01 measured rate](evidence/h01/linux-stability.json); the prototype consumes the whole stereo return for one mono track. No hardware change. | Establish independent-channel capacity and measured worst-case margin before scaling. |

## Waiting

| ID | Work | Status | Release condition / dependencies | Acceptance |
| --- | --- | --- | --- | --- |
| X01 | More voices | DEFERRED | V01-A (dense modulation) + X01a capacity result + transport capacity decision | Establish independent-channel capacity and measured worst-case margin before scaling. |
| X02 | Teensy port | DEFERRED | V01 + proven architecture | Measure USB, conversion and DSP capacity; state supported voice count. |

## Completed

| ID | Result | Evidence |
| --- | --- | --- |
| I04 | DONE by user acceptance of one-voice prototype: SRC/locks/shallow LFO/local processing/note/velocity and musical timing pass. Native-click→analog median 11.417 ms (old 115.833 ms); 19-hit spread 0.729 ms. Fast/deep LFO failure explicitly deferred to V01; not a full stress pass | [Report](docs/I04-external-trx-voice.md), [acceptance](evidence/i04/physical-acceptance.json), [timing](evidence/i04/event-worker-physical-timing.json), [V01 issue](evidence/i04/v01-dense-lfo-handoff.json) |
| H02 | DONE by user acceptance: physical USB1 tone responds to filter, AMP/decay, overdrive and both sends; neighboring track remains audible. 506.758 s with no new USB gaps/errors or FIFO underruns. User accepted shortened run; timing remains unmeasured | [Report](docs/H02-real-track-pipeline.md), [routing](evidence/h02/physical-routing.json), [stability](evidence/h02/physical-stability.json), [timing limitation](evidence/h02/timing.json) |
| I03 | Eight native SRC controls, ordered START/final lock/LFO values, native USB-MIDI worker, matching-session activation gate and Circle integration pass host/compiled-emulator checks; congestion and reconnect recover. Pi 4 kernel builds. No hardware claim | [Report](docs/I03-src-control-link.md), [compiled exchange](evidence/i03/integrated-154.json), [dynamic ordering](evidence/i03/dynamic-154.json), [integration/build](evidence/i03/integration.json) |
| R05 | Real TRX-B2 stays at 44.1 kHz; exact 160/147 streaming conversion passes continuous/ragged boundary invariance, retrigger, pitch endpoints, 60 s drift and host CPU checks. No Pi/hardware claim; 22 kHz rejection is −26.3 dB | [Report](docs/R05-engine-resampling.md), [results](evidence/r05/benchmark.json), `scripts/test_r05.sh` |
| R04 | OS 1.54 static order established: sequencer stored locks → engine sound/lock apply/restore → engine/LFO → render final params, note, velocity and voice-start bit; ordered event contract proposed. Dynamic timing and distinct retrigger behavior remain unmeasured | [Report](docs/R04-control-ordering.md), [static results](evidence/r04/ordering-154.json), [probe](tools/r04_probe.py) |
| H01 | Pi Linux tone/loopback and Circle both pass four-minute physical streams; Circle FIFO under 0; 48 kHz, 12/2 framing and Linux round-trip measured | [Report](docs/H01-stock-usb-stream.md), [Circle](evidence/h01/circle-stability.json), [Linux](evidence/h01/linux-stability.json), [build](evidence/h01/circle-build.json) |
| I02 | Compiled USB1 prototype boots on 1.54; 13 emulator checks pass, candidate/recovery prepared; hardware timing/reconnect remain H01/H02 | [Report](docs/I02-one-track-routing.md), [results](evidence/i02/routing-154.json), [build](evidence/i02/build.json) |
| D01 | Conditional GO for one-track native USB copy/clear; I02 released, hardware qualification pending | [Decision](docs/adr/0001-usb-return-to-track.md) |
| R01 | OS 1.54 source/render trace; USB ownership assumptions superseded by R07 | [Report](docs/R01-usb-main-trace.md), [facts](evidence/r01/os154-static-facts.json) |
| R02 | Physical endpoint capture and read-only inspector | [Report](docs/R02-usb-descriptors.md), [raw](evidence/r02-usb-descriptors-2026-10-26.txt), [decoded](evidence/r02-endpoints-2026-10-26.json) |
| I01 | Separate USB directional layouts; host tests pass | [Report](docs/I01-directional-usb-layout.md), [patch](patches/sidecar-firmware/0001-directional-overbridge-layout.patch), `scripts/test_i01.sh` |
| R07 | OS 1.54 native USB receive/boot path and dry suppression; focused execution passes | [Report](docs/R07-usb-coldfire-handoff.md), [anchors](evidence/r07/handoff-facts.json), [probe results](evidence/r07/receive-154.json) |
| R03 | Nine emulator assertions pass on OS 1.53 | [Reproduction](docs/R03-REPRODUCTION.md), [results](evidence/r03/insertion-153.json), [probe](tools/r03_probe.py) |

R06's inspector work is consolidated into R02. OS 1.54 candidate boot and track processing now pass emulator checks. Stock live streaming is qualified by H01. H02 physically proves one-track routing under the user-accepted scope. I04 now measures relative integrated timing and observed Pi margin within the user-accepted prototype scope; dense modulation and persistence/reconnect stress remain V01.
