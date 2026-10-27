/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "dt1_transport.h"

#define barrier() __asm__ __volatile__ ("" ::: "memory")

/* Freestanding: the firmware build has no libc. */
static void copy_bytes(void *dst, const void *src, unsigned n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--)
        *d++ = *s++;
}

static void zero_bytes(void *dst, unsigned n)
{
    uint8_t *d = dst;
    while (n--)
        *d++ = 0;
}
#define vload(p) (*(p))
#define vstore(p, v) (*(p) = (v))

void dt1_transport_init(struct dt1_transport *t, const struct dt1_fw_ops *fw,
                        void *fw_ctx)
{
    if (!t)
        return;
    zero_bytes(t, sizeof *t);
    if (fw)
        t->fw = *fw;
    t->fw_ctx = fw_ctx;
    usbmidi_bytes_init(&t->rx, t->rx_buf, sizeof t->rx_buf);
    sysex_split_init(&t->sysex, t->sysex_buf, t->sysex_tags,
                     sizeof t->sysex_buf);
}

/* The firmware calls the installed slot as (bytes, count) with its own
 * context, so the slot must be a function; the transport arrives as the
 * handler context we register beside it. */
static void dt1_rx_trampoline(void *ctx, const uint8_t *data, uint32_t n)
{
    dt1_rx_bytes(ctx, data, n);
}

int dt1_transport_open(struct dt1_transport *t)
{
    if (!t || !t->fw.set_rx_handler)
        return 0;
    t->prev_rx = t->fw.set_rx_handler(t->fw_ctx, (void *)dt1_rx_trampoline, t,
                                      &t->prev_rx_ctx);
    return 1;
}

void dt1_transport_close(struct dt1_transport *t)
{
    if (!t || !t->fw.set_rx_handler)
        return;
    t->fw.set_rx_handler(t->fw_ctx, t->prev_rx, t->prev_rx_ctx, &t->prev_rx_ctx);
    t->prev_rx = 0;
}

/* ---- render: stage only -------------------------------------------------- */

enum dt1_tx_result dt1_tx_stage(struct dt1_transport *t,
                                const uint8_t *msg, uint32_t n)
{
    struct dt1_tx_slot *s;
    uint32_t head, tail;
    if (!t || !msg || !n)
        return DT1_TX_DROPPED;
    if (!vload(&t->link))
        return DT1_TX_LINK_DOWN;
    if (n > DT1_MSG_MAX) {
        ++t->tx_overflow;
        ++t->dropped;
        return DT1_TX_DROPPED;
    }
    head = vload(&t->tx_head);
    tail = vload(&t->tx_tail);
    if (head - tail >= DT1_TX_RING) {
        ++t->tx_overflow;
        ++t->dropped;
        return DT1_TX_DROPPED;
    }
    s = &t->tx_ring[head & (DT1_TX_RING - 1u)];
    s->len = n;
    copy_bytes(s->msg, msg, n);
    barrier();
    vstore(&t->tx_head, head + 1u);
    ++t->staged;
    return DT1_TX_OK;
}

/* ---- task: submit and drain --------------------------------------------- */

static int submit(struct dt1_transport *t, const uint8_t *msg, uint32_t n)
{
    uint32_t now;
    if (!t->fw.link_up || !t->fw.can_send || !t->fw.send)
        return 0;
    if (!t->fw.link_up(t->fw_ctx))
        return 0;
    /* Re-check against the live counters: the firmware must not spin. */
    if (!t->fw.can_send(t->fw_ctx, n))
        return 0;
    now = t->fw.send(t->fw_ctx, msg, n);
    if (now != n) {
        ++t->submit_failures;
        return 0;
    }
    ++t->submitted;
    return 1;
}

uint32_t dt1_tx_service(struct dt1_transport *t, uint32_t budget)
{
    uint32_t done = 0;
    if (!t)
        return 0;
    while (done < budget) {
        uint32_t head, tail;
        const struct dt1_tx_slot *s;
        head = vload(&t->tx_head);
        barrier();
        tail = vload(&t->tx_tail);
        if (head == tail)
            break;
        s = &t->tx_ring[tail & (DT1_TX_RING - 1u)];
        if (!submit(t, s->msg, s->len)) {
            ++t->deferrals;
            break;              /* keep order: retry this message next tick */
        }
        barrier();
        vstore(&t->tx_tail, tail + 1u);
        ++done;
    }
    return done;
}

int dt1_link_update(struct dt1_transport *t, int up)
{
    if (!t)
        return 0;
    if (!up) {
        uint32_t head, tail;
        head = vload(&t->tx_head);
        tail = vload(&t->tx_tail);
        if (head != tail)
            t->tx_discarded += head - tail;
        /* Stale traffic must never cross a reconnect: reset both indices and
         * the receive state. Render sees link == 0 until the task reopens. */
        vstore(&t->tx_head, 0);
        vstore(&t->tx_tail, 0);
        usbmidi_bytes_init(&t->rx, t->rx_buf, sizeof t->rx_buf);
        sysex_split_init(&t->sysex, t->sysex_buf, t->sysex_tags,
                     sizeof t->sysex_buf);
        vstore(&t->link, 0);
        return 0;
    }
    vstore(&t->link, 1);
    return 1;
}

/* ---- receive ------------------------------------------------------------ */

/* Called from the firmware's USB completion context (0x40004808): bounded,
 * allocation-free, no firmware calls. */
void dt1_rx_bytes(struct dt1_transport *t, const uint8_t *data, uint32_t n)
{
    uint32_t i;
    uint8_t kept[64];
    uint32_t k = 0;
    if (!t || (!data && n))
        return;
    t->rx_bytes += n;
    /* USB-MIDI carries SysEx in 4-byte CIN 0x4 groups and pads the final
     * group with zeroes: OS 1.54's own sender does that (0x40004BEC..0x40004C10)
     * and its receiver copies the group's three data bytes without a bit-7
     * test. Nothing may follow EOX inside a packet, so at most two zeroes
     * directly after an F7 are framing, not MIDI. Drop them and nothing else;
     * a third zero is passed on for the stock parser to judge. */
    for (i = 0; i < n; ++i) {
        uint8_t b = data[i];
        if (t->rx_pad && b == 0x00) {
            --t->rx_pad;
            ++t->rx_padding;
        } else {
            t->rx_pad = b == 0xf7 ? 2u : 0u;
            kept[k++] = b;
        }
        if (k == sizeof kept) {
            usbmidi_rx_put(&t->rx, kept, k);
            k = 0;
        }
    }
    if (k)
        usbmidi_rx_put(&t->rx, kept, k);
}

struct dt1_rx_sink {
    struct dt1_transport *t;
};

static int accept_cb(void *ctx, const uint8_t *frame, uint32_t n)
{
    struct dt1_rx_sink *s = ctx;
    struct dt1_transport *t = s->t;
    return t->accept ? t->accept(t->cb_ctx, frame, n) : 0;
}

static void frame_cb(void *ctx, const uint8_t *frame, uint32_t n)
{
    struct dt1_rx_sink *s = ctx;
    struct dt1_transport *t = s->t;
    ++t->rx_frames;
    if (t->on_sysex)
        t->on_sysex(t->cb_ctx, frame, n);
}

static void other_cb(void *ctx, uint8_t b)
{
    struct dt1_rx_sink *s = ctx;
    struct dt1_transport *t = s->t;
    /* Ordinary traffic (and any SysEx that is not ours) goes to the stock
     * handler we chained, byte for byte and in order. */
    ++t->rx_chain_bytes;
    if (t->chain)
        t->chain(t->cb_ctx, &b, 1);
}

uint32_t dt1_rx_service(struct dt1_transport *t, uint32_t budget)
{
    struct dt1_rx_sink sink;
    uint8_t buf[64];
    uint32_t used, total = 0;
    if (!t)
        return 0;
    sink.t = t;
    used = usbmidi_bytes_used(&t->rx);
    if (used > budget)
        used = budget;
    while (total < used) {
        uint32_t n = used - total < sizeof buf ? used - total : sizeof buf;
        n = usbmidi_rx_get(&t->rx, buf, n);
        if (!n)
            break;
        sysex_split_feed(&t->sysex, buf, n, accept_cb, frame_cb, other_cb, &sink);
        total += n;
    }
    t->rx_oversize = t->sysex.oversize;
    return total;
}
