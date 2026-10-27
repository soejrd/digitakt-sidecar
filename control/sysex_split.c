/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "sysex_split.h"

static int tagged(const struct sysex_split *p, uint32_t i)
{
    return (p->tags[i >> 3] >> (i & 7)) & 1u;
}

static void set_tag(struct sysex_split *p, uint32_t i, int realtime)
{
    uint8_t bit = (uint8_t)(1u << (i & 7));
    if (realtime)
        p->tags[i >> 3] |= bit;
    else
        p->tags[i >> 3] &= (uint8_t)~bit;
}

static void push(struct sysex_split *p, uint8_t b, int realtime)
{
    set_tag(p, p->len, realtime);
    p->buf[p->len++] = b;
}

static void emit(struct sysex_split *p, void (*other)(void *, uint8_t),
                 void *ctx, uint8_t b)
{
    ++p->replayed;
    if (other)
        other(ctx, b);
}

/* Release the whole pending interval, in arrival order, to `other`. */
static void replay_all(struct sysex_split *p, void (*other)(void *, uint8_t),
                       void *ctx)
{
    uint32_t i;
    for (i = 0; i < p->len; ++i)
        emit(p, other, ctx, p->buf[i]);
    p->len = 0;
    p->in_sysex = 0;
}

/* The interval is ours: release its real-time bytes in order and compact the
 * remaining SysEx bytes in place. -> the frame's length. */
static uint32_t take_ours(struct sysex_split *p, void (*other)(void *, uint8_t),
                          void *ctx)
{
    uint32_t i, w = 0;
    for (i = 0; i < p->len; ++i) {
        if (tagged(p, i)) {
            emit(p, other, ctx, p->buf[i]);
        } else {
            p->buf[w] = p->buf[i];
            set_tag(p, w, 0);
            ++w;
        }
    }
    p->len = w;
    p->in_sysex = 0;
    return w;
}

void sysex_split_init(struct sysex_split *p, uint8_t *buf, uint8_t *tags,
                      uint32_t cap)
{
    if (!p)
        return;
    p->buf = buf;
    p->tags = tags;
    p->cap = cap;
    p->len = 0;
    p->in_sysex = 0;
    p->flushing = 0;
    p->frames = 0;
    p->accepted = 0;
    p->replayed = 0;
    p->oversize = 0;
    p->malformed = 0;
    p->realtime = 0;
}

void sysex_split_reset(struct sysex_split *p)
{
    if (!p)
        return;
    p->len = 0;
    p->in_sysex = 0;
    p->flushing = 0;
}

void sysex_split_feed(struct sysex_split *p, const uint8_t *data, uint32_t n,
                      int (*accept)(void *ctx, const uint8_t *f, uint32_t n),
                      void (*frame)(void *ctx, const uint8_t *f, uint32_t n),
                      void (*other)(void *ctx, uint8_t b), void *ctx)
{
    uint32_t i;
    if (!p || (!data && n))
        return;
    for (i = 0; i < n; ++i) {
        uint8_t b = data[i];
        int realtime = b >= 0xf8;

        if (p->flushing) {
            /* The interval was already too long to be ours and has been
             * replayed; pass the rest through in order until it ends. */
            emit(p, other, ctx, b);
            if (b == 0xf7 || (!realtime && (b & 0x80))) {
                p->flushing = 0;
                p->in_sysex = 0;
            }
            continue;
        }
        if (!p->in_sysex) {
            if (b == 0xf0) {
                p->in_sysex = 1;
                p->len = 0;
                push(p, b, 0);
            } else {
                emit(p, other, ctx, b);
            }
            continue;
        }
        /* Inside an interval. Real-time bytes are kept in place so that a
         * frame we do not own replays exactly as it arrived. */
        if (realtime) {
            ++p->realtime;
        } else if (b == 0xf7) {
            uint32_t len;
            if (p->len < p->cap) {
                push(p, b, 0);
                ++p->frames;
                if (accept && accept(ctx, p->buf, p->len)) {
                    ++p->accepted;
                    len = take_ours(p, other, ctx);
                    if (frame)
                        frame(ctx, p->buf, len);
                } else {
                    replay_all(p, other, ctx);
                }
                continue;
            }
            /* The F7 itself does not fit: the interval is never ours. */
            ++p->oversize;
            replay_all(p, other, ctx);
            emit(p, other, ctx, b);
            continue;
        }
        if (p->len < p->cap) {
            push(p, b, realtime);
            continue;
        }
        /* Too long to be ours: replay everything so far in order, then pass
         * the remainder of the interval through as it arrives. */
        ++p->oversize;
        replay_all(p, other, ctx);
        p->in_sysex = 1;
        p->flushing = 1;
        emit(p, other, ctx, b);
    }
}
