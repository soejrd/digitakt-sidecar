/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "circle_link_adapter.h"
#include <string.h>

/* Hermetic shim: a real Circle build should map these to Circle's atomics or
 * the core's existing __atomic_* helpers. They are plain C11 here. */
static void gate_store(struct sc_circle_link *l, uint8_t value)
{
    __atomic_store_n(&l->gate, value, __ATOMIC_RELEASE);
}

static int gate_load(const struct sc_circle_link *l)
{
    return __atomic_load_n(&l->gate, __ATOMIC_ACQUIRE) != 0;
}

void sc_circle_init(struct sc_circle_link *l, sc_install_fn install,
                    sc_circle_send_fn send, sc_circle_clear_fn clear,
                    void *ctx, uint32_t lease_blocks)
{
    memset(l, 0, sizeof *l);
    sc_host_init(&l->host, install, clear, ctx);
    l->send = send;
    l->clear = clear;
    l->send_ctx = ctx;
    l->lease = lease_blocks;
}

static int queue_bytes(struct sc_circle_link *l, const uint8_t *bytes,
                       uint8_t size)
{
    struct sc_circle_frame *f;
    uint32_t w = __atomic_load_n(&l->rx_write, __ATOMIC_RELAXED);
    uint32_t r = __atomic_load_n(&l->rx_read, __ATOMIC_ACQUIRE);
    if (!size || size > SC_CIRCLE_SYSEX_MAX) return 0;
    if (w - r >= SC_CIRCLE_RX_QUEUE) {
        sc_circle_transport_lost(l, SC_REASON_OVERFLOW);
        return 0;
    }
    f = &l->rx[w % SC_CIRCLE_RX_QUEUE];
    memcpy(f->bytes, bytes, size);
    f->size = size;
    __atomic_store_n(&l->rx_write, w + 1, __ATOMIC_RELEASE);
    return 1;
}

void sc_circle_midi_byte(struct sc_circle_link *l, uint8_t byte)
{
    if (l == NULL) return;
    if (byte >= 0xf8) return;                 /* realtime: never inside SysEx */
    if (byte == 0xf0) {
        l->sysex_size = 0;
        l->sysex_overflow = 0;
        l->sysex[l->sysex_size++] = byte;
        return;
    }
    if (!l->sysex_size) return;
    if (byte == 0xf7) {
        if (l->sysex_overflow || l->sysex_size >= SC_CIRCLE_SYSEX_MAX) {
            sc_circle_transport_lost(l, SC_REASON_OVERFLOW);
        } else {
            l->sysex[l->sysex_size++] = byte;
            if (sc_is_dtsc(l->sysex, l->sysex_size)) {
                if (!queue_bytes(l, l->sysex, l->sysex_size))
                    sc_circle_transport_lost(l, SC_REASON_OVERFLOW);
            }
        }
        l->sysex_size = 0;
        l->sysex_overflow = 0;
        return;
    }
    if (byte & 0x80) {                         /* another status aborts it */
        l->sysex_size = 0;
        l->sysex_overflow = 0;
        if (byte == 0xf0) l->sysex[l->sysex_size++] = byte;
        return;
    }
    if (l->sysex_size >= SC_CIRCLE_SYSEX_MAX) {
        l->sysex_overflow = 1;
        return;
    }
    l->sysex[l->sysex_size++] = byte;
}

static int emit(struct sc_circle_link *l, const uint8_t *frame,
                uint32_t size)
{
    if (!l->send || !l->send(l->send_ctx, frame, size)) {
        sc_circle_transport_lost(l, SC_REASON_TRANSPORT);
        return 0;
    }
    return 1;
}

static void invalidate_task(struct sc_circle_link *l, uint8_t reason)
{
    uint8_t out[SC_CONTROL_WIRE_SIZE];
    if (sc_host_invalidate(&l->host, reason, out)) emit(l, out, sizeof out);
    gate_store(l, 0);
}

static unsigned process_control(struct sc_circle_link *l,
                                const struct sc_control *c)
{
    uint8_t out[SC_CONTROL_WIRE_SIZE];
    if (sc_host_receive_control(&l->host, c, l->block, out)) {
        int sent = emit(l, out, sizeof out);
        sc_host_reply_sent(&l->host, sent, SC_REASON_TRANSPORT, l->block);
    }
    gate_store(l, sc_host_audio_allowed(&l->host));
    return 1;
}

static unsigned process_start(struct sc_circle_link *l,
                              const struct sc_frame *f)
{
    uint8_t out[SC_CONTROL_WIRE_SIZE];
    int sent = 0;
    if (sc_host_receive_start(&l->host, f, l->block, out)) {
        sent = emit(l, out, sizeof out);
        sc_host_reply_sent(&l->host, sent, SC_REASON_TRANSPORT, l->block);
    } else {
        sc_circle_transport_lost(l, SC_REASON_TRANSPORT);
    }
    gate_store(l, sent && sc_host_audio_allowed(&l->host));
    return 1;
}

unsigned sc_circle_process(struct sc_circle_link *l, unsigned budget)
{
    unsigned done = 0;
    uint32_t r, w;
    uint8_t out[SC_CONTROL_WIRE_SIZE];
    if (l == NULL) return 0;
    if (__atomic_load_n(&l->lost_reason, __ATOMIC_ACQUIRE)) {
        uint8_t reason;
        gate_store(l,0);
        reason=__atomic_exchange_n(&l->lost_reason,SC_REASON_NONE,__ATOMIC_ACQ_REL);
        __atomic_store_n(&l->rx_read,__atomic_load_n(&l->rx_write,__ATOMIC_ACQUIRE),__ATOMIC_RELEASE);
        invalidate_task(l,reason);
        return 0;
    }
    r = __atomic_load_n(&l->rx_read, __ATOMIC_RELAXED);
    w = __atomic_load_n(&l->rx_write, __ATOMIC_ACQUIRE);
    while (done < budget && r != w) {
        struct sc_circle_frame f = l->rx[r % SC_CIRCLE_RX_QUEUE];
        struct sc_control c;
        struct sc_frame s;
        ++r;
        __atomic_store_n(&l->rx_read, r, __ATOMIC_RELEASE);
        ++done;
        if (sc_decode_control(&c, f.bytes, f.size)) {
            process_control(l, &c);
        } else if (sc_decode(&s, f.bytes, f.size)) {
            process_start(l, &s);
        } else {
            invalidate_task(l, SC_REASON_MALFORMED);
        }
    }
    if (sc_host_tick(&l->host, l->block, l->lease, out)) {
        emit(l, out, sizeof out);
        gate_store(l, 0);
    }
    {
        uint8_t lost = __atomic_exchange_n(&l->lost_reason, SC_REASON_NONE,
                                           __ATOMIC_ACQ_REL);
        if (lost) invalidate_task(l, lost);
    }
    return done;
}

void sc_circle_advance_block(struct sc_circle_link *l, uint32_t block)
{
    if (l != NULL) l->block = block;
}

void sc_circle_transport_lost(struct sc_circle_link *l, uint8_t reason)
{
    if (l == NULL) return;
    gate_store(l, 0);
    if (reason) __atomic_store_n(&l->lost_reason, reason, __ATOMIC_RELEASE);
}

int sc_circle_audio_allowed(const struct sc_circle_link *l)
{
    return l != NULL && gate_load(l) && !__atomic_load_n(&l->lost_reason,__ATOMIC_ACQUIRE);
}

unsigned sc_circle_rx_queued(const struct sc_circle_link *l)
{
    uint32_t r, w;
    if (l == NULL) return 0;
    r = __atomic_load_n(&l->rx_read, __ATOMIC_ACQUIRE);
    w = __atomic_load_n(&l->rx_write, __ATOMIC_ACQUIRE);
    return w - r;
}
