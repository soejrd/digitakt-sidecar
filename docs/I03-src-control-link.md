# I03 — SRC controls and control link

Status: **partial; transport adapter still required**. Target: Digitakt mk1
OS 1.54. Nothing flashed; no hardware audio or Pi changes.

## Delivered

- USB1 0.2 exposes PTCH, DEC, RAMP, HOLD, TICK, NOIS, DIRT, DIST on one audio
  track's SRC page, with 0–127 ranges and mdkit defaults
  `60,65,41,62,91,44,56,58`. Numeric graphics avoid inherited sample/loop
  displays; knob D cannot open the sample browser. The SDK's extended page
  ABI applies defaults through native sound updates; legacy v3 did not apply
  those defaults in the emulator. Only eight controls are displayed.
- `control/link.{h,c}` is a portable **protocol prototype**, not a running
  Digitakt/Pi link. Each 61-byte SysEx snapshot contains session, sequence,
  audio-block number, voice ID, note (16.16), velocity and eight signed 8.8
  values, plus START. Fields use explicit 7-bit encoding, never packed structs.
  Scope is the existing single-track prototype, so track identity is implicit
  as audio track 1. Adding tracks requires a protocol revision.
- START and its final parameters share one frame. The consumer must install
  parameters before starting the engine. Session/sequence checks and a block
  lease invalidate the receiver on a gap, duplicate, old session or timeout.
  Session IDs must be fresh and nonzero at reconnect. This receiver state is
  **not connected to either device PCM gating or Circle synthesis** yet.
- Host tests cover signed extrema, malformed/truncated packets, noncanonical
  encodings, ordered state, simulated loss/congestion, stale reconnect data,
  duplicate blocks, clock wrap and timeout. ASan/UBSan pass. These are local
  protocol tests, not physical USB congestion/reconnect measurements.

## Native transport finding

The SDK has no Digitakt mk1 SysEx send/receive export. The hash-checked static
probe identifies endpoint-1 control, a raw-MIDI receive callback slot
`0x4061046C`, and candidate raw-MIDI sender `0x40004990`. That sender contains
capacity/wrap **busy waits** (`0x400049D8`, `0x40004A0C`). These anchors do not
establish safe ownership or calling context. Do not call it from render,
borrow Digitone II/Rytm addresses, or place controls into unassigned audio
header bytes. [Evidence](../evidence/i03/transport-154.json).

Remaining acceptance work, in order:

1. Qualify a bounded Digitakt mk1 transport adapter, including native MIDI
   serialization, DMA payload lifetime/completion, disconnect failure paths,
   and safe receive callback chaining. Isolate reusable hooks in a core-shaped
   adapter, following elekloader's contribution instructions.
2. Capture render snapshots after locks/LFOs using `cm_src_value`, note,
   velocity and `fw_voice_start`; preserve every restart through a bounded
   queue. At 1,500 full snapshots/s the USB-MIDI payload would be about
   126 kB/s (84 USB-MIDI bytes/frame), so test dense modulation and drain rate.
3. Connect Circle receive/ACK and device silence-until-ACK gating. Invalidate
   both sides on machine change, queue overflow, timeout and reconnect;
   clear stale PCM halves before rearming. ACK must identify the new session
   and accepted sequence; a generic MIDI connection is insufficient.
4. Run compiled-device/host congestion and reconnect tests, then dynamic
   p-lock/LFO ordering tests. Distinct retrigger semantics remain unmeasured.

These are engineering gaps, not a request for hardware reconnection or a
firmware flash. I03 is not DONE; I04 remains blocked. The existing I02A 0.1
hardware candidate is separate; 0.2 builds use label **I03P** and are not a
hardware-qualified replacement.

## Dynamic render probe

[`tools/i03_dynamic_probe.py`](../tools/i03_dynamic_probe.py) extends the
compiled-device probes at the render hook (`0x40077FBA`). It records final
8.8 A-H values, note (16.16), velocity (8.8), and the `fw_voice_start` mask for
every render block, plus each scenario's final state. Its scenarios are:

- a manual baseline trigger;
- a panel-created PTCH p-lock on step 1, with an unlocked step 2 for the
  restore check;
- documented LFO sound slots seeded before a trigger (PTCH is the
  destination); and
- three separate manual triggers, retaining every start bit in block order.

The probe refuses to call a run verified unless the render assertions execute.
It runs Unicorn in a child process, so a native `mem_map` failure is recorded
instead of losing the caller. Current evidence:
[`dynamic-154.json`](../evidence/i03/dynamic-154.json).

**Result in this session: BLOCKED, not dynamic evidence.** The preflight
constructs Unicorn, then `uc.mem_map` terminates the child with SIGILL
(return code -4) before any guest code runs with the pinned patched runtime in
the execution sandbox. The evidence therefore contains no scenario values and
`verified: []`; it records the environment and candidate revisions only.

The release condition is unchanged: run the same probe in an environment
where the pinned patched Unicorn can map JIT memory, and use its output to
replace the blocked evidence. The probe makes no hardware timing claim. Even
when it runs, repeated `fw_voice_start` bits must not be described as a
distinct Digitakt retrigger event until a dedicated trace establishes those
semantics.

## Reproduce

From the repository root, with the existing private OS fixture and patched
Unicorn runtime (`$PY` is the compatible Python interpreter):

```sh
bash scripts/test_i03.sh
PYTHONPATH="$PWD/private/r03-runtime:$PWD/deps/digiemu" "$PY" \
  tools/i02_probe.py candidate --root private/i03-final \
  --stock private/i02-fixture/Digitakt_OS1.54.syx --fixture private/i02-fixture
PYTHONPATH="$PWD/private/r03-runtime:$PWD/deps/digiemu" "$PY" \
  tools/i03_probe.py --sections private/i02-fixture/sections \
  --candidate private/i03-final --output evidence/i03/transport-154.json
PYTHONPATH="$PWD/private/r03-runtime:$PWD/deps/digiemu" "$PY" \
  tools/i03_dynamic_probe.py --candidate private/i03-final \
  --output evidence/i03/dynamic-154.json
```

`bash scripts/test_i03_dynamic.sh` runs the local helper tests. Setting
`I03_RUN_DYNAMIC=1` and `PY` runs the emulator probe as above; without that
environment, expect `status: blocked_environment` rather than a PASS.

Candidate creation requires a fresh output directory. The UI probe checks the
compiled descriptor, applied defaults and each panel encoder's corresponding
final render value after a manual trigger. Inputs use paced batches as the
existing R03 probe does. It does not claim p-lock/LFO timing, native USB DMA,
DSP audio, hardware persistence or the activation handshake. The emulator
needs executable memory outside the restricted execution sandbox.
