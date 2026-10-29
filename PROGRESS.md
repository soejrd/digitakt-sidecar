# Progress handoff

Status/ownership: [TASKBOARD.md](TASKBOARD.md). Read AGENTS.md and PLAN.md.

I04's one-voice prototype is accepted; dense-LFO qualification remains V01.
[I04 report](docs/I04-external-trx-voice.md) records accepted behavior and timing.

**V01 IN PROGRESS; hard freeze reproduced, no proven culprit or fix.** At
maximum LFO speed the Digitakt is fully responsive with USB detached and freezes
the instant the cable is attached; the freeze survives detaching and needs a
power cycle. The last healthy counters were small (UI gap34ms, batch4ms,
guards0) and then every Digitakt counter stopped together, so this is a stop,
not CPU saturation, and native 2K LFO cost is excluded. Suspect the
interrupt-masked call into the native MIDI sender, whose busy waits are drained
only by a completion interrupt; unproven.
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

**Next:** power-cycle, reconnect, select USB1, run the LFO at maximum speed with
destination OFF for ~30s (no flash). Responsive ⇒ send path/volume; frozen ⇒
attach/enumeration or ISR-context RX path. The worker-versus-render measurement
is deprioritised until the freeze is explained. DTIM2 external rate, physical
memory costs and core clock remain unresolved.
No fitted delays or speculative installs/sweeps. Gates B/C remain open.

The [V01 report](docs/V01-persistence-stress.md) owns the exact next steps,
reproduction command, acceptance gates and installed hardware/recovery details.
G remains installed; the 2K freeze left it needing a power cycle. Startup
FIFO0→11 remains unresolved. No new hardware candidate is ready.
