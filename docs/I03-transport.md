# I03 — USB-MIDI transport adapter (Digitakt mk1 OS 1.54)

Status: **adapter delivered and host-qualified; device execution unproven**.
Nothing flashed; no hardware audio or live USB test. Independent of the
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
4. **Bounded every step.** 64 staged messages, 96 bytes each, 8 submissions
   and 512 received bytes per tick, 4096-byte receive ring, 1024-byte frame
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
  zeroes after `F7`, and a 2-byte message leaves one. The adapter drops at
  most two zeroes directly after an `F7` on receive and never emits padding
  itself; anything else is passed on untouched.

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

Device build — `usb-midi 0.1`: `.run 5236`, `.bss 12836`, **0 sites** (it
subscribes to `ev_tick`; it hooks no stock code), 62 relocations, imports only
core symbols. Lint passes with core 4.1 enabled.

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

## Known gaps

- **Not executed on the device or in the emulator.** Unicorn cannot map
  executable memory in this sandbox (`Illegal instruction` in `mem_map`),
  the same environment block recorded in
  [evidence/i03/dynamic-154.json](../evidence/i03/dynamic-154.json). So the
  sender's real DMA timing, the completion cadence, and the stock receive
  handlers at `0x400D3298`/`0x400D34CC` are modelled or chained but not run.
- **Drain rate is unmeasured.** Dense modulation at 1,500 snapshots/s is
  ≈126 kB/s; whether the completion path sustains it, and what happens to the
  staging ring under that load, needs a live stream.
- **No physical reconnect.** Link state is derived from the ready/ISR flags.
  Whether a cable pull actually clears them, and how long that takes, is not
  measured. A real disconnect may also need a bounded lease from the session
  layer, which lives in the handshake work.
- **One cable only.** The adapter owns cable 0 packets and slot
  `0x4061046C`. Cable 1 (`0x40610470`) is untouched.
- **No retrigger semantics.** That remains a render-ordering question, not a
  transport one.

Reproduce (host only):

```sh
bash scripts/test_i03_transport.sh
# static anchors, needs the private sections and the pinned decoder:
PYTHONPATH=tools:private/r03-runtime:deps/digiemu "$PY" \
  tools/i03_transport_anchor.py --sections private/i02-fixture/sections \
  --output evidence/i03/transport-anchors-154.json
```
