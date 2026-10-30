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

**Hardware preflight (2026-10-30):** user reports I04R restored, Pi restarted,
disposable project with USB1 on track1 and LFO destinations NONE. Pi build
remains Oct29 12:31:45. Native control arrives: 7 STARTs/7 consumed, TRX faults0.
Underruns rose from1 (uptime24/48s) to15 by uptime1886s; cause/timing unproven.
Muted samples at1903/1913/1923s stayed15, USB gaps/errors0, no power throttling.
Voice enabled with existing fade-in: arm1 accepted, faults0, underruns still15
at1963/1966/1969s. User confirms quiet drum, PTCH change and responsive UI.
At2062s, all33 STARTs consumed, TRX faults0, underruns still15; no new power
faults. Voice muted again for project save/load testing; trace remains off.
This is a preflight, not a clean-boot stability result or any Gate B/C pass.
No X01 hardware candidate is ready; V01-A remains unresolved. Recovery hashes
remain in the V01 report.

**Gate B physical B1 trial:** user confirms all eight prescribed SRC values and
USB1 restored after project save/change/load on I04R. This is not a full pass:
afterward (uptime2236s) Pi faults rose0→1, code3 SC_PF_LATE (due217216,
clock222180; 4964 frames late), underruns15→44. User reports normal reload, no visible freeze/unusual pause. Exact timing/cause
within the sequence is unproven. Voice remains muted; audio cases stopped. Raw log private.
[Evidence](evidence/v01/gate-b-i04r-project-reload.json). Note/velocity, dirty
flags, post-reload sound, cold boot and Pi-disconnected persistence unverified.

B9 displayed-state portion confirmed by user: with Pi USB disconnected,
PTCH/DEC changed90/10 then project reloaded; all eight prescribed SRC values
and USB1 selection restored. This excludes host caching for displayed state.
[Evidence](evidence/v01/gate-b-i04r-without-pi.json). USB remains disconnected;
cold-boot check subsequently confirmed (below). No audio/note/velocity/dirty-flag qualification. B1's
late-playback fault and underrun growth remain open; Gate B not complete.

B2 displayed-state portion confirmed: Digitakt powered off30s, rebooted,
loaded saved test project with USB still disconnected; USB1 and all eight
SRC values restored. [Evidence](evidence/v01/gate-b-i04r-cold-boot.json).
Next: native Sound save/change/load. No Gate B audio pass or timing-fault fix.
