# Progress handoff

Status/ownership: [TASKBOARD.md](TASKBOARD.md). Read AGENTS.md and PLAN.md.

I04's one-voice prototype is accepted; dense-LFO qualification remains V01.
[I04 report](docs/I04-external-trx-voice.md) records accepted behavior and timing.

**V01 IN PROGRESS; the masked-preflight fix did not stop the freeze.** V01K was
flashed: at ~256/512 LFO speed the device crashed and lagged extremely, and it
stayed frozen 30 s after the cable was pulled. All device counters stopped
together, so it is still a stop, not saturation. The race was a real defect but
not the cause. [Freeze](evidence/v01/v01k-freeze.json),
[live capture](evidence/v01/v01k-freeze-live-2026-10-29.txt).

**Regression to close first:** making a zero-length send a deferral removed the
fail-closed path, so a stalled queue now retries without bound. Worker elapsed
is 74% of elapsed at ~410 sends/s, versus ~25% at ~816 sends/s in the original
dense capture. Bound the retries (limit or age, then retire) before anything
else.

Earlier isolation of the freeze to per-snapshot traffic still holds: the
destination-OFF test passed (attached, USB1, max LFO speed, no routing: fully
responsive), so attach alone is harmless and neither the LFO nor CPU is the
cause. With routing on it freezes, survives detaching and needs a power cycle.
The unlocked-preflight/masked-send race was real and is now fixed, but V01K
shows it was not this hang. The RX/ACK interrupt path is the remaining suspect.
[Evidence](evidence/v01/2k-attach-freeze.json),
[live capture](evidence/v01/2k-freeze-live-2026-10-29.txt).

Diagnostic clock reads are fixed with a replayable patch: intra-block native
reads advance by97 modeled cycles instead of0;28 dependency and23 V01 tests
pass. Corrected200MHz dense panel maximum is8.657ms. Existing hardware masked
sends average~15µs versus8.8µs modeled, while hardware worker elapsed grows much
more with density. [Comparison](evidence/v01/clock-cost-comparison.json).

The new native-span observer matches firmware totals without changing prior
measurements: only2 batches overlap rendering in either model profile; maximum
non-render batch time stays~94µs.

**Fix in place, unproven on hardware.** `fw_send` now re-runs the preflight
after masking and refuses instead of entering the native sender; a zero return
is a deferral, not a fault. Host regression fails pre-fix and passes now.
Component profile is unchanged otherwise: one session, gate1, drained queue,
continuous audio, no modelled hang.
[Emulator result](evidence/v01/masked-preflight-fix-emulator.json).
An early index-margin guard I tried stalled traffic (only our sends advance the
index) and was reverted; the native wrap already covers it.

**Next:** bound the deferral and explain the worker-elapsed inflation offline;
then make the RX/ACK interrupt path append-only. Do not flash again until both
are resolved. Rollback for normal use remains I04R. DTIM2 external rate,
physical memory costs and core clock remain unresolved.
No fitted delays or speculative installs/sweeps. Gates B/C remain open.

The [V01 report](docs/V01-persistence-stress.md) owns the exact next steps,
reproduction command, acceptance gates and installed hardware/recovery details.
G remains installed; the 2K freeze left it needing a power cycle. Startup
FIFO0→11 remains unresolved. No new hardware candidate is ready.
