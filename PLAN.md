# Integrated sidecar plan

## Goal

Digitakt mk1 OS 1.54 controls an external TRX-B2 engine on a Raspberry Pi 4B: machine selection and eight SRC controls on an existing audio track, sequencer locks/LFOs, and returned audio through that track's filter, amp and effects sends. First prove one voice. More sequencer tracks are outside scope; Teensy is a later target.

```text
Digitakt machine/voice events → Pi synthesis → USB return → pre-filter track buffer
```

The creator's [Digitakt guide](https://github.com/trigcondition/sidecar-expander/blob/main/src/content/docs/developers/digitakt.md) covers a stock-firmware MIDI/MAIN-return experiment. It is a transport reference; our integrated-machine goal requires a separate firmware workstream. Rytm has dedicated voice returns; the Digitakt capture supports the source-described stereo return, so eight independent streams must not be assumed.

## Gates

| Stage | Required result | Tasks |
| --- | --- | --- |
| Establish feasibility | Physical USB descriptors, identified receive path, proven pre-filter insertion, explicit go/no-go | R01–R03, R07, D01 |
| Prove transport | Separate IN/OUT host layouts; 4-minute stock stream with zero gaps/underruns and measured timing | I01, H01 |
| Prove track routing | Ready PCM copied into one track, original dry MAIN copy suppressed; actual filter/amp/send behavior and timing measured | I02, H02 |
| Integrate controls | SRC A–H, ordered triggers/retriggers, final lock/LFO values and versioned control protocol | R04, I03 |
| Play TRX-B2 | Native-rate synthesis + resampling, explicit note/velocity policy, one integrated external voice | R05, I04 |
| Persist and qualify | Save/load, copy/paste, undo, mutes, sound locks, reconnect and dense-pattern regressions | V01 |
| Expand | Demonstrated transport capacity and CPU headroom before more voices  | X01, X02 |

D01 records a [conditional GO for native USB copy/clear](docs/adr/0001-usb-return-to-track.md). I02 passes OS 1.54 emulator checks, [H01 qualifies stock physical streaming](docs/H01-stock-usb-stream.md), and [H02 is complete under user-accepted physical routing/stability scope](docs/H02-real-track-pipeline.md); integrated timing remains unmeasured. The prototype consumes the whole stereo USB return to feed one mono track; additional independent returns are not assumed.

## Expansion feasibility (X01a desk study)

The current 48 kHz return carries two sample lanes. Two external mono tracks
are a plausible next implementation using left/right separately; eight Pi
engines mixed into those lanes are a different feature from eight independent
Digitakt track returns. Eight full-rate independent returns require a new or
modified transport, not just more engine instances. See the
[capacity study](docs/X01a-multivoice-capacity.md) and
[derived evidence](evidence/x01a/capacity.json). Pi one-voice observations suggest
headroom, but multivoice worst-case margin is unmeasured. Offline benchmarking
and protocol/routing design can proceed while V01-A is parked; X01 hardware
expansion remains deferred under the taskboard's gates.

The [first X01 offline milestone](docs/X01-multivoice-offline.md) now passes:
1/2/8 real host engines, track-aware playback, and compiled two-lane insertion
through native OS1.54 AMP/filter processing. Actual multitrack capture/worker/
handshake integration and Pi/device margin are still open; the production path
remains one voice.

## Design requirements

- Control semantic voice events and final modulated values, rather than forwarding every button. Keep normal navigation/filter/amp controls local. USB MIDI/SysEx may carry internal events without dedicating MIDI sequencer tracks.
- Timestamp and order events against the device clock. A remote voice cannot return audio before its trigger arrives: investigate sequencer lookahead or fixed compensation and measure live-key latency separately.
- Use bounded queues, sequence/generation counters and explicit underrun/reconnect behavior. The audio callback copies ready PCM; it never waits for a host response.
- TRX-B2 controls: PTCH, DEC, RAMP, HOLD, TICK, NOIS, DIRT, DIST. Preserve 44.1 kHz synthesis; 48 kHz output requires a 160/147 conversion ratio. Define pitch mapping and apply velocity gain once.
- Measure latency/jitter, buffer occupancy and CPU margin. Set acceptable timing limits from the one-voice experiment before scaling.
- Stop or revise the architecture if no practical MAIN-to-track route exists. Changing a host channel count alone does not add device/controller capacity.

Current status and exact acceptance criteria: [TASKBOARD.md](TASKBOARD.md). Findings and commands: [PROGRESS.md](PROGRESS.md). External sources: [docs/REFERENCES.md](docs/REFERENCES.md).
