/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Digitakt mk1 USB-MIDI transport adapter: host tests.
 *
 * These prove the adapter's ownership, buffer lifetime, completion and
 * disconnect rules against control/dt1_usbmidi_model.c, a model written from
 * the OS 1.54 disassembly. They are not a substitute for executed firmware;
 * see docs/I03-transport.md for exactly what is and is not established. */
#include "dt1_transport.h"
#include "dt1_usbmidi_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ---- a firmware stand-in wired to the model ---------------------------- */
struct fake_fw {
    struct dt1_usbmidi_model m;
    void *handler, *handler_ctx;
    uint32_t sends, ran_from_render;
    int in_render;
    uint8_t rx_out[256];
    uint32_t rx_len;
};

static int fw_link_up(void *ctx)
{
    struct fake_fw *f = ctx;
    return f->m.ready && f->m.isr_on;
}

static int fw_can_send(void *ctx, uint32_t n)
{
    struct fake_fw *f = ctx;
    if (f->in_render)
        ++f->ran_from_render;      /* must never happen */
    if (!dt1_model_accepts(&f->m))
        return 0;
    if (n > 0x2f && f->m.index > DT1_WRAP_LIMIT &&
        f->m.queued >= f->m.index - DT1_WRAP_KEEP)
        return 0;
    return 1;
}

static uint32_t fw_send(void *ctx, const uint8_t *msg, uint32_t n)
{
    struct fake_fw *f = ctx;
    if (f->in_render)
        ++f->ran_from_render;
    ++f->sends;
    return dt1_model_send(&f->m, msg, n, 0);
}

static void *fw_set_rx_handler(void *ctx, void *handler, void *handler_ctx,
                               void **prev_ctx)
{
    struct fake_fw *f = ctx;
    void *old = f->handler;
    if (prev_ctx)
        *prev_ctx = f->handler_ctx;
    f->handler = handler;
    f->handler_ctx = handler_ctx;
    return old;
}

static const struct dt1_fw_ops ops = {
    fw_link_up, fw_can_send, fw_send, fw_set_rx_handler
};

static void fw_init(struct fake_fw *f)
{
    memset(f, 0, sizeof *f);
    dt1_model_init(&f->m);
    f->m.ready = 1;
    f->m.isr_on = 1;
}

/* The firmware's USB completion path: deliver host packets to the installed
 * handler, exactly as 0x40004808 does. */
static void fw_deliver(struct fake_fw *f, const uint8_t *packets, uint32_t n)
{
    uint8_t out[256];
    uint32_t w;
    assert(f->handler);
    w = dt1_model_receive(&f->m, packets, n, out, sizeof out);
    f->rx_len = w;
    memcpy(f->rx_out, out, w);
    ((void (*)(void *, const uint8_t *, uint32_t))f->handler)
        (f->handler_ctx, f->rx_out, f->rx_len);
}

/* ---- collected receive output ------------------------------------------ */
struct sink {
    uint8_t ours[4096];
    uint32_t ours_len;
    uint8_t chain[4096];
    uint32_t chain_len;
    uint32_t frames;
};

static int accept_ours(void *ctx, const uint8_t *f, uint32_t n)
{
    (void)ctx;
    return n >= 7 && f[1] == 0x7d && f[2] == 'D' && f[3] == 'T' &&
           f[4] == 'S' && f[5] == 'C';
}

static void on_sysex(void *ctx, const uint8_t *f, uint32_t n)
{
    struct sink *s = ctx;
    assert(s->ours_len + n <= sizeof s->ours);
    memcpy(s->ours + s->ours_len, f, n);
    s->ours_len += n;
    ++s->frames;
}

static void on_chain(void *ctx, const uint8_t *d, uint32_t n)
{
    struct sink *s = ctx;
    assert(s->chain_len + n <= sizeof s->chain);
    memcpy(s->chain + s->chain_len, d, n);
    s->chain_len += n;
}

static void wire(struct dt1_transport *t, struct sink *s)
{
    t->on_sysex = on_sysex;
    t->accept = accept_ours;
    t->chain = on_chain;
    t->cb_ctx = s;
}

static void usb_pack(uint8_t *out, const uint8_t *bytes, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i += 3) {
        uint32_t take = n - i < 3 ? n - i : 3;
        memset(out + (i / 3) * 4, 0, 4);
        out[(i / 3) * 4] = 0x4;
        memcpy(out + (i / 3) * 4 + 1, bytes + i, take);
    }
}

static void make_frame(uint8_t *f, uint32_t len, uint8_t tag)
{
    uint32_t i;
    f[0] = 0xf0;
    f[1] = 0x7d;
    f[2] = 'D';
    f[3] = 'T';
    f[4] = 'S';
    f[5] = 'C';
    f[6] = 2;
    f[7] = tag;
    for (i = 8; i < len - 1; ++i)
        f[i] = (uint8_t)((i * 7 + tag) & 0x7f);
    f[len - 1] = 0xf7;
}

/* ---- tests ------------------------------------------------------------- */

static void test_packet_layer(void)
{
    static const struct { uint8_t msg[3]; uint8_t n, cin, len; } cases[] = {
        {{0x90, 0x40, 0x7f}, 3, 0x9, 3},
        {{0x80, 0x40, 0x00}, 3, 0x8, 3},
        {{0xb0, 0x07, 0x64}, 3, 0xb, 3},
        {{0xc0, 0x05, 0x00}, 2, 0xc, 2},
        {{0xd0, 0x40, 0x00}, 2, 0xd, 2},
        {{0xe0, 0x00, 0x40}, 3, 0xe, 3},
        {{0xf8, 0x00, 0x00}, 1, 0xf, 1},
        {{0xfa, 0x00, 0x00}, 1, 0xf, 1},
        {{0xf2, 0x01, 0x02}, 3, 0x3, 3},
        {{0xf1, 0x21, 0x00}, 2, 0x2, 2},
        {{0xf3, 0x02, 0x00}, 2, 0x2, 2},
        {{0xf6, 0x00, 0x00}, 1, 0x5, 1},
    };
    struct usbmidi_packet p;
    unsigned i;
    for (i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        uint8_t raw[4];
        assert(usbmidi_packetize(raw, cases[i].msg, cases[i].n));
        assert(raw[0] == cases[i].cin);
        assert(usbmidi_parse(&p, raw));
        assert(p.cin == cases[i].cin && p.len == cases[i].len);
        assert(!memcmp(p.data, cases[i].msg, cases[i].n));
    }
    /* Refusals mirror the firmware: a nonzero header high nibble (another
     * cable) and CIN 0x0/0x1, whose firmware copy length is zero. */
    {
        uint8_t raw[4] = {0x14, 0x90, 0x40, 0x7f};
        assert(!usbmidi_parse(&p, raw));
    }
    {
        uint8_t raw[4] = {0x00, 0x90, 0x40, 0x7f};
        assert(!usbmidi_parse(&p, raw));
        raw[0] = 0x01;
        assert(!usbmidi_parse(&p, raw));
    }
    /* A SysEx continuation whose first data byte is F0/F7 is valid: OS 1.54's
     * receiver copies it without a bit-7 test (0x400047C4). */
    {
        uint8_t raw[4] = {0x04, 0xf7, 0x00, 0x00};
        assert(usbmidi_parse(&p, raw) && p.len == 3 && p.data[0] == 0xf7);
        raw[1] = 0xf0;
        assert(usbmidi_parse(&p, raw) && p.data[0] == 0xf0);
    }
    /* The packetizer refuses a length the status does not allow. */
    {
        uint8_t raw[4];
        assert(!usbmidi_packetize(raw, (const uint8_t[]){0x90, 0x40}, 2));
        assert(!usbmidi_packetize(raw, (const uint8_t[]){0x40, 0x40}, 2));
        assert(usbmidi_packetize(raw, (const uint8_t[]){0xc0, 0x05, 0x00}, 2));
        assert(raw[0] == 0xc);
    }
    puts("packet: CIN/status mapping and refusals OK");
}

static void test_message_staging(void)
{
    struct usbmidi_scratch s;
    struct usbmidi_bytes q;
    uint8_t ring[128], frame[64], got[64];
    uint32_t n;
    usbmidi_scratch_init(&s);
    usbmidi_bytes_init(&q, ring, sizeof ring);
    make_frame(frame, 40, 1);
    assert(usbmidi_scratch_push_message(&s, frame, 40, 0, 0));
    assert(s.count == 14);                 /* ceil(40/3) packets */
    assert(usbmidi_drain(&q, &s, 64) == 14);
    assert(s.count == 0);
    n = usbmidi_pop_message(&q, got, sizeof got);
    assert(n == 40 && !memcmp(got, frame, 40));
    assert(usbmidi_messages(&q) == 0);
    /* A message that does not fit is refused whole. */
    assert(!usbmidi_scratch_push_message(&s, frame, USBMIDI_SCRATCH * 3, 0, 0));
    assert(s.count == 0);
    puts("staging: whole-message packetization and refusal OK");
}

static void test_render_never_calls_firmware(void)
{
    struct fake_fw f;
    struct dt1_transport t;
    uint8_t frame[61];
    fw_init(&f);
    dt1_transport_init(&t, &ops, &f);
    assert(dt1_link_update(&t, 1));
    make_frame(frame, 61, 3);
    f.in_render = 1;
    assert(dt1_tx_stage(&t, frame, 61) == DT1_TX_OK);
    f.in_render = 0;
    assert(f.sends == 0 && f.ran_from_render == 0);
    assert(f.handler == 0);                 /* nothing installed from render */
    assert(t.submitted == 0);
    assert(dt1_tx_service(&t, 8) == 1);
    assert(f.sends == 1 && f.m.staged == 61);
    assert(t.submitted == 1);
    puts("ownership: render stages only; the task owns every firmware call OK");
}

static void test_completion_and_backpressure(void)
{
    struct fake_fw f;
    struct dt1_transport t;
    uint8_t frame[61];
    uint32_t i, queued_before;
    fw_init(&f);
    dt1_transport_init(&t, &ops, &f);
    dt1_link_update(&t, 1);
    make_frame(frame, 61, 4);
    /* Fill the firmware staging ring to its documented limit. */
    queued_before = 0;
    for (i = 0; i < 100; ++i) {
        int r = dt1_tx_stage(&t, frame, 61);
        assert(r == DT1_TX_OK);
        dt1_tx_service(&t, 1);
        if (f.m.queued != queued_before) {
            queued_before = f.m.queued;
        }
    }
    /* With the ring near full the adapter defers instead of spinning. The
     * model's counters record a would-be wait only if a call was made that
     * hit one; our preflight keeps them at zero, so any nonzero value here is
     * a real ownership regression. */
    assert(f.m.queued <= DT1_QUEUE_LIMIT);
    assert(t.deferrals > 0);
    /* Completion releases exactly what the callback reports. */
    {
        uint32_t before = f.m.queued;
        assert(dt1_model_complete(&f.m, 61) == 61);
        assert(f.m.queued == before - 61);
    }
    /* While the firmware is over its documented limit the adapter defers and
     * never calls the sender. Completion is what makes progress, so drain it
     * the way the USB ISR would until the pending message is accepted. */
    {
        uint32_t guard = 0;
        while (t.tx_head != t.tx_tail && guard++ < 200) {
            dt1_model_complete(&f.m, 61);
            dt1_tx_service(&t, 8);
        }
        assert(t.tx_head == t.tx_tail);
        assert(guard > 1);              /* deferral really happened */
    }
    assert(f.m.wrap_waits == 0);
    assert(f.m.last_stop == DT1_STOP_NONE);
    puts("completion: bounded queue, deferral instead of busy wait OK");
}

static void test_congestion_is_bounded_and_ordered(void)
{
    struct fake_fw f;
    struct dt1_transport t;
    uint8_t frame[61];
    uint32_t i, accepted = 0;
    fw_init(&f);
    dt1_transport_init(&t, &ops, &f);
    dt1_link_update(&t, 1);
    for (i = 0; i < 400; ++i) {
        make_frame(frame, 61, (uint8_t)(i & 0x7f));
        if (dt1_tx_stage(&t, frame, 61) == DT1_TX_OK)
            ++accepted;
    }
    /* The staging ring is bounded: excess messages are dropped whole and
     * counted, never truncated. */
    assert(accepted <= DT1_TX_RING);
    assert(t.tx_overflow == 400 - accepted);
    /* Drain fully against a firmware that never completes: every message is
     * either submitted or deferred, and both waits stay at zero. */
    for (i = 0; i < 200; ++i)
        dt1_tx_service(&t, 8);
    assert(f.m.wrap_waits == 0);
    assert(t.submitted + (t.tx_head - t.tx_tail) == accepted);
    /* Order: with a contiguous run in the staging ring, byte k of the run is
     * byte k of the concatenated frames, so a reordered message shows up as a
     * mismatched tag at its 8-byte header. */
    {
        uint8_t expect[61];
        uint32_t k;
        for (k = 0; k < t.submitted; ++k) {
            make_frame(expect, 61, (uint8_t)(k & 0x7f));
            assert(f.m.staging[k * 61 + 7] == expect[7]);
            assert(f.m.staging[k * 61] == 0xf0);
        }
    }
    puts("congestion: bounded staging, whole-message drops, no busy waits OK");
}

static void test_disconnect_discards_stale_state(void)
{
    struct fake_fw f;
    struct dt1_transport t;
    struct sink s;
    uint8_t frame[61], packets[4 * 32];
    fw_init(&f);
    memset(&s, 0, sizeof s);
    dt1_transport_init(&t, &ops, &f);
    wire(&t, &s);
    assert(dt1_transport_open(&t));
    dt1_link_update(&t, 1);
    make_frame(frame, 61, 9);
    assert(dt1_tx_stage(&t, frame, 61) == DT1_TX_OK);
    /* A partial frame arrives, then the cable is pulled. */
    usb_pack(packets, frame, 12);
    fw_deliver(&f, packets, 12);
    dt1_rx_service(&t, 64);
    assert(t.sysex.in_sysex == 1);
    assert(s.chain_len == 0 && s.ours_len == 0);
    assert(!dt1_link_update(&t, 0));
    assert(t.sysex.in_sysex == 0);
    assert(t.tx_head == t.tx_tail);
    assert(t.tx_discarded == 1);
    /* Nothing may be staged before the link is re-established. */
    assert(dt1_tx_stage(&t, frame, 61) == DT1_TX_LINK_DOWN);
    /* The firmware's sender refuses while ready/isr are down. */
    f.m.isr_on = 0;
    assert(dt1_link_update(&t, 1));
    assert(dt1_tx_stage(&t, frame, 61) == DT1_TX_OK);
    assert(dt1_tx_service(&t, 4) == 0);      /* can_send saw the live state */
    assert(t.deferrals == 1);
    /* Reconnect: fresh state, old staged bytes gone. */
    assert(!dt1_link_update(&t, 0));
    f.m.isr_on = 1;
    f.m.queued = 0;
    assert(dt1_link_update(&t, 1));
    assert(t.tx_head == t.tx_tail && t.rx.head == t.rx.tail);
    /* The stock handler is restored on close. */
    dt1_transport_close(&t);
    assert(f.handler == 0);
    puts("disconnect: stale TX/RX discarded, reopen required, handler chained OK");
}

static void test_receive_chaining_preserves_midi(void)
{
    struct fake_fw f;
    struct dt1_transport t;
    struct sink s;
    uint8_t ours[40], foreign[24], stream[128], packets[4 * 64];
    uint32_t n;
    fw_init(&f);
    memset(&s, 0, sizeof s);
    dt1_transport_init(&t, &ops, &f);
    wire(&t, &s);
    dt1_transport_open(&t);
    make_frame(ours, sizeof ours, 5);
    foreign[0] = 0xf0;
    foreign[1] = 0x41;
    foreign[2] = 0x10;
    foreign[3] = 0x42;
    foreign[4] = 0x00;
    foreign[5] = 0xf7;
    /* Note on, foreign SysEx, our frame, real-time, CC. */
    n = 0;
    stream[n++] = 0x90; stream[n++] = 0x3c; stream[n++] = 0x64;
    memcpy(stream + n, foreign, 6); n += 6;
    memcpy(stream + n, ours, sizeof ours); n += sizeof ours;
    stream[n++] = 0xf8;
    stream[n++] = 0xb0; stream[n++] = 0x07; stream[n++] = 0x40;
    usb_pack(packets, stream, n);
    /* Deliver in two callback chunks that each end mid-frame: once inside our
     * SysEx, once inside the foreign one. The adapter must reassemble across
     * callbacks. */
    fw_deliver(&f, packets, 4 * 12);        /* through the 7th byte of ours */
    dt1_rx_service(&t, 64);
    fw_deliver(&f, packets + 4 * 12, 4 * ((n + 2) / 3 - 12));
    dt1_rx_service(&t, 4096);
    assert(s.frames == 1);
    assert(s.ours_len == sizeof ours && !memcmp(s.ours, ours, sizeof ours));
    /* Everything else arrives at the stock handler in the original order.
     * The final zero is the firmware's own doing, not ours: 0x40004760 copies
     * table[cin] data bytes for every packet, and that is three for CIN 0xB,
     * so the fourth byte of the CC packet (zero) is copied too. The adapter
     * must neither add nor reorder bytes, and must not silently drop one the
     * stock parser would have received. */
    {
        uint8_t expect[64];
        uint32_t e = 0;
        expect[e++] = 0x90; expect[e++] = 0x3c; expect[e++] = 0x64;
        memcpy(expect + e, foreign, 6); e += 6;
        expect[e++] = 0xf8;
        expect[e++] = 0xb0; expect[e++] = 0x07; expect[e++] = 0x40;
        expect[e++] = 0x00;         /* OS 1.54's third data byte for CIN 0xB */
        assert(s.chain_len == e);
        assert(!memcmp(s.chain, expect, e));
    }
    puts("receive: our SysEx withheld, all other MIDI chained byte-exact OK");
}

static void test_receive_splits_realtime_inside_sysex(void)
{
    struct fake_fw f;
    struct dt1_transport t;
    struct sink s;
    /* Eight USB-MIDI packets: SysEx head, clock, SysEx tail, CC. */
    static const uint8_t packets[4 * 4] = {
        0x04, 0xf0, 0x41, 0x10,     /* SysEx start, CIN 0x4 */
        0x0f, 0xf8, 0x00, 0x00,     /* clock, CIN 0xF */
        0x05, 0xf7, 0x00, 0x00,     /* EOX, CIN 0x5 */
        0x0b, 0xb0, 0x07, 0x00,     /* CC 7 = 0 */
    };
    static const uint8_t expect[] = {0xf0, 0x41, 0x10, 0xf8, 0xf7, 0xb0, 0x07, 0x00};
    fw_init(&f);
    memset(&s, 0, sizeof s);
    dt1_transport_init(&t, &ops, &f);
    wire(&t, &s);
    dt1_transport_open(&t);
    fw_deliver(&f, packets, sizeof packets);
    dt1_rx_service(&t, 64);
    assert(s.frames == 0);
    assert(s.chain_len == sizeof expect);
    assert(!memcmp(s.chain, expect, sizeof expect));
    puts("receive: real-time bytes pass through in place, order preserved OK");
}

static void test_oversize_frame_is_replayed_not_dropped(void)
{
    struct fake_fw f;
    struct dt1_transport t;
    struct sink s;
    uint8_t *big;
    static uint8_t huge[1400];
    uint8_t packets[4 * 500];
    uint32_t i, n;
    fw_init(&f);
    memset(&s, 0, sizeof s);
    dt1_transport_init(&t, &ops, &f);
    wire(&t, &s);
    dt1_transport_open(&t);
    /* Shrink the frame buffer to force the oversize path. */
    t.sysex.cap = 32;
    big = huge;
    n = 200;
    big[0] = 0xf0; big[1] = 0x41;
    for (i = 2; i < n - 1; ++i)
        big[i] = (uint8_t)(i & 0x7f);
    big[n - 1] = 0xf7;
    usb_pack(packets, big, n);
    fw_deliver(&f, packets, 4 * ((n + 2) / 3));
    dt1_rx_service(&t, 4096);
    assert(s.frames == 0);
    assert(t.rx_oversize == 1);
    assert(s.chain_len == n && !memcmp(s.chain, big, n));
    puts("receive: oversize frame counted and replayed intact OK");
}

static void test_handle_wrap_policy(void)
{
    struct fake_fw f;
    struct dt1_transport t;
    uint8_t msg[48];
    uint32_t i;
    fw_init(&f);
    dt1_transport_init(&t, &ops, &f);
    dt1_link_update(&t, 1);
    memset(msg, 0x11, sizeof msg);
    msg[0] = 0xf0;
    msg[47] = 0xf7;
    /* 48 bytes crosses the firmware's 0x2F wrap threshold; index near the end
     * of the ring with an empty queue must still not spin (wrap to 0 is
     * allowed when nothing is queued). */
    f.m.index = 0xFC0;
    f.m.queued = 0;
    assert(dt1_tx_stage(&t, msg, 48) == DT1_TX_OK);
    dt1_tx_service(&t, 4);
    assert(f.m.wrap_waits == 0);
    /* With queued bytes >= index - 0x3F the firmware would block. Our
     * preflight sees that and defers; the adapter never calls the sender. */
    f.m.index = 0xFC0;
    f.m.queued = 0xFC0 - 0x20;
    {
        uint32_t sends = f.sends;
        assert(dt1_tx_stage(&t, msg, 48) == DT1_TX_OK);
        assert(dt1_tx_service(&t, 4) == 0);
        assert(f.sends == sends);
        assert(f.m.wrap_waits == 0);
        assert(t.deferrals >= 1);
    }
    for (i = 0; i < 4; ++i)
        dt1_model_complete(&f.m, 100);
    assert(dt1_tx_service(&t, 4) == 1);
    assert(f.m.wrap_waits == 0);
    puts("ownership: handle wrap avoided by preflight, never by spinning OK");
}

static void test_staging_refuses_oversize_messages(void)
{
    struct dt1_transport t;
    struct fake_fw f;
    uint8_t msg[DT1_MSG_MAX + 1];
    fw_init(&f);
    dt1_transport_init(&t, &ops, &f);
    dt1_link_update(&t, 1);
    memset(msg, 0x22, sizeof msg);
    msg[0] = 0xf0;
    assert(dt1_tx_stage(&t, msg, sizeof msg) == DT1_TX_DROPPED);
    assert(t.tx_overflow == 1 && t.tx_head == t.tx_tail);
    puts("staging: over-long message refused whole OK");
}

int main(void)
{
    test_packet_layer();
    test_message_staging();
    test_render_never_calls_firmware();
    test_completion_and_backpressure();
    test_congestion_is_bounded_and_ordered();
    test_disconnect_discards_stale_state();
    test_receive_chaining_preserves_midi();
    test_receive_splits_realtime_inside_sysex();
    test_oversize_frame_is_replayed_not_dropped();
    test_handle_wrap_policy();
    test_staging_refuses_oversize_messages();
    puts("DT1 USB-MIDI transport adapter: PASS (host model)");
    return 0;
}
