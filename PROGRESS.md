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

**Gate B emulator status:** SRC values are sound slots 46-52 in a version-4
160-byte record, so B is part-automatable, but the save->change->load round-trip
is **not yet proven** — three call attempts returned 0. Known-good pattern to
port is `deps/elekloader/tests/probe_digitakt_src.py`.
[Findings](evidence/v01/gate-b-emulator-feasibility.json). Emulator-only
candidate `private/gateb-emu` (`B01E`) exists for this; it is not a hardware
candidate. Awaiting the owner's decision between porting the harness and running
B on I04R by hand.

Still unresolved: DTIM2 external rate, physical memory costs, core clock, and
boot→active FIFO0→11. Prior support detail: diagnostic clock reads are fixed by a
replayable patch (28 dependency/23 V01 tests), corrected200MHz dense panel
maximum is8.657ms, and the native-span observer matches firmware totals
([comparison](evidence/v01/clock-cost-comparison.json)).

**X01 second offline milestone passes; default production remains one voice.**
[Report/commands](docs/X01-multivoice-offline.md). Opt-in v5 negotiation/v4
snapshots, two-track native capture/worker, portable Pi peer and actual Circle
glue/independent Output lanes pass host/ASan/UBSan/ColdFire checks. Full booted
OS1.54/native serializer/RX exchange passes UI selection of both USB1 machines,
simultaneous triggers, isolated panel SRC edit, ACK loss, unplug/reconnect and
stalled TX recovery; 17,815 render boundaries preserve the other six sources.
[Evidence](evidence/x01/integrated-154.json). Private `x01-control` / **X01E** is
emulator-only, never a hardware candidate. Default V01 worker/dependency tree
unchanged. Earlier 1/2/8-engine host benchmark remains valid within its scope.
Next offline work: per-track native locks/LFO/sound/copy tests and isolated Pi
kernel cross-build. Physical timing/margin and V01-A remain open; X01/X01a are
not complete. No hardware accessed or installed firmware/kernel changed.

X01 follow-up: single-lane mapping, timestamp/lease wrap and stalled ACKs despite
valid keepalives pass the session regressions. Pi build preflight is blocked by
the stopped local Docker daemon (missing socket); start Docker to release it.
Native lock/LFO tests remain independent offline work.

**Installed:** V01K (frozen); restore I04R before normal use. Pi kernel
unchanged by V01K. Packages and recovery hashes are in the report. No new
hardware candidate is ready.

X01 native LFO fixture investigation: opt-in `--lfo-probe` currently fails to
produce variation over 3 seconds (target stays80; other track stays60).
[evidence](evidence/x01/lfo-fixture.json). Native sound-fixture application is
unproven; no LFO pass or transport-failure claim. Next: observe seeded live
sound slots and native final values before host capture. Default probe retains
the previously passing scenarios.
