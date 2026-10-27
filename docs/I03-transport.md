# I03 — USB-MIDI transport adapter (Digitakt mk1 OS 1.54)

Status: **adapter verified in host tests and compiled OS 1.54 execution**.
Nothing flashed; no hardware audio or live USB test. Integrated with the
handshake work in [I03-src-control-link.md](I03-src-control-link.md): this
document covers the byte transport, which the session handshake rides on.

## What this is

The Digitakt has a vendor USB-MIDI path. OS 1.54 exposes a raw-MIDI sender and
a receive callback, but the sender contains capacity/wrap **busy waits** and
its queue only drains from a USB completion. This adapter owns that
interaction so no other code can spin, block or deadlock in it:

| layer | file | runs in |
| --- | --- | --- |
| rules, queues, de-framing | `control/dt1_transport.c`, `control/usbmidi.c`, `control/sysex_split.c` | device-neutral, host-testable |
| OS 1.54 firmware anchors | `mods/usb-midi/usb_midi.c` | UI task (`ev_tick`) and USB completion |
| firmware model (tests only) | `control/dt1_usbmidi_model.c` | host |
| host tests | `tests/dt1_transport.c`, `scripts/test_i03_transport.sh` | host |
| static anchor check | `tools/i03_transport_anchor.py` | host, private firmware |

Message framing is **not** here. This layer moves bytes and reassembles
complete `F0..F7` frames; the v1 START snapshot and the 24-byte v2 session
control frames belong to [`control/link.*`](I03-src-control-link.md).

## Transport contract

1. **Render stages, the task submits.** `dt1_tx_stage()` is the only entry
   allowed at interrupt level. It copies one whole message into a fixed
   64-slot ring and returns. It performs no firmware call, no descriptor
   access and never waits. `dt1_tx_service()` runs from `ev_tick` (order 30),
   and only there does the firmware's sender get called.
2. **Never enter a firmware wait.** Before every submission the adapter
   re-checks `can_send()` against the live counters: the sender must be ready
   (`0x40610474 == 1`), the USB ISR must be on, `queued < 0xFFB`, and for a
   message longer than `0x2F` bytes the wrap condition must not hold. If any
   check fails the message stays at the head of the ring and the tick retries.
   Order is preserved; nothing is skipped.
3. **Whole messages only.** A message that does not fit is refused whole and
   counted (`dropped`, `tx_overflow`). No partial frame can ever reach the
   wire, so a truncated START cannot be mistaken for a short one.
4. **Bounded every step.** 64 staged messages, 96 bytes each, up to 64 submissions
   and 4096 received bytes per service call, 4096-byte receive ring, 1024-byte frame
   buffer. Any overflow is reported, never silently trimmed.
5. **Completion is the only drain.** The adapter never advances the firmware's
   queue itself, never writes `0x40610478`, and never re-uses a buffer whose
   completion has not arrived. The firmware owns staged bytes until its
   callback lowers the counter.
6. **Receive is chained, not stolen.** `dt1_transport_open()` installs one
   trampoline in `0x4061046C` and remembers the previous handler. In the
   completion context the adapter only appends to a byte ring. In the task it
   splits the stream: `F0 7D 'D' 'T' 'S' 'C'` frames (versions 1 and 2) go to
   `on_sysex`; every other byte, and every SysEx that is not ours, is replayed
   to the stock handler in original order. Real-time bytes inside somebody
   else's SysEx are held in place so they cannot overtake it.
7. **Disconnect is a hard reset.** `dt1_link_update(0)` discards staged
   messages, clears the receive ring and any partial frame, and clears the
   link flag. Staging refuses until the link is observed up again. A new
   session therefore cannot inherit stale bytes from the old one.
8. **Ordinary MIDI keeps working.** Non-DTSC SysEx, channel messages, clock
   and transport all reach the stock parser byte for byte. The adapter adds
   no bytes and reorders none; it withholds only complete DTSC frames.

### OS 1.54 facts the contract rests on

Hash-pinned in [evidence/i03/transport-anchors-154.json](../evidence/i03/transport-anchors-154.json),
reproduced by `tools/i03_transport_anchor.py`:

| Fact | Location |
| --- | --- |
| Sender gate on ready flag and ISR | `0x400049A2`, `0x400049AC` |
| Wrap path only for messages > `0x2F` bytes | `0x400049B6` |
| Wrap busy wait | `0x400049D0..0x400049D8` |
| Staging ring, base and index | `0x400049E0..0x400049E6` (`0x4BA84000`, `0x4061047C`) |
| Per-byte capacity busy wait, limit `0xFFB` | `0x400049FE..0x40004A0C` |
| Completion is the only decrement of `0x40610478` | `0x40004404..0x40004416` |
| Receiver per-packet copy length table (CIN) | `0x400047A8..0x400047D8`, table at `0x4017CE70` |
| Callback slots, cable 0 and 1 | `0x4061046C`, `0x40610470`, set at `0x400045AA`/`0x400045B4` |
| Call with `(bytes, count)` | `0x40004808` |
| UI tick site the adapter subscribes to | `0x4000A770` |

Two consequences are easy to get wrong and are handled explicitly:

- The receive copy applies **no bit-7 test** and copies `table[cin]` bytes, so
  a SysEx group's `F0`/`F7` are ordinary payload. The adapter's parser
  follows the same table instead of refusing status-looking data.
- Because groups are always three data bytes, senders pad the last group with
  zeroes after `F7`, and a 2-byte message leaves one. The native CIN decoder removes padding. The adapter receives decoded MIDI
  bytes and preserves foreign zero bytes; it does not guess at padding after
  an `F7`.

## Evidence

Host tests — 11 groups, ASan/UBSan clean, `bash scripts/test_i03_transport.sh`:

- packet CIN/status mapping and refusals, including `F0`/`F7` as CIN 0x4 data;
- whole-message packetization and whole-message refusal;
- render stages only, with a firmware stand-in that fails the test if any
  firmware call or send happens from render;
- bounded queue with deferral instead of a busy wait, and progress driven only
  by completions;
- 400-message congestion: bounded staging, whole-message drops, order kept,
  both modelled busy waits at zero;
- disconnect: staged and partial traffic discarded, staging refused until a
  fresh link, stock handler restored on close;
- receive chaining: our frame withheld, a foreign SysEx and channel messages
  replayed byte-exactly with and without interleaved real-time bytes, frames
  spanning two completion callbacks;
- oversize frame counted and replayed intact;
- handle-wrap preflight: defers instead of spinning, then succeeds after
  completions.

Device build — `usb-midi 0.1`: `.run 5480`, `.bss 11828`, **0 sites** (it
subscribes to `ev_tick`; it hooks no stock code), 75 relocations, has no imports. Lint passes with core 4.1 enabled.

## Reusable SDK hooks

The adapter is written so its firmware half is a thin, replaceable layer. Two
hooks are deliberately candidates for elekloader's core rather than permanent
local knowledge:

1. **`ev_usb_midi_send`** — a core event exporting the safe, nonblocking
   sender (`can_send` + `send` behind one call), so no mod ever needs the
   addresses or the wait rules.
2. **`ev_usb_midi_rx`** — a chained receive event with the de-framed byte
   stream, so several mods can share the one callback slot instead of
   competing for `0x4061046C`.

Both are described here rather than implemented in core: they change another
project's public surface and are offered upstream, not taken. Until then the
addresses stay isolated in `mods/usb-midi/usb_midi.c`.

## Compiled integration and remaining physical qualification

The final compiled OS 1.54 candidate executes the real native serializer and
our two-argument RX bridge, worker, snapshot queue and render gate. The
[compiled exchange](../evidence/i03/integrated-154.json) verifies activation,
SRC updates, repeated starts, ACK loss, link loss/reconnect, sustained dense
modulation, stalled-completion congestion and recovery. Sender preflight and
submission execute in one native critical section. USB descriptor submission,
completion progress and incoming PCM are modelled at explicit boundaries;
this does not measure real DMA lifetime or cadence.

The native callback is `(bytes, count)`; the adapter supplies its own context
before calling the portable trampoline. Initially-null slots are retried, USB
reinitialization captures the new stock handler, and foreign MIDI is chained
in original order. The sender is `void`; acceptance is checked by the exact
increase in the encoded-byte queue counter. Capacity reserves the complete
USB-MIDI encoding, not the input MIDI length. Descriptor exhaustion or partial
submission latches failure rather than retrying a possibly truncated frame.

Dense unchanged controls no longer flood the queue: the render suppresses
only identical non-START records, retaining a sample every 64 blocks. Every
change and native START remains ordered. Continuous changed values still
require sustained drain; insufficient progress closes the gate. A
firmware-ready flag alone never activates audio: a fresh session needs its
matching ACTIVATE, and ACK leases bound undetected transport loss.

Physical USB throughput, cable-pull flag behavior/detection time, real native
completion cadence and hardware IRQ headroom remain unmeasured. Only cable 0
and callback slot `0x4061046C` are owned; cable 1 is untouched. Distinct
retrigger semantics remain a separate render/sequence question.

Reproduce:

```sh
bash scripts/test_i03_transport.sh
bash scripts/test_i03_integration.sh
PYTHONPATH=private/r03-runtime:deps/digiemu "$PY" \
  tools/i03_integrated_probe.py --candidate private/i03-release3 \
  --output evidence/i03/integrated-154.json
PYTHONPATH=tools:private/r03-runtime:deps/digiemu "$PY" \
  tools/i03_transport_anchor.py --sections private/i02-fixture/sections \
  --output evidence/i03/transport-anchors-154.json
```
