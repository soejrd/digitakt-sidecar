# R04 control-event ordering

Status: **DONE at static-source level**, claimed `2026-10-27T11:02:19Z` and completed
`2026-10-27T11:08:16Z`. Target is Digitakt mk1 OS 1.54. No hardware firmware,
audio or USB action occurred. Dynamic emulator execution is blocked in this
environment; the exact release condition is below.

## Result

The semantic order for an audio-track step is:

1. **Sequencer fire** reads the step and evaluates its trig/condition, note,
   velocity and stored p-locks. This is the `seq-events-dt1` path; its audio
   fire is `0x4006F4BE`, with lock resolution called at `0x4006F748`.
2. **Engine setup** copies the track's sound, restores the previous lock
   targets, then applies the current step's locks to the engine. The direct
   call chain is `engine_sound` (`0x40077B36`/`0x40077BB2`),
   `lock_restore` (`0x40077BCA`) and `lock_apply` (`0x40077BE4`).
3. **Modulation** runs after lock application: engine block copy
   (`0x40077E9A`), LFO processing (`0x40077EAC`), voice processing
   (`0x40077F8E`) and post-voice processing (`0x40077FA6`).
4. **Render** reaches machine-pages' `ev_render_voices` site
   (`0x40077FBA`). At this point `fw_voice_start` (`0x80001228`), the final
   8.8 A-H values in `fw_voice_params` (`0x80002794`), `fw_voice_note`
   (`0x80001F28`) and `fw_voice_vel` (`0x80001F18`) are available. This is the
   only point justified for sending final synthesis values to the host.

The p-lock list itself is *stored* input, not final audio values. Sending it at
step-fire would omit smoothing and LFO results. The render hook is therefore
the authoritative value source for I03; the sequencer events are authoritative
for step identity, condition outcome, note, velocity and restart intent.

## Evidence

Hash-pinned static report: [ordering-154.json](../evidence/r04/ordering-154.json).
The analyzer [r04_probe.py](../tools/r04_probe.py) refuses any MAIN image other
than SHA-256 `5c58bf9e3949ef09977c5fc007a61e8d026931f67f1621238379dfb8ee4d31a2`
and emits only addresses, call sites and semantic ordering.

```sh
PYTHONPATH="$PWD/deps/digiemu" "$PY" tools/r04_probe.py \
  --main private/i02-fixture/sections/section_3_MAIN_OS.bin \
  --output evidence/r04/ordering-154.json
```

The report records these call sites in address order:
`engine_sound` → `engine_sound` → `lock_restore` → `lock_apply` →
`engine_block` → `lfo` → `voice_process` → `post_voice`. The subsequent render
site is `0x40077FBA`. The pinned `seq-events-dt1` source independently defines
the step-fire events and 53-slot lock list; `machine-pages` defines
`ev_render_voices` and `cm_src_value(track, parameter)` as final values after
native locks, smoothing and LFOs.

Measured/source facts above are static. No cycle time, callback latency,
hardware ordering or audio result is claimed.

## Track identity and lock lifetime

The audio track index is part of both sequencer events (`track`, 0-7) and the
render event's `fw_track_blocks[32 * track + i]`. Machine-pages' final-value
accessor uses `cm_src_value(track, parameter)`. The host must therefore include
the track index in every voice event and keep per-track state isolated.

Lock lifetime is bounded in the engine path by `lock_apply` and
`lock_restore`; a later step restores the previous targets before applying its
own locks. The report does not claim that a lock remains audible for a specific
number of samples or survives a voice restart. Those require the dynamic trace
below or hardware observation.

## Ordered semantic events proposed for I03

Use a versioned, ordered stream with `track`, `voice_id`, `stream_generation`
and device/audio-block timestamp. Do not forward UI button presses.

| Event | When | Required fields | Purpose |
| --- | --- | --- | --- |
| `MACHINE_SELECT` | machine/render identity changes | track, machine id, generation | create or replace the host voice |
| `VOICE_START` | step-fire or panel/MIDI trigger is accepted | track, voice id, step, note, velocity, generation | start or restart the host voice; `fw_voice_start` is the render-side restart signal |
| `PARAM_SET` | render hook, after locks/LFOs | track, voice id, ordered A-H values (0-127 or 8.8), generation | update synthesis controls for the next block |
| `VOICE_END`/`SILENCE` | stop, mute, disconnect or underrun | track, voice id, generation | fade to silence without replaying stale audio |

Within a block, `VOICE_START` precedes `PARAM_SET` for the same voice; values
must be applied to the block they describe. A retrigger uses the same start
ordering. `MACHINE_SELECT` invalidates prior per-track state. An underrun or
generation mismatch causes silence, never reuse of the previous block.

## Unresolved and release condition

**Measured limitation:** retrigger is not evidenced as a distinct Digitakt mk1
sequencer event in the pinned sources. `fw_voice_start` is the only proven
restart bit; rapid retriggers must be treated as repeated `VOICE_START` events
until a dedicated trace distinguishes their timing. The Digitakt panel profile
also exposes no RETRIG control, so no claim is made that it is a stock panel
path.

Dynamic timing, per-block stage timestamps, track selection at execution and
lock lifetime remain unmeasured. The attempted dynamic probe aborted with
`SIGILL` in Unicorn `mem_map` under the execution sandbox; the same failure
occurs with all available Unicorn installations. **Release condition:** run a
dynamic probe in an environment where the pinned patched Unicorn can map JIT
memory, using the same private fixture, and record stage timestamps, track IDs,
`fw_voice_start` and lock restore/apply transitions. Until then I03 may
implement against the static contract, but must label integrated timing and
retrigger behavior as unverified.
