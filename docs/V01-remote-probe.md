# V01E — remote scheduling diagnostic

[Verified pair/evidence](../evidence/v01/remote-candidate.json). Both images are
staged under `private/v01/firmware-V01E/`. The [Pi SD update](../evidence/v01/remote-sd-deployment.json) is read-back verified and safely ejected; E boot and Digitakt installation are now verified; the E experiment is recorded in [its result](../evidence/v01/remote-severe-result.json). This probe
keeps V01C's priority 14/no-self-wake, event order and 128-frame Pi deadline.
It adds instrumentation; it is not a lag fix.

## Install and recover

Keep monitors lowered and work in the same disposable backed-up project. Keep
USB between Digitakt and Pi disconnected during installation.

1. With the Pi **off**, attach its SD to the Mac. Verify the existing boot kernel
   is the known `c057f063…` image. Preserve that kernel and the existing
   SHA256SUMS as new pre-remote backups, both locally and on the card. Do not
   overwrite earlier rollback files.
2. Copy only `pi/kernel8-rpi4.img` from this package into the active boot-kernel
   location. Verify the full read-back SHA256 against `paired-pi.json`; update
   the existing checksum manifest for this file only and verify other entries.
   Preserve all boot configuration, recordings, projects and other files. Eject
   cleanly, return the card and boot. The agent verifies the new build/status.
3. Human uses Transfer to install `Digitakt-V01E-diagnostic.syx`. Protected
   sections are verified unchanged; no bootloader or wiring change.
4. Reconnect USB only after both are installed. Agent sends `trx=off` and
   `trxtrace=off`; never enable the timing trace dump during the trial.

Pi rollback is this package's `pi/recovery/kernel8-rpi4.img`, the exact known
pre-probe `c057f063…` kernel. With Pi off, restore it and the backed-up checksum
manifest. Digitakt rollback is `Digitakt-I04R-previous.syx` for normal accepted
prototype use. Full hashes are in `paired-pi.json` and `manifest.json`.

## What changes

The worker emits a separate `F0 7D V01 01` 82-byte diagnostic SysEx at most once
per 1500 render blocks. It stages only after normal TX has drained; it does not
build a diagnostic backlog or ACK, reset sessions or alter STARTs. No telemetry
work is added to the render callback. The paired Pi decodes canonical 7-bit
uint32 values into a bounded last-sample buffer. Malformed diagnostic packets
are counted and consumed, never sent into the voice/session or stock engine.

`v01status` logs two compact lines on command, rather than per packet. Samples
include sequence/progress, render clock, UI/worker counts and render-gap maxima,
sticky guard faults, plus raw timer counter/mode and send/UI/worker timing.
Send count/total/max cover the adapter's IRQ-masked send critical section,
including its preflight; worker elapsed time includes preemption. Maxima and
counters are not a complete CPU profile or a direct input-dispatch latency trace.

Timer reads use DTIM0's existing mode/counter without configuring it. **Raw ticks
are not microseconds.** Calibrate using counter differences and Pi monotonic
sample time under a clean interval; account for uint32 wrap. If the timer does
not move, its mode/rate changes, or calibration is inconsistent, stop interpreting
the raw durations and revise the clock source. Emulator instruction timing does
not qualify the hardware clock. Some extra measurement work is unavoidable;
compare the same held profile to the established V01C failure.

## Trial

1. Keep playback stopped and Pi voice muted. Agent checks snapshot sequence,
   UI/worker/render progress and raw clock against wall time at the 32/32
   control. A manual trig may be needed to activate the voice-control stream;
   document any activation underruns separately before a measured interval.
2. Agent polls with:

   ```sh
   python3 tools/v01_remote_watch.py --url "$SIDECAR_URL" --seconds 20 \
     --output private/v01/live-gate-a/remote-control.json
   ```

   This sends status commands only. It checks mute, playout faults and new FIFO
   underruns, saves raw values, and does not decide overall acceptance.
3. Human attempts the known 64/64 settings and reports only whether controls
   lag. Do not navigate SRC to extract numbers; agent reads the remote samples.
   Stop at first lag/fault/underrun and disconnect USB if needed for recovery.
4. Compare UI wall-tick gaps, masked-send cost/count, worker batch cost and
   event progress at equal held settings. Repeated sequence means stale data,
   not zero work. Missing telemetry can itself indicate a stalled worker/render/
   USB path, but cannot distinguish those alone.

Both dense variants have already failed; this probe should identify the next
specific mechanism to test. Do not repeat arbitrary firmware priority changes,
mask meaningful updates, enlarge latency buffers or claim an audible/stress pass
from muted telemetry. The first-activation FIFO rise remains a separate open issue.

## V01F — aggregate elapsed follow-up

[Verified F pair](../evidence/v01/duty-candidate.json), staged under
`private/v01/firmware-V01F/`. [F Pi SD deployment](../evidence/v01/duty-sd-deployment.json)
is read-back verified and safely ejected; boot and human F installation
confirmation are pending. It keeps E's control behavior and
adds cumulative raw elapsed ticks in the worker and native render-event pair.
Existing core `ev_render_in`/`ev_render_out` hooks are used; no new stock code
sites, hardware timer configuration, audio waiting or allocation. The diagnostic
packet becomes version 2, 17 fields/92 bytes, still one best-effort frame per
render-second within the existing 96-byte native transport limit.

The paired Pi prints `V01TOTAL` and the wire version. It accepts old 15-field
version-1 E packets, marking the additional fields unavailable; missing totals
must never be interpreted as zero time. New totals are cumulative modulo uint32.
Compute per-source-snapshot deltas against the same raw timer and report each
as elapsed occupancy. Worker time includes rendering/other preemptions; render
time can include higher IRQs. **Do not add the two totals as pure CPU use.**
Do not divide Pi host-read event deltas by an unrelated device snapshot interval.

Install using the same backed-up SD and human Transfer procedure above, with
F's kernel and `Digitakt-V01F-diagnostic.syx`. Preserve the currently installed
E kernel (`8df83a5a…`) and current manifest under fresh **pre-F** backup names;
never overwrite pre-E or I04 rollback files. F's `pi/recovery/kernel8-rpi4.img`
is that exact E kernel. The older accepted-prototype `c057…` image and I04R
firmware remain available in prior packages. First confirm version-2 fields
advance/calibrate under 32/32 before one brief dense-setting capture. Human
reports lag only; no diagnostic-page extraction. Leave USB disconnected until
the pair is installed. No further density sweep or audible pass is authorized
by this preparation alone.
