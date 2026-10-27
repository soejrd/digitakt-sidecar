/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "host_link.h"
#include <stddef.h>
#include <string.h>

static uint32_t block_delta(uint32_t now, uint32_t then)
{
    return now - then;
}

static int session_was_closed(const struct sc_host *h, uint32_t session)
{
    uint8_t i;
    for (i = 0; i < h->recent_count; ++i)
        if (h->recent_closed[i] == session) return 1;
    return 0;
}

static int same_payload(const struct sc_frame *a, const struct sc_frame *b)
{
    unsigned i;
    if (a->voice != b->voice || a->note != b->note ||
        a->velocity != b->velocity || a->flags != b->flags) return 0;
    for (i = 0; i < 8; ++i) if (a->params[i] != b->params[i]) return 0;
    return 1;
}

static void remember_closed(struct sc_host *h, uint32_t session)
{
    if (!session) return;
    h->recent_closed[h->recent_next] = session;
    h->recent_next = (uint8_t)((h->recent_next + 1) % SC_HOST_RECENT_SESSIONS);
    if (h->recent_count < SC_HOST_RECENT_SESSIONS) ++h->recent_count;
}

static int close_gate(struct sc_host *h, uint8_t reason,
                      uint8_t out[SC_CONTROL_WIRE_SIZE])
{
    struct sc_control c;
    int encoded = 0;
    if (out != NULL && h->session &&
        h->state != SC_HOST_SILENT && h->state != SC_HOST_INVALID) {
        c.kind = SC_DEACTIVATE;
        c.session = h->session;
        c.sequence = h->last_sequence ? h->last_sequence : h->hello_sequence;
        c.value = reason;
        encoded = sc_encode_control(out, &c);
    }
    if (out != NULL && !encoded) out[0] = 0;
    remember_closed(h, h->session);
    if (h->clear != NULL) h->clear(h->ctx);
    h->state = SC_HOST_INVALID;
    h->reason = reason;
    h->pending = SC_HOST_PENDING_NONE;
    h->hello_ack_sent = 0;
    h->reply_ready = 0;
    h->pending_sequence = h->pending_block = 0;
    return encoded;
}

void sc_host_init(struct sc_host *h, sc_install_fn install,
                  sc_clear_fn clear, void *ctx)
{
    memset(h, 0, sizeof *h);
    h->state = SC_HOST_SILENT;
    h->install = install;
    h->clear = clear;
    h->ctx = ctx;
}

int sc_host_receive_control(struct sc_host *h, const struct sc_control *c,
                            uint32_t current_block,
                            uint8_t out[SC_CONTROL_WIRE_SIZE])
{
    struct sc_control reply;
    if (h == NULL || c == NULL || out == NULL) return 0;
    out[0] = 0;
    if (c->kind == SC_HELLO) {
        if (!c->session || session_was_closed(h, c->session) ||
            (h->session && h->session == c->session))
            return 0;
        if (h->session && h->state != SC_HOST_SILENT &&
            h->state != SC_HOST_INVALID)
            close_gate(h, SC_REASON_RECONNECT, NULL);
        memset(&h->rx, 0, sizeof h->rx);
        sc_reset(&h->rx, c->session);
        h->session = c->session;
        h->hello_sequence = c->sequence;
        h->hello_block = c->value;
        h->last_sequence = h->last_block = 0;
        h->last_rx_block = current_block;
        h->state = SC_HOST_HEARD;
        h->reason = 0;
        h->pending = SC_HOST_PENDING_NONE;
        h->hello_ack_sent = 0;
        h->reply_ready = 0;
        h->pending_sequence = h->pending_block = 0;
        if (h->clear != NULL) h->clear(h->ctx);
        reply.kind = SC_HELLO_ACK;
        reply.session = c->session;
        reply.sequence = c->sequence;
        reply.value = current_block;
        if (!sc_encode_control(out, &reply)) {
            close_gate(h, SC_REASON_MALFORMED, NULL);
            out[0] = 0;
            return 0;
        }
        h->reply_ready = 1;
        return 1;
    }
    if (!h->session || c->session != h->session || h->state == SC_HOST_INVALID)
        return 0;
    if (c->kind == SC_DEACTIVATE) {
        close_gate(h, c->value <= SC_REASON_TRANSPORT ? (uint8_t)c->value
                                                      : SC_REASON_MALFORMED,
                   NULL);
        return 0;
    }
    if (c->kind == SC_HELLO_ACK) return 0;
    if (c->kind != SC_KEEPALIVE) return 0;
    if (h->state != SC_HOST_ACTIVE) return 0;
    /* A device keepalive may still name the previous confirmed snapshot
     * while a new ACTIVATE is in flight. Anything else is a sequence fault. */
    if (!(c->sequence == h->confirmed_sequence &&
          c->value == h->confirmed_block) &&
        !(c->sequence == h->last_sequence && c->value == h->last_block))
        return close_gate(h, SC_REASON_SEQUENCE, out);
    h->last_rx_block = current_block;
    return 0;
}

int sc_host_receive_start(struct sc_host *h, const struct sc_frame *f,
                          uint32_t current_block,
                          uint8_t out[SC_CONTROL_WIRE_SIZE])
{
    struct sc_control c;
    if (h == NULL || f == NULL || out == NULL || !h->session ||
        f->session != h->session ||
        (h->state != SC_HOST_HEARD && h->state != SC_HOST_ACTIVE)) return 0;
    out[0] = 0;
    /* A byte-identical replay means our previous ACK may have been lost.
     * Re-emit it without reinstalling; a different frame in the same slot is
     * still a sequence fault. */
    if (h->last_sequence && f->sequence == h->last_sequence &&
        f->block == h->last_block) {
        struct sc_control replay;
        if (!same_payload(&h->rx.state, f)) {
            return close_gate(h, SC_REASON_SEQUENCE, out);
        }
        replay.kind = SC_ACTIVATE;
        replay.session = h->session;
        replay.sequence = f->sequence;
        replay.value = f->block;
        if (!sc_encode_control(out, &replay)) return 0;
        h->pending = SC_HOST_PENDING_ACTIVATE;
        h->pending_sequence = f->sequence;
        h->pending_block = f->block;
        h->last_rx_block = current_block;
        h->reply_ready = 1;
        return 1;
    }
    if (h->pending != SC_HOST_PENDING_NONE) {
        return close_gate(h, SC_REASON_OVERFLOW, out);
    }
    if (h->state == SC_HOST_HEARD && (!h->hello_ack_sent ||
        !(f->flags & SC_START))) {
        return close_gate(h, SC_REASON_SEQUENCE, out);
    }
    if (!sc_accept(&h->rx, f)) {
        return close_gate(h, SC_REASON_SEQUENCE, out);
    }
    h->last_sequence = f->sequence;
    h->last_block = f->block;
    h->last_rx_block = current_block;
    if (h->install != NULL && !h->install(h->ctx, &h->rx.state)) {
        return close_gate(h, SC_REASON_INSTALL, out);
    }
    c.kind = SC_ACTIVATE;
    c.session = h->session;
    c.sequence = f->sequence;
    c.value = f->block;
    if (!sc_encode_control(out, &c)) {
        close_gate(h, SC_REASON_MALFORMED, NULL);
        out[0] = 0;
        return 0;
    }
    h->pending = SC_HOST_PENDING_ACTIVATE;
    h->pending_sequence = f->sequence;
    h->pending_block = f->block;
    h->reply_ready = 1;
    return 1;
}

int sc_host_reply_sent(struct sc_host *h, int queued, uint8_t reason,
                       uint32_t current_block)
{
    if (h == NULL || !h->reply_ready) return 0;
    h->reply_ready = 0;
    if (!queued) {
        close_gate(h, reason ? reason : SC_REASON_TRANSPORT, NULL);
        return 0;
    }
    if (h->pending == SC_HOST_PENDING_ACTIVATE) {
        h->pending = SC_HOST_PENDING_NONE;
        if (h->state == SC_HOST_HEARD) h->state = SC_HOST_ACTIVE;
        h->confirmed_sequence = h->last_sequence;
        h->confirmed_block = h->last_block;
        h->last_rx_block = current_block;
        return 1;
    }
    if (h->state == SC_HOST_HEARD && !h->hello_ack_sent) {
        h->hello_ack_sent = 1;
        h->last_rx_block = current_block;
        return 1;
    }
    return 0;
}

int sc_host_keepalive(struct sc_host *h, uint32_t current_block,
                      uint8_t out[SC_CONTROL_WIRE_SIZE])
{
    struct sc_control c;
    if (h == NULL || out == NULL || h->state != SC_HOST_ACTIVE || !h->session)
        return 0;
    out[0] = 0;
    if (block_delta(current_block, h->last_rx_block) >= 0x80000000u) {
        close_gate(h, SC_REASON_SEQUENCE, out);
        return 0;
    }
    c.kind = SC_KEEPALIVE;
    c.session = h->session;
    c.sequence = h->last_sequence;
    c.value = h->last_block;
    if (!sc_encode_control(out, &c)) {
        close_gate(h, SC_REASON_MALFORMED, NULL);
        out[0] = 0;
        return 0;
    }
    return 1;
}

int sc_host_tick(struct sc_host *h, uint32_t current_block, uint32_t lease,
                 uint8_t out[SC_CONTROL_WIRE_SIZE])
{
    if (h == NULL || out == NULL || !lease) return 0;
    out[0] = 0;
    if ((h->state == SC_HOST_HEARD || h->state == SC_HOST_ACTIVE) &&
        block_delta(current_block, h->last_rx_block) >= lease) {
        close_gate(h, SC_REASON_TIMEOUT, out);
        return 1;
    }
    return 0;
}

int sc_host_invalidate(struct sc_host *h, uint8_t reason,
                       uint8_t out[SC_CONTROL_WIRE_SIZE])
{
    if (h == NULL || out == NULL || !h->session) return 0;
    return close_gate(h, reason ? reason : SC_REASON_TRANSPORT, out);
}

int sc_host_audio_allowed(const struct sc_host *h)
{
    return h != NULL && h->state == SC_HOST_ACTIVE;
}

const char *sc_host_state_name(const struct sc_host *h)
{
    if (h == NULL) return "none";
    return h->state == SC_HOST_ACTIVE ? "active" :
           h->state == SC_HOST_HEARD ? "heard" :
           h->state == SC_HOST_INVALID ? "invalid" : "silent";
}
