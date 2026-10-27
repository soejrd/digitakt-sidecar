# I03 — host session, activation ACK and silence gate

Status: **integrated control link verified in compiled OS 1.54 and production
Circle host harnesses**. Target: Digitakt mk1 OS 1.54 and Pi 4B/Circle. The
Pi 4 kernel cross-build passes. No hardware firmware, SD deployment or audio
trial occurred. See [I03 acceptance report](I03-src-control-link.md).

## Interface and ownership

The render owns final control capture and the bounded queue. The UI worker
owns device session state and native USB-MIDI submissions. The Circle task
owns parameter installation and ACK generation; its USB callback only enqueues
bytes, while the output consumer reads the published gate.

**Protocol roles are not USB roles.** In this repository:

- **device** = simulated Digitakt sender of snapshots and control frames;
- **host** = Pi/Circle receiver, parameter installer, ACK sender and PCM gate.

## Wire format

The existing **v1 61-byte START snapshot is unchanged**. Session control is an
additive **v2 24-byte SysEx frame**:

```text
F0 7D 'D' 'T' 'S' 'C' 02  kind
session(5x7)  sequence(5x7)  value(5x7)  F7
```

All numeric fields use explicit little-endian 7-bit groups. The top group of
each 5-group integer must be `<= 15`; overlong encodings are rejected. No
packed C structures cross the wire. `session` and `sequence` must be nonzero;
sequence `UINT32_MAX` is prohibited so exhaustion cannot wrap.

| Kind | Direction | Meaning |
| --- | --- | --- |
| `HELLO` (1) | device -> host | Open a fresh nonzero session |
| `HELLO_ACK` (2) | host -> device | Session accepted; no snapshot may precede it |
| `ACTIVATE` (3) | host -> device | START installed; `value` is the accepted block |
| `DEACTIVATE` (4) | either | Close; `value` is the reason |
| `KEEPALIVE` (5) | both | Liveness naming accepted session/sequence/block |

Reasons: timeout 1, sequence/order 2, overflow 3, reconnect 4, parameter
install 5, malformed 6, session 7, transport 8.

## Handshake and gate

1. Device opens with a fresh nonzero session and sends `HELLO`.
2. Host resets receiver state and queues `HELLO_ACK`; the PCM gate remains
   closed throughout the exchange.
3. Device sends the first snapshot only after `HELLO_ACK`. It must carry
   `SC_START` and its own next sequence and block.
4. Host verifies session/sequence/block, installs all parameters, and only
   then queues `ACTIVATE(session, sequence, block)`.
5. Host opens PCM only when that ACTIVATE was accepted by the nonblocking
   sender. Device opens its gate only upon receiving matching ACTIVATE.
6. Later contiguous snapshots update installed state without requiring a new
   handshake; after activation they need not carry `SC_START`. A byte-identical
   snapshot replay re-emits ACTIVATE without reinstalling; a same-sequence/block frame with a
   different payload is an order fault. While a new ACTIVATE is in flight, a
   device keepalive may name either the previous confirmed snapshot or the
   new one; any other tuple closes the gate.

Silence is fail-closed. Sequence loss/duplicate/conflict, malformed DTSC
variants, receive-queue or replay-window overflow, lease expiry, transport
failure or reconnect clears the gate. Stale sessions are remembered and
rejected; a new nonzero session is mandatory.

Timeouts use the host audio-block clock (`sc_circle_advance_block`), never
uptime. The lease is a caller policy in blocks. `deactivate` names the last
accepted sequence so the device cannot confuse a close from an older session.

## Integration shape

`control/circle_link_adapter.*` is the portable Circle-shaped core. It uses a
bounded SPSC receive ring: `sc_circle_midi_byte` is the sole callback writer;
`sc_circle_process` is the sole task reader. The callback never sends,
installs, clears PCM or waits. It latches silence immediately on overflow or
transport loss; the task then emits DEACTIVATE once. `sc_circle_audio_allowed`
is one acquire atomic read for the render path.

`control/circle_usbmidi.*` is the thin `CUSBMIDIDevice` bridge. It accepts
packet-handler MIDI bytes and queues replies through Circle's documented
`SendPlainMIDI(cable, data, size, chunk)` only during task processing.

The synthesis owner must provide `sc_install_fn` and `sc_clear_fn` callbacks:
install copies preallocated TRX-B2 parameters and only then returns 1; clear
zeros bounded stale parameter/PCM state. Synthesis policy is deliberately not
implemented here.

`mods/usb-track/worker.c` publishes the matching ACTIVATE result to the render
gate. `usb_track_render` checks it and the 750-block ACK lease, closes on
invalid route/session and clears both stale USB halves before rearming.
Circle patch 0003 installs control snapshots in `dt_link.cpp` and gates the
actual Output consumer with FIFO epochs. A late ACK cannot reopen an output
epoch closed by asynchronous transport loss. Integrated mode remains silent
until I04 supplies synthesis.

## Evidence

`scripts/test_i03_handshake.sh` compiles and runs:

- `tests/control_link.c`: 61-byte v1 codec, 24-byte v2 canonical encoding,
  malformed/truncated/overlong rejection;
- `tests/control_handshake.c`: simulated device<->host handshake, gate timing,
  duplicate replay, sequence loss, receive overflow, timeout, transport loss,
  stale reconnect and fresh-session recovery, under ASan/UBSan;
- `tests/control_circle_adapter.cpp`: Circle wrapper behavior against a
  declarations-only shim, plus a compile check of the real `SendPlainMIDI`
  signature.

Result artifact: [handshake-portable.json](../evidence/i03/handshake-portable.json).
Current result: three suites pass. This is portable evidence only.

## Integrated verification and limits

`scripts/test_i03_integration.sh` executes production device render/worker/
native adapter code with a modelled USB boundary and the portable Circle peer.
It also executes the actual Circle `dt_link.cpp`, reply queue and Output gate,
covering installation, activation, loss, reconnect, lease expiry, queue
congestion, stale ACK rejection and FIFO flushing under ASan/UBSan.

[Compiled-device evidence](../evidence/i03/integrated-154.json) runs the actual
OS 1.54 serializer and native RX bridge through the session exchange, SRC
updates, repeated starts, ACK/link loss, dense modulation and stalled native
completions. Its USB DMA/completion and incoming PCM boundary is modelled.
[Integration/build evidence](../evidence/i03/integration.json) records the
production harnesses and successful Pi 4 kernel build.

Physical DMA lifetime/cadence, cable-pull detection, USB congestion, Circle
scheduling under hardware load and integrated audio latency remain H02/I04
qualification. The kernel was built, not deployed. No external synthesis or
hardware audio result is implied by successful control-state installation.
