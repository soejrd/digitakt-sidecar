# V01 Gate A — baseline versus no-self-wake

Prepared for a human-operated, **muted** scheduling trial. Neither image is a
qualified fix. Evidence index: `evidence/v01/gate-a-candidates.json`.

## Candidate pair and recovery

- **V01B:** priority 14, original self-wake, corrected timing and trial windows.
  Package: `private/v01/firmware-V01B/`.
- **V01C:** identical source and diagnostics, priority 14, only
  `V01_NO_SELF_WAKE` enabled. Package: `private/v01/firmware-V01C/`.
- Each package includes I04R, I04Q, H02F and stock 1.54 rollback. I04R is the
  accepted prototype for normal use; retain the Pi 128-frame kernel unchanged.
  Do not install V01P again. No bootloader, wiring or SD changes are needed.

Human flash using Transfer only after reviewing the candidate/recovery. Before
new hardware audio tests, lower monitors, confirm routing and back up a
**disposable** project. Do not change retained personal projects. The guided
capture script below verifies the local firmware/recovery hashes before asking
for any action. It sends no commands to either device and performs no flash.

## What the diagnostics mean

Selecting a different machine on track 1, waiting one UI tick, then selecting
USB1 starts a new diagnostic window. Merely reopening SRC or reselecting the
same machine does not reset it. The callback is compiled and tested through the
native machine-selection path. Configure settings **after** the selection;
normal machine defaults may be reapplied. Keep both candidates' settings equal.
No new USB protocol/session-reset behavior is introduced by the diagnostics.

`SECS` is elapsed render-clock seconds since selection. `UI`/`WORK` are entries
within that window, displayed modulo 10000. `WGAP`/`BATCH`/`UGAP` are corrected
milliseconds; time fields saturate at 9999. Worker fields show zero until the
worker acknowledges a new window: WORK must subsequently grow. `FAULT` remains
a lifetime sticky guard-fault count; resetting a window cannot clear a fault.
The render-wake counter also remains cumulative; it is not shown on this page.

The window includes setting changes, Save and Play/Stop; it excludes the earlier
boot. BATCH is elapsed time including preemption, not worker CPU time. No render
clock can reveal wall-time pauses in rendering itself. Before scoring any
trial, compare SECS with a separate stopwatch: over a ten-second interval it
should advance about ten seconds (allow one-second reading/rounding error).
Record mismatches and stop; do not interpret the gap metrics as wall time.
Record UI action delays separately; a video with an independent time reference
can quantify them if rough observation is ambiguous. New trials need a new
machine-selection window, not subtraction of maxima from the previous trial.

## Short reproduction loop

Use the same note/pattern/routing as the prior failure: track 1, USB1, C5,
velocity 100, PTCH LFO, MULT BPM16, triangle, free mode, SPH 0. Recover and record
FADE, tempo and pattern density; the older evidence did not freeze all of these
numerically. Do not silently assume an unknown setting matches. Keep the Pi
voice muted and `trxtrace` off. Read status before any trace dump.

1. Install V01B; confirm its version. Freshly reboot the Pi and record its
   build/uptime, FIFO underruns, playout faults and USB gap/error baseline.
2. Start a fresh USB1 diagnostic window. Set **SPD 32 / DEP 32**. Record any
   setting/Save delay, check SECS against ten seconds of stopwatch time, and
   read all seven fields before Play. If clean, Play for up to ten seconds,
   Stop, and read all fields and Pi counters again.
3. If the control is clean, start another window and repeat at **64/64**.
   Include the setting/Save phase: the old failure appeared before Play.
4. Stop at the first UI stall, new fault/underrun, stopped clock or guard fault.
   Capture evidence, leave the voice muted, and recover with a fresh session or
   rollback as needed. Do not continue into unrelated listening or dump a
   verbose trace during the measured interval.
5. Install V01C only after recording the V01B result. Fresh Pi boot, same two
   cases, same held settings and procedure. If the initial control fails,
   diagnose that before proceeding to dense settings.

The guided recorder is based on the diagnosing-bugs skill's human-loop template:

```sh
bash scripts/v01_trial.sh V01B 32
bash scripts/v01_trial.sh V01B 64
# After the baseline result has been reviewed:
bash scripts/v01_trial.sh V01C 32
bash scripts/v01_trial.sh V01C 64
```

Run one invocation at a time, only when its preceding case permits it. It stores
raw observations under ignored `private/v01/trial-*.json`; the agent reviews them
and writes compact shareable evidence. A muted short run is not an audible pass
or a supported modulation budget. Do not proceed to sound output before a clean
muted control session and the required lowered-monitor check; later audio tests
start with silence and a quiet limited/faded-in signal.

## Decisions

- Baseline reproduces, V01C improves UI **and** maintains delivery: self-wake is
  implicated; repeat the pair and inspect the actual readiness/service mechanism
  before proposing a production fix.
- Both stall: no-self-wake has not isolated the cause. Probe USB send critical
  sections/backpressure versus stock UI/render timing next.
- V01C improves UI but loses deadlines/FIFO: reject it, as with V01P. Do not
  adopt a responsiveness-only improvement.
- Baseline does not reproduce: compare exact settings and measurement overhead;
  record inconclusive, not fixed.

Static OS 1.54 inspection establishes that the native wait at `0x400016fe`
clears the whole positive post count. Posts at `0x40001770` can increment the
count, but old credits are not consumed one-by-one through extra worker waits.
That proposed mechanism is ruled out; repeated current self-posts may still
keep the worker ready. Removing self-wake also does not guarantee a sleep when
render/RX producers are continuously signaling.
