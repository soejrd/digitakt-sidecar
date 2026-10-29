# Progress handoff

Status/ownership: [TASKBOARD.md](TASKBOARD.md). Read AGENTS.md and PLAN.md.

I04's one-voice prototype is accepted; dense-LFO qualification remains V01.
[I04 report](docs/I04-external-trx-voice.md) records accepted behavior and timing.

**V01 IN PROGRESS; no proven culprit or fix.** G still lagged at SPD64/DEP48
with Pi fault0/FIFO11 stable. Native MIDI/ACK, periodic USB and audio now run
together in the emulator without packet gaps, but dense panel service remains
below9.2ms under the tested timing assumptions: physical lag is not reproduced.
21 focused tests pass; [evidence](evidence/v01/native-audio-component.json).

**Next:** make diagnostic DTCN0 reads coherent with CycleClock, then establish
the undriven external DTIM2 clock before comparing hardware/model costs.
Do not fit delays or repeat speculative installs/sweeps. Gates B/C remain open.

The [V01 report](docs/V01-persistence-stress.md) owns the exact next steps,
reproduction command, acceptance gates and installed hardware/recovery details.
G remains installed, muted/trace off at last capture; unplug/recovery confirmation
and startup FIFO0→11 remain unresolved. No new hardware candidate is ready.
