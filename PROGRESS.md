# Progress handoff

Status/ownership: [TASKBOARD.md](TASKBOARD.md). Read AGENTS.md and PLAN.md.

I04's one-voice prototype is accepted; dense-LFO qualification remains V01.
[I04 report](docs/I04-external-trx-voice.md) records accepted behavior and timing.

**V01 IN PROGRESS; hard freeze isolated to our per-snapshot traffic.** The
destination-OFF test passed (attached, USB1, max LFO speed, no routing: fully
responsive), so attach alone is harmless and neither the LFO nor CPU is the
cause. With routing on, the freeze is immediate on connect, survives detaching
and needs a power cycle; the last healthy counters were small (UI gap34ms,
batch4ms, guards0). Source defect: the preflight reads the USB queue counters
unlocked, then `fw_send` masks all interrupts and calls the native sender
without re-checking, whose busy waits spin on those counters and are drained
only by the completion interrupt. Unproven as the cause; the RX/ACK ISR path is
the alternative.
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

**Next:** candidate `V01K` is verified and staged (firmware only; Pi kernel
unchanged) with rollback to I04R or stock. Run the recorded muted 2K attach
trial: [package](evidence/v01/fix-k-package.json). If the freeze persists, the
RX/ACK interrupt path is next (make it append-only). The worker-versus-render
measurement stays deprioritised. DTIM2 external rate, physical memory costs and
core clock remain unresolved.
No fitted delays or speculative installs/sweeps. Gates B/C remain open.

The [V01 report](docs/V01-persistence-stress.md) owns the exact next steps,
reproduction command, acceptance gates and installed hardware/recovery details.
G remains installed; the 2K freeze left it needing a power cycle. Startup
FIFO0→11 remains unresolved. No new hardware candidate is ready.
