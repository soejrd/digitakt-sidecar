# Progress handoff

Status/ownership: [TASKBOARD.md](TASKBOARD.md). Read AGENTS.md and PLAN.md.

I04's one-voice prototype is accepted; dense-LFO qualification remains V01.
[I04 report](docs/I04-external-trx-voice.md) records accepted behavior and timing.

**V01 IN PROGRESS; no proven culprit or fix.** G still lagged at SPD64/DEP48
with Pi fault0/FIFO11 stable. Native MIDI/ACK, periodic USB and audio now run
together in the emulator without packet gaps, but dense panel service remains
below9.2ms under the tested timing assumptions: physical lag is not reproduced.

Diagnostic clock reads are fixed with a replayable patch: intra-block native
reads advance by97 modeled cycles instead of0;28 dependency and23 V01 tests
pass. Corrected200MHz dense panel maximum is8.657ms. Existing hardware masked
sends average~15µs versus8.8µs modeled, while hardware worker elapsed grows much
more with density. [Comparison](evidence/v01/clock-cost-comparison.json).

The new native-span observer matches firmware totals without changing prior
measurements: only2 batches overlap rendering in either model profile; maximum
non-render batch time stays~94µs.

**Next:** prepare one bounded measurement of physical worker elapsed versus
nested render time, validated offline before a flash. DTIM2 external rate,
physical memory costs and core clock remain unresolved.
No fitted delays or speculative installs/sweeps. Gates B/C remain open.

The [V01 report](docs/V01-persistence-stress.md) owns the exact next steps,
reproduction command, acceptance gates and installed hardware/recovery details.
G remains installed, muted/trace off at last capture; unplug/recovery confirmation
and startup FIFO0→11 remain unresolved. No new hardware candidate is ready.
