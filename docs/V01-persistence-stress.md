# V01 — one-voice qualification

**IN PROGRESS; unaccepted.** Ownership: [TASKBOARD.md](../TASKBOARD.md).
Target: Digitakt mk1 OS 1.54 + Pi 4B/Circle, one USB1 voice on track 1.
Gate A resolves dense-LFO UI lag; Gates B/C qualify persistence and recovery.
More voices and Teensy remain deferred.

**State:** the device freezes under dense modulation traffic. The trigger is our
per-snapshot traffic (snapshots out, Pi ACKs back), not the LFO and not the USB
attach. One real source defect was found and fixed, and the hardware test showed
it was **not** this hang. The RX/ACK interrupt path is the remaining suspect.

## Current next work — offline only, no flash

1. Bound the deferral this session introduced. [Candidate V01K](../evidence/v01/fix-k-package.json)
   made a refused send a deferral, which removed the fail-closed retire: a stalled
   native queue now retries indefinitely. Add a retry/age limit that retires.
   Worker elapsed also rose to 74% of elapsed at ~410 sends/s (was ~25% at
   ~816 sends/s); explain that before trusting any timing from this image.
2. Make the RX/ACK interrupt path append-only. The Pi ACKs every snapshot, so
   `native_rx` runs at the send rate and currently parses frames and wakes in
   interrupt context.
3. Reproduce or refute the remaining RX hypothesis in the emulator, then one
   short muted candidate. Do not flash on "the last hypothesis was wrong".
4. Keep DTIM2's external rate unresolved: firmware selects T2IN, but does not
   establish the board signal. Snapshot PLL registers are zero; do not infer a
   core clock from them, and do not invent clock rates or memory delays.

Investigate in software first. No fitted delays, no speculative installs, no
broad knob sweeps. A hardware test must answer one hypothesis and have a stop
condition.

## Established findings

| Evidence | Finding |
| --- | --- |
| [Freeze vs LFO speed](../evidence/v01/2k-attach-freeze.json), [capture](../evidence/v01/2k-freeze-live-2026-10-29.txt) | LFO at maximum speed with USB detached: fully responsive. Attach plus routing: freeze. Last healthy counters small (UI gap34ms, batch4ms, guards0), then every device counter stopped together: a stop, not saturation. Native LFO cost is excluded. |
| [Destination-OFF test](../evidence/v01/2k-attach-freeze.json) | Attached, USB1 selected, LFO at maximum speed, destination NONE: fully responsive. Attach and enumeration alone are harmless; the trigger is per-snapshot traffic. |
| [V01K hardware](../evidence/v01/v01k-freeze.json), [capture](../evidence/v01/v01k-freeze-live-2026-10-29.txt) | Flashed the masked-preflight fix: freeze reproduced at ~256/512 and survived 30s cable removal. **The masked-send race is falsified as the cause.** Worker elapsed 74% of elapsed at ~410 sends/s (was ~25% at ~816 sends/s): the deferral change needs a bound. |
| [Sender waits](../evidence/i03/transport-anchors-154.json), [send path](../mods/usb-midi/usb_midi.c) | Source fact: `0x40004990` spins in two unbounded waits (`0x400049D0` wrap, `0x400049FE` capacity) drained only by the completion interrupt at `0x40004404`. The in-mask preflight fix is kept for correctness, not as this bug's fix. |
| Regression value | Host test asserts the native sender is never entered past either limit; it fails on the pre-fix adapter and passes now. [Emulator](../evidence/v01/masked-preflight-fix-emulator.json): one session, gate1, drained queue, continuous audio. |
| Index guard (reverted) | An early "stay below `0xFFF`" guard stalled traffic: only our own sends advance the staging index, so a refusal band below `0xFBF` never ends. The native wrap at `0x400049C2` already covers it. Do not re-add. |
| [G 32/32](../evidence/v01/compact-baseline.json), [64/32](../evidence/v01/compact-speed-result.json), [64/48](../evidence/v01/compact-depth48-result.json) | Short baseline stable; slight sluggishness at 64/32; quite laggy at 64/48 despite Pi fault/guards0 and FIFO11 unchanged; ~816 dense sends/s. |
| [Lower worker priority](../evidence/v01/phase2-v01p-result.json) | Improved UI but caused late audio/FIFO failures; rejected. |
| [No-self-wake](../evidence/v01/gate-a-v01c-dense.json) | Did not solve lag. |
| [Compact snapshots](../evidence/v01/compact-candidate.json) | 61→45 SysEx bytes without losing events; lag persisted. Requires the matching Pi decoder. |
| [Native MIDI](../evidence/v01/native-control-component.json), [audio](../evidence/v01/native-audio-component.json) | Native descriptor/ISR/RX/ACK and periodic audio paths work in the model; they still do not reproduce the physical lag. |

Lag grows with effective update rate; CPU saturation is not established.
Worker/render brackets overlap: never add them as CPU. Periodic UI ticks do not
measure panel-input latency. Historical D/N/P values were inflated by46.875×;
use the [clock correction](../evidence/v01/diagnostic-clock-correction.json).
Earlier narratives remain in Git at `55ccf55`.

## Emulator profile and reproduction

`tools/v01_contention_probe.py`: native LFO/panel/RTOS with `hle=False`,
scheduled panel DMA/IRQs, native USB MIDI/ACK, periodic USB and optional audio
queues. The portable peer returns one silent88-byte OUT per seven-frame368-byte
IN, following H01. Completion metadata writes `FRINDEX >> 3`; the old microframe
behaviour produced136 gaps and two descriptor faults in120ms and is covered by a
regression. Semantics follow
[NXP MCF54418RM](https://www.nxp.com/docs/en/reference-manual/MCF54418RM.pdf)
24.3.2, 24.3.4 and 24.5.2.2.3.

Fixed500ms audio+periodic+MIDI profiles, eight panel edges each (candidate G):

| Assumed core | SPD/DEP | Snapshots | Audio IN/return | Max panel service |
| --- | --- | --- | --- | --- |
| 250MHz |32/32|119|3432|3.237ms|
| 250MHz |~64/48|354|3432|3.775ms|
| 200MHz |32/32|119|3431|7.280ms|
| 200MHz |~64/48|353|3431|8.657ms|

All have750 render blocks, continuous audio, matching return credits,
gate1/session1, no decoder mismatch or artificial wait. Output stays
`Gate A NOT SCORED`: dispatch-pass completion is not visible-screen latency.
The model omits external DTIM2 input, cache/DDR/USB contention, enumeration,
real Pi scheduling and the complete physical project. Snapshot setup uses
shortcuts; native replay does not validate that history. The assumed132MHz
diagnostic timer is not proof of CPU frequency. `i03_integrated_probe.py`
passes use synthetic completions and cannot establish responsiveness.

```sh
PYTHONPATH=private/r03-runtime:deps/digiemu \
  deps/digiemu/.venv/bin/python tools/v01_contention_probe.py \
  --candidate private/v01-compact-g --usb-device --usb-peer \
  --usb-periodic --usb-audio --profile-execution --duration-ms 500 \
  --fsys 200000000 --output private/v01/native-audio.json
```

Use `--fsys 250000000` for the other core assumption.

## Acceptance gates

### Gate A — responsive dense modulation

Demonstrate the causal fix, preserve its regression, then qualify one paired
candidate on the formerly failing workload with usable UI, no audio stop or new
deadline/order/overflow/FIFO fault, bounded queues and intact guards. Record the
full profile, effective updates/s and delivery slack. Preserve ordered STARTs,
timestamps, full state and fail-closed deadlines; no event dropping, ACK-policy
or latency changes without an explicit contract and validation. Audio callbacks
must remain allocation-free and nonblocking.

### Gate B — native persistence and edit semantics

The Digitakt owns saved state; the Pi rebuilds transient state from the stream.
Use distinguishable values in a disposable project and record before/action/
after, restored state, output and unintended dirty changes. Verify persistence
once without the Pi so host caches cannot hide failures.

Cover project save/reload/cold boot; sound save/load; track/pattern copy-paste;
supported undo/reload; mute/unmute and native tails; sound locks and base-sound
restoration; switches during play; and copying USB1 to another track (supported
or explicitly safe/unsupported). Preserve neighboring tracks and START identity.
Invalidate sessions/queues where stale events could survive; do not require a
handshake for every ordinary edit. Every applicable case needs evidence.

### Gate C — recovery and combined qualification

Freeze firmware/kernel hashes and a numeric profile: LFO destination and all
settings, tempo, notes/velocity/locks and stock track load. Check32/32, the prior
64/64 failure and the worst supported workload; expand only for a specific
boundary question. Proposed UI criteria: panel/Play/Stop within100ms wall time
and trial UI-service gaps≤100ms, excluding boot/project loads. Validate baseline
and record any revision before scoring. Recheck I04's accepted ~11.4ms relative
onset timing and disclose latency tradeoffs.

Test USB pull/replug during PCM, Pi reboot and Digitakt restart separately,
three times each; stop on a failed cycle. Old PCM/ACKs/notes must not cross a
session: require fresh session, START and matching ACTIVATE. Then run one
10-minute combined soak with notes, locks, UI edits, mutes and state changes.
Record counters, queue/slack maxima, guards, Pi costs and human listening/UI
observations; require zero new faults, underruns or dropped/inserted blocks.
A shorter human-accepted run is a limited result. V01 exits only after A–C,
explicit listening acceptance, reproducible patches and verified rollback.

## Installed hardware and recovery

- **V01K is installed and has frozen; restore I04R before normal use.** Fixes
  live in `private/` and are not deliverables until replayed as patches.
- Pi kernel SHA256 `2b027a172e089af8fc273e4b2351d7fd72a7223b92f8ac6dfc8b20c491e2df5f`
  (build `Oct 29 2026 12:31:45`), unchanged by V01K, which is firmware-only.
- Agent Pi HTTP: `http://192.168.1.15`; POST `/cmd` form `c=trx=off`,
  `trxtrace=off`, `trxstatus` or `v01status`; GET `/status`, `/log`.
  Muting leaves synthesis running. Do not dump full traces during timing tests.
  Playout/analyzer delay is128 frames.
- Packages: `private/v01/firmware-V01K/` (candidate + I04R/I04Q/H02F/stock
  recoveries, all hash-verified); pre-G Pi backups in
  `private/v01/sd-backup-before-compact/`. Restore I04R
  (`6a04ae9dd7791c7b11489af0061ea0012464922d5b6ca6935450f2b712f7fa2b`) before
  normal use. Human handles Transfer/SD insertion.
- Follow `AGENTS.md` for lowered speakers, backups and hardware boundaries.
  Firmware, projects and raw captures stay private. No new flash is ready.
