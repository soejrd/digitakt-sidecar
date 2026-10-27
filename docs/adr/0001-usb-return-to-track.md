# D01 — Route the native USB return into one audio track

**Decision: conditional GO for I02 implementation, not hardware qualification.**
Digitakt mk1 OS 1.54 has an identified native USB receive path and a viable
dry-MAIN suppression mechanism. Use a bounded ColdFire copy/clear operation to
replace one existing audio track's source with Pi-generated mono PCM. Keep the
Pi 4B and the patched Sidecar host as the transport/engine foundation.
Owner: codex, 2026-10-27. No firmware flash or hardware audio test is authorized
by completion of this decision alone.

## Basis and limits

| Evidence | Supports | Does not establish |
| --- | --- | --- |
| [R02 physical descriptors](../R02-usb-descriptors.md) | Alt 3: 368-byte IN, 88-byte OUT | Live sample rate, format, clock drift or latency |
| [I01 host tests](../I01-directional-usb-layout.md) | Separate directional layouts, replayable patch | Pi kernel build or working physical stream |
| [R03 OS 1.53 emulator](../R03-REPRODUCTION.md) | Source replacement before track filter/amp; second track unchanged | Full OS 1.54 track behavior or hardware execution cost |
| [R07 OS 1.54 static/function execution](../R07-usb-coldfire-handoff.md) | Native receive buffers, half selection, underflow zero-fill, dry suppression with internal mix retained | Full boot, real USB DMA, startup/reconnect or complete track integration |

The conjunction warrants a prototype, not a claim that the sidecar works.
R07 supersedes R01's section-8-only/DSPI handoff assumptions. No section 8
modification or internal wiring is an evidenced prerequisite. Actual hardware
path selection must still agree with the native path before H02 proceeds.

## Routing and ordering contract

1. Leave stock USB reception, descriptor ownership and requeueing intact.
   `0x400035F4` consumes completed packets and fills the next 32-frame stereo
   half; the audio callback must never inspect a USB-owned payload or wait.
2. At the verified pre-track-processing hook, after stock playback has produced
   the source blocks, snapshot the current selector `s` at `0x4199F134`.
   The intended source is `0x80001F60 + 256*s`, the half MAIN will consume.
   The receiver fills `s XOR 1`; taking that half advances the wrong block.
3. Copy the 32 left-channel int32 words into track 0's 128-byte source block
   at `0x80001A18`, replacing its sample playback. Initially transmit silence
   on the right channel. Establish scaling/polarity on 1.54 before treating
   received words as production track Q31 samples.
4. After preserving the source, clear all 256 bytes of that stereo half before
   stock MAIN summation (`0x40072194..0x4007221E`). Clear both channels even
   though only left is used. This consumes the entire USB return for this mode;
   simultaneous ordinary stereo USB-to-MAIN monitoring is unavailable.
5. Let local overdrive/filter/amp/sends process the replacement block. Other
   track buffers, internal MAIN routing and the next USB half remain intact.
   Stock selector toggling at `0x40078352` remains untouched.

I02 must prove these operations occur in this order in one complete OS 1.54
render cycle. R03's `0x40077FBA` hook is **1.53-only**. The pinned elekloader
SDK describes `ev_render_voices` orders 10–49 as source generation after
playback and before overdrive; verify its 1.54 installation, ABI/register
preservation and position relative to the MAIN consumer before using it.
Do not install the copy/clear at an address inferred only from nearby references.

Only enable routing when the external-machine mode owns the return. Silence,
underruns and disconnects must replace the selected track with zero, never
replay a stale block or restore its stock sample unexpectedly. Validate both
halves, startup state and mode changes; flush stale PCM at transitions. No
allocation, USB calls, unbounded loops or host-response waits in the hook.
The prototype performs 32 mono word copies plus 64 word clears per block;
its actual worst-case cost must be measured, including hook overhead.

## Clock and timing budget

The Digitakt audio render cadence is the pacing reference. Pi buffering and
resampling must follow measured device consumption, rather than an independent
wall-clock assumption. R07 clock-enable writes establish peripheral enables,
not the physical sample clock or drift control. Preserve TRX-B2's 44.1 kHz
behavior; convert to the measured rate (160/147 only if it is 48 kHz).

For measured sample rate `F`, the render deadline is `T = 32/F` seconds.
At the currently assumed 48 kHz this is 0.667 ms; seven packet frames span
0.146 ms of audio, which is not itself a measured USB scheduling interval.
One opposite-half staging block is visible in code. Do not add it twice when
measuring a complete end-to-end path.

Budget trigger-to-output as control transport + Pi scheduling/synthesis +
conversion + host/device buffering + device staging/processing. H01 measures
transport rate, packet cadence, gaps, queue occupancy and drift; H02 measures
complete onset latency/jitter and worst-case render cost with stock tracks
active. R04 must establish whether sequencer events arrive early enough for
lookahead. Live keys have no assumed lookahead. A same-block round trip is
not a requirement the audio ISR may wait to satisfy.

I02 must show bounded execution; H02 must show total render time stays below
`T` with measured margin and no deadline misses. Record maximum and distribution,
not only mean latency. No numerical musical latency/jitter tolerance has been
agreed: set it from the measured one-voice trial before declaring timing GO
or scaling. A stable stream alone is insufficient evidence of usable timing.

## Release gates and alternatives

- **I02 is READY:** verify a complete 1.54 cycle, both halves, scaling/polarity,
  exact dry silence, filter/amp/sends, zero-source replacement, stock-track
  regression, lifecycle behavior and bounded hook cost. Prepare a candidate
  and recovery procedure for a human trial; do not flash as part of I02.
- **H01 remains READY independently:** build the Pi host and verify stock
  streaming, actual format/rate/channel placement, latency/jitter and the
  taskboard's four-minute zero-gap/underrun run.
- **H02 stays BLOCKED on H01 + I02 and a human hardware trial:** qualify real
  track processing, timing and ten-minute stability. Confirm native USB path
  selection. Retain output limiting/fade-in; start with silence and quiet tone.
- **R04 and R05 are complete at source/benchmark level; I03 is READY.** R04
  established the render hook as the final lock/smoothing/LFO value source and
  proposed ordered semantic events. Dynamic timing and a distinct retrigger
  path remain unverified; I03 must label those limits. A MIDI-track demo still
  does not discharge the requirement.

Reopen D01 if the unit selects a different receive path, the required hook
ordering fails, suppression damages internal audio, or measured timing cannot
meet an explicitly agreed bound with practical buffering/lookahead. Do not
compensate by blocking the renderer or assuming more USB return channels.

Stock MIDI-to-MAIN remains a useful H01 diagnostic but cannot replace native
track integration. UI USB gain minimum is unnecessary for suppression and its
gain law remains unqualified. Controller changes and alternate transports are
fallback research only if the native route fails. More independent voices and
Teensy remain deferred: the observed stereo return is not eight separate buses.
