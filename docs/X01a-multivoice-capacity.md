# X01a — Independent return capacity

Desk study, 2026-10-29. Target: Digitakt mk1 OS 1.54 + Pi 4B. No hardware
operation, firmware change or new performance measurement was performed.
Subsequent [X01 offline implementation](X01-multivoice-offline.md) proves the
host engine bank and compiled two-lane native insertion in the emulator;
physical worst-case margin and complete multitrack control remain open.
Repository source/evidence baseline: `6a269df3290a173231eef23e8ee9ba02fa01ccab`;
links below are repository-relative and refer to that baseline unless stated.
Dependency bases are recorded in [sources.json](../sources.json): Sidecar
firmware `6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457`, digi emulation
`f57c2f237e84b8ff91bc3adb2a2931bccf9928e9`. Local production changes are
delivered as [replayable patches](../patches/sidecar-firmware/), not inferred
from the upstream base alone.

## Decision

**The proven implementation supports one independent mono external track.
The existing conventional PCM transport can carry two independent mono
streams at 48 kHz, or one stereo stream. Two-track routing is a feasible
implementation candidate, not a measured two-track result. Eight independent
full-rate conventional PCM returns do not fit this transport.** The number of
simultaneously synthesized voices mixed into one return is a different quantity:
it does not preserve separate Digitakt filter/amp/send processing per voice.
The current prototype reserves both return lanes, so its unused right lane is
not currently available as a second external machine.
Sources: [physical endpoints](../evidence/r02-endpoints-2026-10-26.json),
[measured stream](../evidence/h01/linux-stability.json),
[native receive facts](../evidence/r07/handoff-facts.json),
[current copy/clear implementation](../mods/usb-track/usb_track.c), and
[one-voice acceptance](../evidence/i04/physical-acceptance.json).

**Worst-case scaling margin remains unmeasured.** X01a establishes the
transport ceiling and identifies the missing qualification; it does not satisfy
the task's entire acceptance condition (“measured worst-case margin before
scaling”). X01 should remain deferred pending a transport decision, the relevant
V01 gates, and workload qualification. [Acceptance and dependencies](../TASKBOARD.md),
[V01 failure/parked investigation](V01-persistence-stress.md).

## What is measured and what is derived

| Fact | Evidence class and source |
| --- | --- |
| Alt 3: device→host EP `0x83`, 368 B; host→device EP `0x03`, 88 B. OUT alt 2 is also 88 B; alt 1 is 32 B. | Physical descriptor capture: [R02 JSON](../evidence/r02-endpoints-2026-10-26.json), [raw capture](../evidence/r02-usb-descriptors-2026-10-26.txt). Alternate settings are choices for an interface, not additive return endpoints. |
| Seven frames per packet, 32 B header, two big-endian 32-bit sample words per return frame; 47,999.484785 frames/s over 240.000119 s. | Hardware stream: [H01 Linux](../evidence/h01/linux-stability.json), [report and format/channel observations](H01-stock-usb-stream.md). Host return 0/1 reaches MAIN L/R. |
| Native receive allocates 16 descriptors with 88 B payload stride, copies stereo frames, preserves partial-packet cursor and zero-fills underflow into 32-frame stereo halves. | OS 1.54 static evidence and focused execution with synthetic descriptors, not a DMA/hardware measurement: [R07 facts](../evidence/r07/handoff-facts.json), [execution](../evidence/r07/receive-154.json), [method](R07-usb-coldfire-handoff.md). MAIN SHA-256 `5c58bf9e3949ef09977c5fc007a61e8d026931f67f1621238379dfb8ee4d31a2`. |
| 12 channels in the opposite direction do not establish 12 returns. | Source fact confirmed by asymmetric physical endpoints and H01 channel checks. Upstream [Overwitch Digitakt table](https://github.com/dagargo/overwitch/blob/master/res/devices.json) lists two device input lanes and twelve device output lanes (reviewed 2026-10-29; moving upstream branch, corroboration only). |

In this report IN/OUT always means **USB host direction**. The upstream device
table uses device input/output terminology. Do not reverse the two when counting
returns. The 12 IN lanes are recordings from the Digitakt toward the Pi; they
cannot carry eight Pi voices back into the Digitakt.

For existing framing, channels = `(packet_bytes - 32)/(7*4)`:
`(88-32)/28 = 2` OUT; `(368-32)/28 = 12` IN. The firmware consumer's stereo
stride corroborates the two-lane interpretation; descriptor arithmetic alone
would not establish PCM format. [Directional-layout patch](../patches/sidecar-firmware/0001-directional-overbridge-layout.patch),
[R07](R07-usb-coldfire-handoff.md), [H01](H01-stock-usb-stream.md).

Calculated at nominal 48,000 frames/s (the measured rate differs by ~10.73 ppm):

| Quantity | Calculation | Result |
| --- | --- | --- |
| Packets/s | `48000/7` | 6,857.143 |
| Packet audio span | `7/48000` | 145.833 µs |
| OUT PCM payload/s | `48000*2*4` | 384,000 B/s |
| OUT header/s | `48000/7*32` | 219,428.571 B/s |
| OUT protocol bytes/s | `48000/7*88` | 603,428.571 B/s (4.827 Mbit/s) |
| IN protocol bytes/s | `48000/7*368` | 2,523,428.571 B/s |
| Eight conventional mono returns | `32+7*8*4` | 256 B/packet; 1,755,428.571 B/s |
| Eight-return PCM deficit | `8*4*48000 - 2*4*48000` | 1,152,000 B/s; 4× existing PCM capacity required |

These rates exclude USB transaction overhead. The eight-channel packet exceeds
the advertised 88 B OUT limit by 168 B. Even discarding all 32 header bytes would
leave only 88 B, still below its 224 B PCM payload. Twelve-channel 368 B IN
packets cannot be reused as the OUT allocation.

At High Speed, interrupt `bInterval=1` gives a 125 µs service interval
([R02 interpretation](R02-usb-descriptors.md)). For the captured single-transaction
88 B OUT endpoint, `8000*88 = 704,000 B/s` is a service-opportunity upper bound,
not a sustained measured throughput. Existing cadence occupies about 85.714% of
those opportunities (14.286% nominal slack, 1,142.857 opportunities/s). It does
not create a third full-rate PCM lane: the stock framing/consumer remains seven
stereo frames per packet. Eight native 32-bit streams would exceed even the
704 kB/s bound. Aggregate USB bus speed is therefore not the relevant ceiling.

## Implementation work before even two tracks

The current renderer reads only `pair[2*i]` into track 0's block and clears both
`pair[2*i]` and `pair[2*i+1]`. Simply enabling another machine instance after this
hook would lose its return. A two-track design must snapshot/demultiplex both
lanes before clearing once, insert each into its own pre-filter mono block,
preserve the half selector and silence/reconnect rules, and avoid restoring the
dry MAIN contribution. [Renderer](../mods/usb-track/usb_track.c),
[existing integration report](I02-one-track-routing.md).

The control/engine path also needs explicit per-track identity and state:

- `capture_control` rejects `track != 0`; route/session/control state is singleton
  in [usb_track.c](../mods/usb-track/usb_track.c) and
  [worker.c](../mods/usb-track/worker.c).
- The wire `voice` field is a **hit generation**, not a track ID. Reusing it for
  track identity repeats a previously diagnosed error.
  [Contract](../control/link.h), [generation regression](../evidence/i04/generation-regression.json).
- [TRX rendering](../audio/trx_voice.c) clears the eight-column output frame then
  writes column 0; array width eight is not eight independent active engines.
  [Playout](../audio/playout.c) contains one voice/queue and rejects zero event
  block delta, so simultaneous different-track events need a scheduling redesign.

These are source facts, not newly tested multivoice behavior. Two mono lanes
can retain native track panning after independent insertion; a stereo synth
using both lanes leaves no conventional lane for another independent mono
return. Independent synthesis voices mixed on the Pi can share a lane but then
share the receiving track's local processing.

## Transport options and their limits

The first two options retain the measured stock PCM format. The remainder are
research hypotheses requiring new code/proofs; none is an established route to
eight qualified tracks.

| Option | Capacity / consequence | Required proof or limitation |
| --- | --- | --- |
| Split L/R into two mono machines | Two independent 48 kHz mono streams with unchanged 88 B packets. | Per-track control/engine/scheduler state, dual insertion and physical independence/crosstalk/stress tests. Recommended smallest next capacity experiment. |
| Pi mixes several engines into existing mono/stereo return | More synth voices may be possible within CPU budget, but only one mono or two mono local processing paths. | Benchmark DSP and aggregate peaks; cannot claim eight independent Digitakt filter/amp/send tracks. |
| Pack reduced-width samples into both 32-bit lanes | Fixed payload budget is 64 bits per 48 kHz frame: four 16-bit mono or eight 8-bit mono streams fit arithmetically. | Custom host packing and device unpacking before track DSP; establish bit-exact transport and scaling/quantization quality. Existing int16 output mapping/limiting is not a transparent arbitrary-bit container. These are raw fixed-width bit budgets, not implemented codecs or accepted sound quality. |
| Lower per-voice sample rate / time multiplex | Eight 16-bit mono streams fit the same raw bit budget only at 24 kHz each; eight 32-bit mono streams at 12 kHz each. | Custom demultiplexing, per-track reconstruction/resampling, bandwidth/alias rejection, state and timing tests. Rate/precision loss is a product decision; native 48 kHz transport remains unchanged. |
| Compress PCM | No fixed eight-track guarantee for arbitrary full-band signals at native width/rate follows from the available bytes. | Define codec, bounded worst-case size, algorithmic latency, decoder cost and acceptable losses; variable compression ratio is not deterministic capacity. |
| Enlarge/redefine EP `0x03` | Eight native mono streams need 256 B packets at the existing cadence. | Custom device descriptor, allocation/stride, queue/consumer and host changes; verify controller, memory, IRQ/render budget and enumeration/recovery. Changing the host channel count alone is insufficient. [Native constraints](../evidence/r07/handoff-facts.json). No established hardware/controller feasibility verdict. |
| Separate USB bulk/control endpoint | R02 exposes OUT `0x02` (512 B), MIDI OUT `0x01` (512 B). | Packet maximum is not audio bandwidth or a ready audio FIFO. Protocol ownership, device dispatch, bounded scheduling, rate, buffering and audio insertion must be established; do not hijack a live control path by assumption. [Captured endpoints](../evidence/r02-endpoints-2026-10-26.json). |
| Analog return / extra physical link | Existing stereo analog input is another possible source pair, not an eight-channel digital return demonstrated here. | Requires external audio hardware and new conversion/routing/latency qualification. Wiring/unused hardware paths are outside this desk study and existing project authorization. |

For alternate USB modes, R07 identifies a separate receive routine in mode 5,
but does not prove its format, boot behavior or extra lanes. R02's captured
alternate settings supply no larger OUT endpoint. Switching modes is not
evidence of a multichannel return. Rytm's dedicated voice returns are a
different device architecture, not a Digitakt capacity precedent.
[Mode analysis](R07-usb-coldfire-handoff.md), [physical mode match](../evidence/h02/usb-mode-154.json),
[project constraints](../PLAN.md).

## Headroom and acceptance gap

Measured one-voice Pi observations cover 13 one-second rows around I04R's short
timing pattern: 12-frame blocks with a 250 µs budget; maximum observed block
work 10 µs, control pass 18 µs, core 3 busy ≤3.5%, FIFO 78–96. The scoped clean
interval had no new underruns and no playout faults. This is **observed** 240 µs
block slack / 96% unspent budget for that workload, not a worst-case guarantee
or proof that all render, worker and USB work is included in one counter.
[Acceptance and counter scope](../evidence/i04/physical-acceptance.json).

For intuition only, multiplying 10 µs by two/eight gives 20/80 µs
(8%/32% of 250 µs). This is not a prediction or bound: shared overhead,
coincident STARTs, separate converter/engine state, control traffic, scheduling,
cache behavior and native rendering costs need measurement. The separately
observed 18 µs control pass is not automatically an additional per-voice audio
callback cost. The recorded timing range (19 hits, 11.021–11.750 ms analog
onset relative to a native click) similarly does not bound worst-case latency.
[Timing methodology/limits](../evidence/i04/event-worker-physical-timing.json).

The dense-modulation freeze disproves an unconditional scaling claim even for
one voice. V01-A remains parked; emulator cycle assumptions and short passing
profiles cannot establish physical CPU margin. Existing clean H01/H02 streams
prove their tested scope, not multivoice worst-case scheduling.
[V01](V01-persistence-stress.md), [emulator cost limitations](../evidence/v01/clock-cost-comparison.json),
[H02 scoped stability](../evidence/h02/physical-stability.json).

To close the acceptance gap after selecting a supported architecture:

1. Define the supported voice/track count, precision/rate, modulation profile,
   latency/jitter limits and safety margin explicitly; keep dense V01-A excluded
   until its cause/regression is resolved.
2. Prove per-track controls, simultaneous STARTs, lane identity, local
   filter/amp/send isolation and dry suppression offline, then on hardware.
3. Measure maxima for render/callback and control service separately, queue
   occupancy/minimum FIFO, USB gaps/errors/credits and native UI/render cost
   under coincident triggers, tails and the supported worst control workload.
   Record duration/build and measurement resolution; observed maxima remain
   workload-bounded, not formal WCET.
4. Qualify reconnect/invalidations and a soak with zero new faults/underruns;
   compare timing against one-voice acceptance. These are future hardware
   gates, not actions performed in this study.

Two-voice design and offline implementation/proofs can proceed within this
ceiling while the X01 hardware scaling gate remains deferred; this report makes
no runtime changes.

The coordinator's [derived capacity artifact](../evidence/x01a/capacity.json)
records the measured inputs and arithmetic separately from hardware claims.
Reproduce desk checks with `git rev-parse HEAD`, inspect the linked JSON/source,
and recompute the table:

```python
import json
from pathlib import Path
measured = json.loads(Path('evidence/h01/linux-stability.json').read_text())
for rate in (48000, measured['measured_frame_rate_hz']):
    for channels in (1, 2, 8):
        packet = 32 + 7 * channels * 4
        print(rate, channels, packet, rate * channels * 4, rate / 7 * packet)
# Columns: frames/s, channels, packet B, PCM B/s, protocol B/s.
```

The 60 B one-lane arithmetic is a payload requirement, not a stock 60 B
alternate setting; the one-voice prototype still sends/reserves 88 B stereo
packets. No dependency source, generated firmware or private capture needs to
be changed.
