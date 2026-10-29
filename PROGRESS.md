# Progress handoff

Status/ownership: [TASKBOARD.md](TASKBOARD.md). Read AGENTS.md and PLAN.md.

I04's one-voice prototype is accepted; dense-modulation qualification is V01
([report](docs/V01-persistence-stress.md)).

**V01 IN PROGRESS; no fix for the freeze.** The trigger is our per-snapshot
traffic, not the LFO or the USB attach: destination NONE at maximum LFO speed is
fully responsive, while snapshot traffic freezes the device and it stays frozen
after the cable is pulled. Every device counter stops together, so it is a stop,
not saturation. The RX/ACK interrupt path is the remaining suspect.

**The masked-send race was real but is falsified as the cause.** Candidate V01K
shipped an authoritative in-mask preflight; it still froze at ~256/512
([freeze](evidence/v01/v01k-freeze.json),
[capture](evidence/v01/v01k-freeze-live-2026-10-29.txt)). The fix is kept with
its regression, but it introduced a defect: a refused send is now a deferral,
so a stalled queue retries without bound and never fails closed. Worker elapsed
rose to74% of elapsed at~410 sends/s (was~25% at~816 sends/s).

**Plan change (owner):** the dense-modulation work is **parked** as V01-A and
does not block the rest. B and C need a stable device, which I04R is within its
accepted scope, so run them there with slow/shallow LFOs
([runbook](docs/V01-gate-b-c-runbook.md)). X01a capacity study is desk work and
also unblocked. Resume V01-A when the RX/ACK hypothesis is settled in the
emulator and the deferral is bounded; no flash before that.

Still unresolved: DTIM2 external rate, physical memory costs, core clock, and
boot→active FIFO0→11. Prior support detail: diagnostic clock reads are fixed by a
replayable patch (28 dependency/23 V01 tests), corrected200MHz dense panel
maximum is8.657ms, and the native-span observer matches firmware totals
([comparison](evidence/v01/clock-cost-comparison.json)).

**Installed:** V01K (frozen); restore I04R before normal use. Pi kernel
unchanged by V01K. Packages and recovery hashes are in the report. No new
hardware candidate is ready.
