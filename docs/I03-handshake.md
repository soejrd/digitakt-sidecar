# I03 — host session, activation ACK and silence gate

Status: **portable implementation and simulated-device tests pass; not
integrated with Digitakt firmware or a live Circle link**. Target: Digitakt
mk1 OS 1.54 and Pi 4B/Circle. No hardware was flashed; no audio was played.

## Interface agreement and ownership

The wire interface and file ownership were agreed with the two peer I03
workers before either shared file changed:

| Worker | Owns |
| --- | --- |
| host-handshake (this work) | `control/link.*`, `control/host_link.*`, `control/device_link.*`, Circle adapters, handshake tests/docs/evidence |
| USB-MIDI transport | `control/usbmidi.*`, `control/dt1_*`, transport adapter, framing-agnostic complete-F0..F7 send/receive |
| SRC/render queue | `mods/usb-track/*`, queue tests, neutral ordered render records |

The transport owner accepted `usbmidi_device_send(const uint8_t*, uint32_t)`
from `ev_tick`, complete `F0..F7` frames spanning USB-MIDI packets, and fresh
nonzero sessions after loss. The queue owner kept records transport-neutral.
Neither peer edits these handshake files.

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

The I02 USB-track render hook is not modified. It must gate ready PCM on
`sc_circle_audio_allowed()` (or an equivalent single-word published gate) and
must still clear stale USB halves before rearming. That wiring remains open.

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

## Limits and release conditions

- No Digitakt firmware, native raw-MIDI sender/receiver, DMA completion or
  disconnect was exercised.
- No physical USB-MIDI congestion or reconnect was measured; the receive ring
  tests only local bounded overflow.
- No Circle kernel was linked or run; the adapter uses portable C11 atomics
  and a declarations-only Circle header shim.
- No PCM or synthesis was rendered. The gate is state only, and synthesis work
  remains separate.
- The I03 USB-track render hook is not yet wired to this gate.

Releasing the hardware/emulator parts requires the transport owner's native
adapter and a compiled-device exchange, then a separately authorized physical
test. I03 remains partial until the hook, transport and dynamic p-lock/LFO
ordering are demonstrated together.
