# V01 — one-voice qualification

**IN PROGRESS; unaccepted.** Ownership: [TASKBOARD.md](../TASKBOARD.md).
Target: Digitakt mk1 OS 1.54 + Pi 4B/Circle, one USB1 voice on track 1.
Gate A resolves dense-LFO UI lag; Gates B/C qualify persistence and recovery.
A hard freeze at maximum LFO speed is reproduced; the USB attach path is the
suspect but the send-versus-attach split is not yet resolved. No root cause is
proven and no fix exists yet. More voices and Teensy remain deferred.

## Current next work — one discriminating hardware test

1. Run the [2K attach test](../evidence/v01/2k-attach-freeze.json): power-cycle
   the Digitakt, keep SPD down, reconnect USB, select USB1, then set the LFO to
   maximum speed with destination OFF so the hook emits nothing. ~30 s, no
   flash. Responsive ⇒ send path/volume; frozen ⇒ attach/enumeration or the
   ISR-context RX path. Report both outcomes before any candidate work.
2. If it is the send path, replace the interrupt-masked native submission with
   a bounded path that never enters a busy wait with IRQs masked, and keep a
   host regression that fails when it does. Validate offline first.
3. Keep DTIM2's external rate unresolved: firmware selects T2IN, but does not
   establish the board signal. Snapshot PLL registers are zero; do not infer a
   hardware core clock from them. Do not invent clock rates or memory delays.

## Superseded next work — emulator reproduction of the slow case

Kept for the record; deprioritised until the freeze above is explained. The
window described here remains the plan once the hard stop is fixed.

1. Prepare one bounded diagnostic measurement: does render preemption explain
   the physical worker wall-time growth? Capture per-batch elapsed and nested
   render time, summarized as totals/maxima and overlap count. If subtracting
   render time removes most excess, investigate scheduling; otherwise investigate
   worker execution/other preemption. Validate firmware instrumentation offline
   before any flash, then use one short muted fixed-profile capture, stopping
   on new faults/FIFO growth or sufficient lag evidence. Retrieve after timing
   stops; no sweep/full trace dump. No candidate is staged yet.
Investigate in software first. Do not fit synthetic delays, repeatedly install
speculative firmware, or request broad knob sweeps. Another hardware test must
answer a specific hypothesis with a short procedure and stop condition.

## Established findings

| Evidence | Finding |
| --- | --- |
| [2K attach freeze](../evidence/v01/2k-attach-freeze.json), [live capture](../evidence/v01/2k-freeze-live-2026-10-29.txt) | At maximum LFO speed the Digitakt is fully responsive with USB detached and freezes the instant the cable is attached; the freeze survives detaching and needs a power cycle. Last healthy counters were small (UI gap34ms, batch4ms, guards0) and then all Digitakt counters stopped together, so this is a stop, not saturation. Native 2K LFO cost is therefore excluded. |
| [Sender waits](../evidence/i03/transport-anchors-154.json), [send path](../mods/usb-midi/usb_midi.c) | Source fact: `fw_send` masks all interrupts and calls the native sender, whose two busy waits are drained only by the completion interrupt. Entering a wait with IRQs masked cannot terminate. Fits the signature; not yet proven to be what happened. |
| [G 32/32](../evidence/v01/compact-baseline.json), [64/32](../evidence/v01/compact-speed-result.json), [64/48](../evidence/v01/compact-depth48-result.json) | Short baseline stable; slight sluggishness at 64/32; quite laggy at 64/48 despite Pi fault/guards0 and FIFO11 unchanged. Dense native sends reached ~816/s. |
| [Lower worker priority](../evidence/v01/phase2-v01p-result.json) | Improved UI but caused late audio/FIFO failures; rejected. |
| [No-self-wake](../evidence/v01/gate-a-v01c-dense.json) | Did not solve lag. USB removal subsequently restored responsiveness; this changes several workloads together and does not identify the cause. |
| [Compact snapshots](../evidence/v01/compact-candidate.json) | G reduced full snapshots from61 to45 SysEx bytes without losing events, timestamps or state; physical lag persisted. Requires the matching Pi decoder. |
| [Native MIDI component](../evidence/v01/native-control-component.json) | Native descriptor→ISR209→ISR134→RX job→ACK paths work without context-injected replies. |
| [Native audio component](../evidence/v01/native-audio-component.json) | Periodic USB, native audio DMA and MIDI together still do not reproduce physical lag under the tested model assumptions. |

Lag grows with effective update rate, but CPU saturation remains a hypothesis.
Worker/render elapsed brackets overlap: never add them as CPU utilization.
Periodic UI ticks do not measure panel-input latency. Historical D/N/P displayed
milliseconds were inflated by46.875×; use the
[clock correction](../evidence/v01/diagnostic-clock-correction.json), not the old
JSON time labels. Earlier trial narratives and implementation audits remain in Git at `55ccf55`.

## Current emulator result and reproduction

`tools/v01_contention_probe.py` uses native LFO/panel/RTOS execution with
`hle=False`, scheduled panel DMA/IRQs, native USB MIDI/ACK processing, periodic
USB events and optional native audio queues. The portable peer returns one
silent88-byte OUT packet for each seven-frame368-byte IN packet, following H01.
Native descriptor completion regressions cover full, short and chained transfers.

Model repairs preserve dTD IOC and the page address, and write a USB frame
number (`FRINDEX >> 3`) rather than a microframe into completion metadata.
The old frame behavior produced136 packet gaps/two descriptor faults in120ms;
the regression detects it. Corrected runs have zero gaps/model faults.
USB timer and descriptor semantics follow
[NXP MCF54418RM](https://www.nxp.com/docs/en/reference-manual/MCF54418RM.pdf),
sections24.3.2,24.3.4 and24.5.2.2.3.

Fixed500ms audio+periodic+MIDI profiles, eight panel edges each:

| Assumed core | SPD/DEP | Snapshots | Audio IN/return packets | Max panel service |
| --- | --- | --- | --- | --- |
| 250MHz |32/32|119|3432|3.237ms|
| 250MHz |~64/48|354|3432|3.775ms|
| 200MHz |32/32|119|3431|7.280ms|
| 200MHz |~64/48|353|3431|8.657ms|

All have750 render blocks, continuous audio counters, matching return credits,
gate1/session1, no decoder mismatch, artificial worker wait or modeled render
overrun. 23 focused V01 tests pass. Periodic-only dense panel maximum was3.240ms.
Diagnostic DTCN0 reads now use instruction-position time within precharged
blocks: the regression changed from0 to the expected97 cycles between two
native reads. The [replayable patch](../patches/digiemu/README.md) passes28
clock/checkpoint tests and leaves legacy stepping unchanged. The corrected
200MHz run still has no physical lag reproduction.
[Cost comparison](../evidence/v01/clock-cost-comparison.json): masked native sends
average8.6–8.8µs in the model versus14.9–15.1µs in existing hardware captures.
The native worker-span observer matches firmware elapsed totals exactly and
leaves all prior per-profile measurements unchanged. Baseline/dense runs have
138/372 complete batches; only2 overlap rendering in each. Maximum non-render
time stays94.3/94.2µs, versus~626µs maximum elapsed including rendering.
Non-render time still includes other preemption and is an upper bound on CPU.
Counter-wrap and post-end-read preemption tests pass (23 V01 checks total).
Hardware's normalized worker elapsed rises121–129→304–344µs/send while masked
send cost remains~15µs. This discrepancy motivates the single measurement
above; it does not prove the physical culprit or CPU saturation.

```sh
PYTHONPATH=private/r03-runtime:deps/digiemu \
  deps/digiemu/.venv/bin/python tools/v01_contention_probe.py \
  --candidate private/v01-compact-g --usb-device --usb-peer \
  --usb-periodic --usb-audio --profile-execution --duration-ms 500 \
  --fsys 200000000 --output private/v01/native-audio.json
```

Use `--fsys 250000000` for the other core assumption. Output remains
`Gate A NOT SCORED`: dispatch-pass completion is not visible-screen latency.
The model omits external DTIM2 input, physical cache/DDR/USB contention, full
USB enumeration, real Pi scheduling and the complete physical project.
Snapshot setup uses shortcuts; native replay does not validate that history.
The assumed132MHz diagnostic bus timer is not proof of CPU frequency.
Functional `i03_integrated_probe.py` passes use synthetic completions and
context-injected replies and cannot establish responsiveness.

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

- G Digitakt and matching Pi installed; [boot verified](../evidence/v01/compact-boot.json).
  Pi kernel SHA256: `2b027a172e089af8fc273e4b2351d7fd72a7223b92f8ac6dfc8b20c491e2df5f`.
  Last capture: muted, trace off, fault0/FIFO11. USB unplug/recovery confirmation
  is pending; both trial watchers are stopped. Boot→active FIFO0→11 is unresolved.
- Agent Pi HTTP: `http://192.168.1.15`; POST `/cmd` form `c=trx=off`,
  `trxtrace=off`, `trxstatus` or `v01status`; GET `/status`, `/log`.
  Muting leaves synthesis running. Do not dump full traces during timing tests:
  printing previously caused underruns. Playout/analyzer delay is128 frames.
- Recovery packages: `private/v01/firmware-V01G/`; pre-G Pi backups:
  `private/v01/sd-backup-before-compact/`. I04R recovery:
  `private/v01/firmware-V01P/Digitakt-I04R-previous.syx`, SHA256
  `6a04ae9dd7791c7b11489af0061ea0012464922d5b6ca6935450f2b712f7fa2b`.
- Restore I04R before normal use. Human handles Transfer/SD insertion. Follow `AGENTS.md` for lowered speakers,
  backups and hardware boundaries. Firmware, projects and raw captures stay
  private; dependency changes require replayable patches. No new flash is ready.
