/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIDECAR_SYSEX_SPLIT_H
#define SIDECAR_SYSEX_SPLIT_H
#include <stdint.h>

/* Splits a receive byte stream into "our SysEx" and everything else without
 * losing, reordering or duplicating a byte of ordinary MIDI traffic.
 *
 * A frame cannot be judged ours until its F7 arrives, so every byte of it is
 * buffered - including real-time bytes that arrive inside it (clock and
 * transport may legally appear between any two bytes). Emitting those
 * immediately would let them overtake the frame's own bytes, which changes
 * what the stock parser sees. Instead the whole interval is kept in order and
 * resolved at F7:
 *
 *   ours      -> the frame goes to `frame`; real-time bytes from inside it are
 *                released to `other` in their original order.
 *   not ours  -> every byte of the interval, real-time included, is replayed
 *                to `other` in its original order.
 *
 * An interval longer than the buffer is flushed to `other` in order (so
 * nothing is lost or reordered) and the frame is then not ours. A non-real-time
 * status before F7 ends the interval the same way and becomes the first byte of
 * the next message. */
struct sysex_split {
    uint8_t *buf;               /* pending bytes, in arrival order */
    uint8_t *tags;              /* one bit per pending byte: 1 = real-time */
    uint32_t cap;               /* pending byte capacity */
    uint32_t len;
    uint8_t in_sysex;
    uint8_t flushing;           /* overflow: pass everything straight through */
    /* Counters. */
    uint32_t frames, accepted, replayed, oversize, malformed, realtime;
};

/* `buf` and `tags` are caller storage: cap bytes and (cap + 7) / 8 bytes. */
void sysex_split_init(struct sysex_split *p, uint8_t *buf, uint8_t *tags,
                      uint32_t cap);
void sysex_split_feed(struct sysex_split *p, const uint8_t *data, uint32_t n,
                      int (*accept)(void *ctx, const uint8_t *frame, uint32_t n),
                      void (*frame)(void *ctx, const uint8_t *frame, uint32_t n),
                      void (*other)(void *ctx, uint8_t byte), void *ctx);
/* Discard a partial interval (link loss). Its bytes are dropped with it:
 * they belong to an interrupted transfer that can never be judged. */
void sysex_split_reset(struct sysex_split *p);
#endif
