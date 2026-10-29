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

**X01 remains IN PROGRESS; default production is one voice.**
[Report/commands](docs/X01-multivoice-offline.md). Opt-in v5/v4 two-track native
capture/worker, portable peer, Circle glue and independent Output lanes pass
host/ASan/UBSan/ColdFire checks. The booted OS1.54 exchange preserves other
sources through 17,811 render boundaries, simultaneous hits, panel edits,
ACK loss, reconnect and stalled TX ([evidence](evidence/x01/integrated-154.json)).
Session regressions cover single-lane mapping, timestamp/lease wrap and stalled
ACKs despite valid keepalives. Earlier 1/2/8-engine host benchmark remains valid
within its scope; eight independent Digitakt returns are not established.

Native seeded LFO and distinct stored-lock restoration pass. New offline
coverage: all eight SRC controls edited through native UI copy from track2 to
track1 via held SRC+RECORD / SRC+STOP; exact engine values transfer, neighboring
sound slots and target non-SRC slots stay intact. Two RAM pool sounds on step3
(no p-locks) produce60→72→60 and60→84→60 at matching native/host STARTs.
Both keep the same active session with zero host fault.
[Page copy](evidence/x01/copy-src-154.json),
[sound locks](evidence/x01/sound-lock-154.json). Panel gestures service replies
throughout and wait for native smoothing. These do not pass V01-B, saved-project
persistence or full track/pattern copy. USB/host timing remains modelled.

Docker now runs and the isolated Pi4 ARM build passes after correcting the
adapter's target headers; the actual Circle host regression passes.
[Build evidence](evidence/x01/pi-build.json): image2,482,984 bytes,
ELF BSS1,530,433 bytes, peer static68,032 bytes. Cached libraries reused;
runtime heap/stack peaks, CPU margin and physical timing remain unmeasured.
Native machine/route changes now pass9 phases, including a shifted pair,
unsupported third track, empty route and recovery. Exact selected-source PCM
matches negotiated lanes; unselected sources stay intact and closed gates
remain silent ([routes](evidence/x01/routes-154.json)). A new regression
reproduced worker sleep after route retirement with UI fallback wakes disabled.
The X01 renderer now posts its existing bounded semaphore on route edges;
third/empty recovery passes without another panel input
([before/after](evidence/x01/route-wakeup-regression.json)). No USB/allocation/wait
was added to render; normal one-voice and V01 paths remain unchanged. This does
not fix V01-A. Current rebuilt emulator-only fixture: `private/x01-routes`,
label X01E; old `private/x01-control` belongs to the previous source hashes.
Next offline: full track/pattern copy.
Hardware expansion still needs V01-A, measured X01a margin and reviewed recovery.
Private X01E remains emulator-only. No installed firmware/kernel changed.

**Installed:** V01K (frozen); restore I04R before normal use. Pi kernel
unchanged by V01K. Packages/recovery hashes are in the V01 report. No new
hardware candidate is ready.
