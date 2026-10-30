# Progress handoff

Read AGENTS.md, TASKBOARD.md and PLAN.md. No complete V01 gate has passed.

**Installed hardware:** user restored I04R on2026-10-30. Pi still runs the
Oct29 12:31:45 kernel. Current project is disposable; only track1 is USB1,
LFO destinations NONE. USB reconnected. Pi voice muted and timing trace off.
No new firmware/kernel installed. Recovery hashes: V01 report.

**V01-B displayed-state checks:** user confirms project save/change/reload,
cold boot, Sound save/load, Pi-disconnected reload and complete Sound copy
restore displayed USB1/eight-SRC state. Full Sound copy is TRK+RECORD /
TRK+STOP; GRID FUNC copy is sequencer data, not complete Sound. Project was
restored to one USB1 track after copying. Evidence index in TASKBOARD/report.
No audio/note/velocity/dirty-flag or full B/C qualification from these checks.

**Timing failure now isolated to reload-only:** untouched muted baseline
97.6s and slow PTCH-only180.6s added no faults or counter growth during capture.
A separately marked single reload (user confirms no edits afterward) added
SC_PF_LATE and froze all128 trace rows. A fresh session started normally; its
sequence107 arrived4988 frames (~103.9ms) late. Source interval2048 frames,
Pi generated7080/requested7077, FIFO94 and scheduler queue0. This narrows the
failure to a source-clock/control-delivery discontinuity, not empty host FIFO
at that instant. Native clock pause vs delivery delay remains unresolved.
[Hardware](evidence/v01/i04r-separated-timing.json),
[timing-only replay](evidence/v01/reload-timing-replay.json).

The HTTP capture tool preserves historical fault baselines, stays muted,
automatically ends on a new fault, and dumps a complete bounded trace only
after measurement. Four local-server tests pass. Old dumps in logs must not
be mixed: use each capture's standalone trace.txt. Public replay data omits
all actual note/velocity/SRC payloads. No live trace printing.

Underflows counts empty requested frames, including intentional FIFO flushes
on epoch changes; counter growth alone is not producer starvation. Post-trial
printing can perturb counters, so compare only within marked capture windows.
[Proof](evidence/v01/output-counter-semantics.json). Earlier mixed load/edit
trial stays inconclusive; the new reload-only capture supersedes that trigger
uncertainty, not the unresolved root cause.

**V01-A remains unqualified; dense hardware work parked.** The physical freeze
was per-snapshot/ACK traffic, not LFO destination NONE or USB attach. Masked
preflight race existed but V01K still froze. New offline source fix bounds
no-progress TX to USB_TRACK_LEASE, independently of valid old keepalives.
Old worker remains active after878 blocks; fixed worker retires after782 and
recovers, including uint32 clock wrap. ASan/UBSan, event-worker regressions,
ColdFire imports and emulator native-component preflight pass.
[Red/green](evidence/v01/stalled-tx-bound.json),
[emulator](evidence/v01/bounded-worker-emulator.json). Private V0DE fixture is
emulator-only, not a hardware candidate. This is not a freeze or B timing fix.

RX source audit: native_rx calls dt1_rx_trampoline→dt1_rx_bytes, which appends
raw bytes; protocol parsing is in task-side dt1_rx_service. This was already
true in V01K. Current native USB/audio component model observes RX at IPL0;
it does not reproduce/refute the physical RX/RTOS/bus freeze. Do not add a
second speculative queue based on the old claim that our IRQ parses frames.
DTIM2 external rate, physical memory costs/core clock and native timing
attribution remain open. No speculative flash or fitted emulator delays.

Persistence correction: eight TRX controls are slots17-24; slots46-52 are
extras, used for V01 diagnostic readouts. Old extra-slot attempts are not
proof of eight-TRX-control persistence. Keep their failed-call history.

**X01:** opt-in two-track native/host/Output,1/2/8 host engines, seeded LFO,
locks, sound locks, SRC-page copy, native route changes and isolated Pi build
pass within their documented offline scope. Route-edge wake fix is in source.
[Report](docs/X01-multivoice-offline.md). Private x01-routes/X01E remains
emulator-only; hardware expansion waits for V01-A, measured X01a margin and
reviewed recovery. Full track/pattern copy, runtime stack/CPU margin and
physical two-return timing remain open. Eight independent returns unproven.
