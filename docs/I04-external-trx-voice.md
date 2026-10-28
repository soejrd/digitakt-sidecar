# I04 — external TRX-B2 voice

Status: **BLOCKED at physical acceptance; host prototype passes** (2026-10-28).
Claim: Codex, 2026-10-28 07:00 UTC. User's “l04” resolves to I04, the sole READY row.
No hardware accessed, SD written, firmware flashed, or audio played.

## Implementation and policy

`audio/trx_voice.c` owns one persistent real mdkit TRX-B2 instance at the
sources.json pin, `96157fe4fbda17bf4cae3168f52437d8232e6570`. It uses the public
init/set_control/trigger/render API without modifying synthesis arithmetic.
SRC A–H map in order to PTCH, DEC, RAMP, HOLD, TICK, NOIS, DIRT, DIST.
Signed 8.8 values truncate to integer controls and clamp to 0–127.
Every accepted SC_START calls trigger after installing all eight controls;
parameter-only snapshots preserve the tail and converter state.

**Prototype note/velocity policy:** SRC PTCH is authoritative. Note is ignored,
so sequencer note changes do not transpose this percussion voice. Host velocity
gain is unity: the existing Digitakt AMP path owns velocity. Host tests prove
note/velocity independence, not the physical AMP velocity curve. That curve
and mute behavior still need measurement. The return is mono channel 0 with
channels 1–7 zero. A fixed 0.125 gain and hard ±4095 int16 ceiling limit this
prototype to approximately −18 dBFS before local processing.

The engine stays at 44.1 kHz. R05's persistent 160/147 converter supplies exactly
48 kHz output, draining available phases before rendering another native sample.
For 48,000 output frames, the implementation consumes 44,131 native frames
(including 31 priming/lookahead frames). Whole and ragged application blocks
are bit-identical. Converter state resets on session invalidation; it does not
reset on each START. No allocation, USB I/O or wait occurs in render. Coefficient
initialization occurs before activation, not in the audio render function.

Patch **0006**, after 0001–0005 at sidecar-firmware
`6cff67b9c6db93f8c8c64c8d9108a0cbad9a0457`, installs controls before ACTIVATE,
clears the engine on invalidation, and replaces the integrated diagnostic-tone
render call with the gated TRX-B2 renderer. Stock Rytm selection remains as before.
The H02 tone API remains available for its regression harness; the production
integrated render call now selects TRX-B2. The offline overlay copies pinned,
CC0 mdkit source and local wrapper/converter files; NOTICE records their licences.
No firmware-derived code enters that overlay. No dependency commit or push is
needed: the six replayable patches plus overlay are the deliverable.

## Evidence and reproduction

[Compact results](../evidence/i04/host.json) separate executed host/build checks
from unmeasured physical requirements. From this repository:

```sh
bash scripts/i04_prepare_circle.sh
bash scripts/test_i04.sh
python3 scripts/test_i04_replay.py
bash scripts/test_i03.sh
bash scripts/test_i03_handshake.sh
bash deps/sidecar-firmware/pi/engine/build.sh
```

`test_i04.sh` uses ASan/UBSan for the real engine, codec, native bridge, Circle
link and Output. It verifies ragged PCM equivalence, exact native-frame count,
note/velocity independence, audible host PCM, START call counts, bounded output,
reset equivalence, unsupported voice rejection, handshake, loss-induced silence,
reconnect, lease expiry and congested ACK failure. This is executable host
production-source evidence, not a live USB or physical filter/lock/LFO proof.
The Pi 4 AArch64 Circle kernel cross-build passes (2,472,056 bytes); its SHA-256
is recorded in the compact evidence. It was not copied to an SD card.
The existing I03 dynamic emulator and H02 physical-tone reports establish their
own narrower evidence; neither is promoted into an I04 physical result.

## Missing acceptance and exact release condition

I04 is not DONE. Release requires a human-approved hardware session with the Pi
and Digitakt available (the prior session ended with the Pi off), followed by:

1. Prepare and verify a recoverable private kernel candidate; preserve SD settings
   and existing H02/H01 backups. Have the human lower speakers before any new
   audio test, begin with silence, then a quiet limited/faded-in voice.
2. Physically demonstrate TRX-B2 from one native audio track with SRC edits,
   locks, LFOs, distinct retriggers, local filter/AMP/sends, and an unaffected
   neighboring track; verify the velocity policy through the real local AMP.
3. Measure integrated latency/jitter and Pi worst-case render/queue margin,
   establish acceptable timing limits, and record stability/reconnect behavior.
   Host success and kernel linking do not establish these values.

**Software qualification limits:** snapshots currently install on receive on
core 3, not at scheduled device block timestamps. Delayed bursts can compress
several STARTs before an engine render; mdkit has a pending trigger rather than
a timestamped trigger queue. Counting trigger calls does not prove distinct
on-time audible hits. A timestamped bounded playout/event design must be chosen
and tested against measured transport latency before timing acceptance. Do not
claim lock/LFO alignment or dense retrigger fidelity from this prototype.

R05's 32-tap filter is unchanged: its measured 22 kHz rejection is −26.3 dB.
A steeper filter with measured rejection is required before claiming production
audio quality. The fixed output ceiling can clip converter overshoot; there is
no new sound-quality qualification. Integrated CPU timing, live-key latency,
semitone mapping, persistence and dense modulation stress remain unmeasured.
There are no other independent READY tasks: V01 depends on I04; X01/X02 remain
deferred. Next action is qualify scheduling/filter design while preparing the
human hardware measurement session; do not deploy this prototype automatically.
