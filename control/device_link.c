/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "device_link.h"
#include <stddef.h>
#include "sc_memory.h"

static uint32_t block_delta(uint32_t now, uint32_t then)
{
    return now - then;
}

static void forget_replay(struct sc_device_link *d)
{
    sc_zero(d->replay, sizeof d->replay);
    d->head = d->count = 0;
}

static int close_session(struct sc_device_link *d, uint8_t reason,
                         uint8_t out[SC_CONTROL_WIRE_SIZE])
{
    struct sc_control c;
    int encoded = 0;
    if (out != NULL && d->session && d->state != SC_DEVICE_CLOSED &&
        d->state != SC_DEVICE_INVALID) {
        c.kind = SC_DEACTIVATE;
        c.session = d->session;
        c.sequence = d->count ? d->replay[d->head].frame.sequence : 1;
        c.value = reason;
        encoded = sc_encode_control(out, &c);
    }
    if (out != NULL && !encoded) out[0] = 0;
    d->state = SC_DEVICE_INVALID;
    d->reason = reason;
    d->audio_allowed = 0;
    forget_replay(d);
    return encoded;
}

int sc_dev_open(struct sc_device_link *d, uint32_t session, uint32_t block,
                uint8_t out[SC_CONTROL_WIRE_SIZE])
{
    struct sc_control c;
    if (d == NULL || out == NULL || !session) return 0;
    sc_zero(d, sizeof *d);
    d->session = session;
    d->next_sequence = 1;
    d->hello_block = block;
    d->last_rx_block = block;
    d->state = SC_DEVICE_WAIT_HELLO_ACK;
    c.kind = SC_HELLO;
    c.session = session;
    c.sequence = 1;
    c.value = block;
    if (!sc_encode_control(out, &c)) {
        d->state = SC_DEVICE_INVALID;
        d->reason = SC_REASON_MALFORMED;
        return 0;
    }
    return 1;
}

int sc_dev_send_start(struct sc_device_link *d, const struct sc_frame *source,
                      uint32_t block, uint8_t out[SC_WIRE_SIZE])
{
    struct sc_frame f;
    struct sc_device_replay *r;
    if (d == NULL || source == NULL || out == NULL ||
        (d->state != SC_DEVICE_WAIT_ACTIVATE && d->state != SC_DEVICE_ACTIVE) ||
        d->next_sequence == 0 || d->next_sequence == UINT32_MAX) return 0;
    if (d->count >= SC_DEVICE_REPLAY) {
        close_session(d, SC_REASON_OVERFLOW, NULL);
        return 0;
    }
    if (d->next_sequence > 1 &&
        (!block_delta(block, d->last_tx_block) ||
         block_delta(block, d->last_tx_block) >= 0x80000000u)) return 0;
    f = *source;
    f.session = d->session;
    f.sequence = d->next_sequence;
    f.block = block;
    if ((d->state == SC_DEVICE_WAIT_ACTIVATE && !(f.flags & SC_START)) ||
        !sc_encode(out, &f)) return 0;
    r = &d->replay[(d->head + d->count) % SC_DEVICE_REPLAY];
    r->frame = f;
    r->used = 1;
    ++d->count;
    ++d->next_sequence;
    d->last_tx_block = block;
    return 1;
}

int sc_dev_resend_start(struct sc_device_link *d, uint8_t out[SC_WIRE_SIZE])
{
    if (d == NULL || out == NULL || !d->count || d->state == SC_DEVICE_CLOSED ||
        d->state == SC_DEVICE_INVALID || d->state == SC_DEVICE_WAIT_HELLO_ACK)
        return 0;
    return sc_encode(out, &d->replay[d->head].frame);
}

static void acknowledge_front(struct sc_device_link *d)
{
    if (!d->count) return;
    d->head = (uint8_t)((d->head + 1) % SC_DEVICE_REPLAY);
    --d->count;
}

struct sc_device_update sc_dev_receive_control(struct sc_device_link *d,
                                               const struct sc_control *c,
                                               uint32_t current_block)
{
    struct sc_device_update u = {0};
    if (d == NULL || c == NULL || !d->session) return u;
    if (c->kind == SC_DEACTIVATE) {
        if (c->session != d->session) return u;
        u.reason = c->value <= SC_REASON_TRANSPORT ? (uint8_t)c->value
                                                   : SC_REASON_MALFORMED;
        close_session(d, u.reason, NULL);
        u.invalidated = 1;
        return u;
    }
    if (c->session != d->session) {
        close_session(d, SC_REASON_SESSION, NULL);
        u.invalidated = 1;
        u.reason = SC_REASON_SESSION;
        return u;
    }
    if (c->kind == SC_HELLO_ACK) {
        if (d->state == SC_DEVICE_WAIT_HELLO_ACK && c->sequence == 1) {
            d->state = SC_DEVICE_WAIT_ACTIVATE;
            d->last_rx_block = current_block;
        } else if (d->state != SC_DEVICE_WAIT_HELLO_ACK) {
            close_session(d, SC_REASON_SESSION, NULL);
            u.invalidated = 1;
            u.reason = SC_REASON_SESSION;
        }
        return u;
    }
    if (c->kind == SC_ACTIVATE) {
        if (!d->count || c->sequence != d->replay[d->head].frame.sequence ||
            c->value != d->replay[d->head].frame.block) {
            close_session(d, SC_REASON_SEQUENCE, NULL);
            u.invalidated = 1;
            u.reason = SC_REASON_SEQUENCE;
            return u;
        }
        if (d->state != SC_DEVICE_WAIT_ACTIVATE && d->state != SC_DEVICE_ACTIVE) {
            close_session(d, SC_REASON_SESSION, NULL);
            u.invalidated = 1;
            u.reason = SC_REASON_SESSION;
            return u;
        }
        acknowledge_front(d);
        d->accepted_sequence = c->sequence;
        d->accepted_block = c->value;
        d->state = SC_DEVICE_ACTIVE;
        d->reason = 0;
        d->audio_allowed = 1;
        d->last_rx_block = current_block;
        u.activated = 1;
        return u;
    }
    if (c->kind != SC_KEEPALIVE || d->state != SC_DEVICE_ACTIVE ||
        c->sequence != d->accepted_sequence || c->value != d->accepted_block) {
        if (c->kind == SC_KEEPALIVE && d->state == SC_DEVICE_ACTIVE) {
            close_session(d, SC_REASON_SEQUENCE, NULL);
            u.invalidated = 1;
            u.reason = SC_REASON_SEQUENCE;
        }
        return u;
    }
    d->last_rx_block = current_block;
    return u;
}

int sc_dev_tick(struct sc_device_link *d, uint32_t current_block,
                uint32_t lease, uint8_t out[SC_CONTROL_WIRE_SIZE])
{
    if (d == NULL || out == NULL || !lease) return 0;
    out[0] = 0;
    if ((d->state == SC_DEVICE_WAIT_HELLO_ACK ||
         d->state == SC_DEVICE_WAIT_ACTIVATE ||
         d->state == SC_DEVICE_ACTIVE) &&
        block_delta(current_block, d->last_rx_block) >= lease) {
        close_session(d, SC_REASON_TIMEOUT, out);
        return 1;
    }
    return 0;
}

int sc_dev_keepalive(struct sc_device_link *d, uint32_t current_block,
                     uint8_t out[SC_CONTROL_WIRE_SIZE])
{
    struct sc_control c;
    (void)current_block;
    if (d == NULL || out == NULL || d->state != SC_DEVICE_ACTIVE ||
        !d->accepted_sequence) return 0;
    c.kind = SC_KEEPALIVE;
    c.session = d->session;
    c.sequence = d->accepted_sequence;
    c.value = d->accepted_block;
    return sc_encode_control(out, &c);
}

int sc_dev_invalidate(struct sc_device_link *d, uint8_t reason,
                      uint8_t out[SC_CONTROL_WIRE_SIZE])
{
    if (d == NULL || out == NULL || !d->session) return 0;
    return close_session(d, reason ? reason : SC_REASON_TRANSPORT, out);
}

int sc_dev_audio_allowed(const struct sc_device_link *d)
{
    return d != NULL && d->audio_allowed;
}

const char *sc_dev_state_name(const struct sc_device_link *d)
{
    if (d == NULL) return "none";
    return d->state == SC_DEVICE_ACTIVE ? "active" :
           d->state == SC_DEVICE_WAIT_HELLO_ACK ? "wait-hello-ack" :
           d->state == SC_DEVICE_WAIT_ACTIVATE ? "wait-activate" :
           d->state == SC_DEVICE_INVALID ? "invalid" : "closed";
}
