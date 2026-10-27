/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "circle_link_adapter.h"
#include "device_link.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

struct peer {
    struct sc_circle_link host;
    struct sc_device_link dev;
    unsigned installed, cleared, sent, dropped;
    uint8_t reply[SC_CONTROL_WIRE_SIZE];
    unsigned reply_size;
    uint8_t host_reply[SC_CONTROL_WIRE_SIZE];
    unsigned host_reply_size;
    int send_ok;
    int gate_at_activate_send;
};

static int install(void *ctx, const struct sc_frame *f)
{
    struct peer *p = ctx;
    ++p->installed;
    assert(f->session);
    return 1;
}

static void clear(void *ctx)
{
    struct peer *p = ctx;
    ++p->cleared;
}

static int send(void *ctx, const uint8_t *frame, uint32_t size)
{
    struct peer *p = ctx;
    if (!p->send_ok) return 0;
    if (frame[0] == 0xf0 && frame[6] == SC_CONTROL_VERSION &&
        frame[7] == SC_ACTIVATE)
        p->gate_at_activate_send = sc_circle_audio_allowed(&p->host);
    assert(size == SC_CONTROL_WIRE_SIZE);
    memcpy(p->reply, frame, size);
    p->reply_size = size;
    memcpy(p->host_reply, p->reply, size);
    p->host_reply_size = size;
    ++p->sent;
    return 1;
}

static void feed(struct peer *p, const uint8_t *bytes, unsigned size)
{
    unsigned i;
    for (i = 0; i < size; ++i) sc_circle_midi_byte(&p->host, bytes[i]);
    sc_circle_process(&p->host, 16);
}

static struct sc_frame snapshot(uint32_t session, unsigned block)
{
    struct sc_frame f = {.session=session,.sequence=1,.block=block,.voice=1,
                        .note=48 << 16,.velocity=INT16_MAX,.flags=SC_START};
    unsigned i;
    for (i = 0; i < 8; ++i) f.params[i] = (int16_t)(i << 8);
    return f;
}

static void open_peer(struct peer *p, uint32_t session, uint32_t block)
{
    uint8_t hello[SC_CONTROL_WIRE_SIZE];
    unsigned sent = p->sent;
    assert(sc_dev_open(&p->dev, session, block, hello));
    feed(p, hello, sizeof hello);
    assert(p->sent == sent + 1);
    {
        struct sc_control c;
        assert(sc_decode_control(&c, p->reply, p->reply_size));
        assert(c.kind == SC_HELLO_ACK && c.session == session);
        assert(sc_dev_receive_control(&p->dev, &c, block).invalidated == 0);
    }
    assert(p->dev.state == SC_DEVICE_WAIT_ACTIVATE);
}

static void activate(struct peer *p, uint32_t block)
{
    struct sc_frame src = snapshot(p->dev.session, block), f;
    uint8_t wire[SC_WIRE_SIZE];
    unsigned sent = p->sent;
    unsigned installed = p->installed;
    assert(sc_dev_send_start(&p->dev, &src, block, wire));
    assert(sc_decode(&f, wire, sizeof wire));
    feed(p, wire, sizeof wire);
    assert(p->installed == installed + 1);
    assert(p->sent == sent + 1);
    assert(sc_circle_audio_allowed(&p->host));
    assert(p->gate_at_activate_send == 0);
    {
        struct sc_control c;
        struct sc_device_update u;
        assert(sc_decode_control(&c, p->reply, p->reply_size));
        assert(c.kind == SC_ACTIVATE && c.session == p->dev.session &&
               c.sequence == f.sequence && c.value == block);
        u = sc_dev_receive_control(&p->dev, &c, block);
        assert(u.activated && !u.invalidated);
    }
    assert(sc_circle_audio_allowed(&p->host));
    assert(sc_dev_audio_allowed(&p->dev));
}

int main(void)
{
    struct peer p;
    uint8_t hello[SC_CONTROL_WIRE_SIZE], wire[SC_WIRE_SIZE];
    struct sc_control c;
    struct sc_frame f, src;

    memset(&p, 0, sizeof p);
    p.send_ok = 1;
    sc_circle_init(&p.host, install, send, clear, &p, 100);
    assert(!sc_circle_audio_allowed(&p.host));
    assert(sc_dev_open(&p.dev, 42, 100, hello));
    feed(&p, hello, sizeof hello);
    assert(p.installed == 0 && !sc_circle_audio_allowed(&p.host));
    assert(sc_decode_control(&c, p.reply, p.reply_size));
    assert(c.kind == SC_HELLO_ACK);
    assert(sc_dev_receive_control(&p.dev, &c, 100).invalidated == 0);

    /* No START installs while the HELLO_ACK itself could not be sent. */
    memset(&p.host, 0, sizeof p.host);
    sc_circle_init(&p.host, install, send, clear, &p, 100);
    p.send_ok = 0;
    feed(&p, hello, sizeof hello);
    assert(!sc_circle_audio_allowed(&p.host));
    assert(p.host.host.state == SC_HOST_INVALID);
    p.send_ok = 1;

    /* Fresh session, full handshake; PCM stays silent until the activation
     * reply has actually been queued, then opens. */
    memset(&p, 0, sizeof p);
    p.send_ok = 1;
    sc_circle_init(&p.host, install, send, clear, &p, 100);
    open_peer(&p, 43, 200);
    activate(&p, 200);
    assert(p.installed == 1 && p.cleared >= 1);

    /* A malformed/second START slot that is not a replay is a fault and
     * closes both sides. */
    src = snapshot(43, 201);
    src.sequence = 3;
    assert(sc_encode(wire, &src));
    feed(&p, wire, sizeof wire);
    assert(!sc_circle_audio_allowed(&p.host));
    assert(p.host.host.reason == SC_REASON_SEQUENCE);
    assert(sc_decode_control(&c, p.reply, p.reply_size));
    assert(c.kind == SC_DEACTIVATE && c.value == SC_REASON_SEQUENCE);

    /* Active path: two ordered snapshots remain active; a gap latches silence. */
    memset(&p, 0, sizeof p);
    p.send_ok = 1;
    sc_circle_init(&p.host, install, send, clear, &p, 100);
    open_peer(&p, 44, 300);
    activate(&p, 300);
    src = snapshot(44, 301);
    assert(sc_dev_send_start(&p.dev, &src, 301, wire));
    assert(sc_decode(&f, wire, sizeof wire));
    feed(&p, wire, sizeof wire);
    assert(p.installed == 2 && sc_circle_audio_allowed(&p.host));
    assert(sc_decode_control(&c, p.reply, p.reply_size));
    assert(sc_dev_receive_control(&p.dev, &c, 301).invalidated == 0);

    /* A parameter-only snapshot after activation need not carry SC_START. */
    src = snapshot(44, 302);
    src.flags = 0;
    assert(sc_dev_send_start(&p.dev, &src, 302, wire));
    assert(sc_decode(&f, wire, sizeof wire));
    feed(&p, wire, sizeof wire);
    assert(p.installed == 3 && sc_circle_audio_allowed(&p.host));
    assert(sc_decode_control(&c, p.reply, p.reply_size));
    assert(sc_dev_receive_control(&p.dev, &c, 302).invalidated == 0);

    src.sequence = 5;    /* sequence 4 lost */
    src.block += 1;
    assert(sc_encode(wire, &src));
    feed(&p, wire, sizeof wire);
    assert(!sc_circle_audio_allowed(&p.host));
    assert(p.host.host.reason == SC_REASON_SEQUENCE);

    /* Timeout and overflow each close the gate. */
    memset(&p, 0, sizeof p);
    p.send_ok = 1;
    sc_circle_init(&p.host, install, send, clear, &p, 10);
    open_peer(&p, 45, 400);
    activate(&p, 400);
    sc_circle_advance_block(&p.host, 411);
    sc_circle_process(&p.host, 1);
    assert(!sc_circle_audio_allowed(&p.host));
    assert(p.host.host.reason == SC_REASON_TIMEOUT);

    memset(&p, 0, sizeof p);
    p.send_ok = 1;
    sc_circle_init(&p.host, install, send, clear, &p, 100);
    open_peer(&p, 46, 500);
    activate(&p, 500);
    {
        unsigned i;
        for (i = 0; i < SC_CIRCLE_RX_QUEUE + 1; ++i) {
            src = snapshot(46, 501 + i);
            src.sequence = 3 + i;
            assert(sc_encode(wire, &src));
            {
                unsigned j;
                for (j = 0; j < sizeof wire; ++j)
                    sc_circle_midi_byte(&p.host, wire[j]);
            }
        }
        assert(!sc_circle_audio_allowed(&p.host));
        assert(sc_circle_rx_queued(&p.host) <= SC_CIRCLE_RX_QUEUE);
    }

    /* Transport failure and stale reconnect require a fresh session. */
    memset(&p, 0, sizeof p);
    p.send_ok = 1;
    sc_circle_init(&p.host, install, send, clear, &p, 100);
    open_peer(&p, 47, 600);
    activate(&p, 600);
    sc_circle_transport_lost(&p.host, SC_REASON_TRANSPORT);
    assert(!sc_circle_audio_allowed(&p.host));
    assert(sc_host_invalidate(&p.host.host, SC_REASON_TRANSPORT, hello));
    assert(sc_decode_control(&c, hello, SC_CONTROL_WIRE_SIZE));
    assert(c.kind == SC_DEACTIVATE && c.value == SC_REASON_TRANSPORT);
    {
        uint8_t stale[SC_CONTROL_WIRE_SIZE];
        assert(sc_dev_open(&p.dev, 47, 600, stale));
        feed(&p, stale, sizeof stale);
        assert(!sc_circle_audio_allowed(&p.host));
    }
    open_peer(&p, 49, 700);
    activate(&p, 700);
    assert(sc_circle_audio_allowed(&p.host));

    /* A keepalive naming the wrong accepted block is a sequence fault. */
    {
        struct sc_control bad = {.kind=SC_KEEPALIVE,.session=p.dev.session,
                                 .sequence=p.dev.accepted_sequence,
                                 .value=p.dev.accepted_block + 1};
        uint8_t bad_wire[SC_CONTROL_WIRE_SIZE];
        assert(sc_encode_control(bad_wire, &bad));
        feed(&p, bad_wire, sizeof bad_wire);
        assert(!sc_circle_audio_allowed(&p.host));
        assert(p.host.host.reason == SC_REASON_SEQUENCE);
    }

    /* Duplicate START while ACTIVATE is unacknowledged is byte-identical:
     * ACTIVATE is re-emitted without reinstalling the snapshot. */
    {
        uint8_t replay[SC_WIRE_SIZE], first[SC_WIRE_SIZE];
        unsigned installed = p.installed;
        struct sc_frame original = snapshot(p.dev.session, 701);
        memset(&p, 0, sizeof p);
        p.send_ok = 1;
        sc_circle_init(&p.host, install, send, clear, &p, 100);
        open_peer(&p, 52, 700);
        assert(sc_dev_send_start(&p.dev, &original, 701, first));
        installed = p.installed;
        feed(&p, first, sizeof first);
        assert(p.installed == installed + 1 && sc_circle_audio_allowed(&p.host));
        assert(sc_decode_control(&c, p.reply, p.reply_size));
        assert(c.kind == SC_ACTIVATE);
        assert(sc_dev_resend_start(&p.dev, replay));
        assert(!memcmp(first, replay, sizeof first));
        feed(&p, replay, sizeof replay);
        assert(p.installed == installed + 1); /* replay did not install */
        assert(sc_decode_control(&c, p.reply, p.reply_size));
        assert(c.kind == SC_ACTIVATE);
        assert(sc_dev_receive_control(&p.dev, &c, 701).activated);
        {
            uint8_t live[SC_CONTROL_WIRE_SIZE];
            assert(sc_dev_keepalive(&p.dev, 701, live));
            feed(&p, live, sizeof live);
        }
        assert(sc_circle_audio_allowed(&p.host));

        /* Same sequence/block but different payload is not a replay. */
        {
            struct sc_frame conflict = snapshot(p.dev.session, 701);
            conflict.sequence = 1;
            conflict.block = 701;
            conflict.params[0] = 123;
            assert(sc_encode(wire, &conflict));
            feed(&p, wire, sizeof wire);
            assert(!sc_circle_audio_allowed(&p.host));
            assert(p.host.host.reason == SC_REASON_SEQUENCE);
        }
    }

    /* If ACTIVATE cannot be queued, both the Pi gate and the device stay
     * closed; the host latches invalid instead of pretending it activated. */
    memset(&p, 0, sizeof p);
    p.send_ok = 1;
    sc_circle_init(&p.host, install, send, clear, &p, 100);
    open_peer(&p, 50, 800);
    {
        struct sc_frame original = snapshot(50, 800);
        assert(sc_dev_send_start(&p.dev, &original, 800, wire));
        p.send_ok = 0; /* ACTIVATE queue fails */
        feed(&p, wire, sizeof wire);
        assert(!sc_circle_audio_allowed(&p.host));
        assert(p.host.host.state == SC_HOST_INVALID);
        p.send_ok = 1;
    }

    /* The device replay window is bounded; exhausting it is fail-closed. */
    {
        struct sc_device_link d;
        uint8_t hello_wire[SC_CONTROL_WIRE_SIZE], start_wire[SC_WIRE_SIZE];
        struct sc_control ack;
        unsigned i;
        assert(sc_dev_open(&d, 60, 1000, hello_wire));
        assert(sc_decode_control(&ack, hello_wire, sizeof hello_wire));
        ack.kind = SC_HELLO_ACK;
        ack.value = 1000;
        assert(sc_dev_receive_control(&d, &ack, 1000).invalidated == 0);
        for (i = 0; i < SC_DEVICE_REPLAY; ++i) {
            struct sc_frame s = snapshot(60, 1001 + i);
            assert(sc_dev_send_start(&d, &s, 1001 + i, start_wire));
        }
        {
            struct sc_frame s = snapshot(60, 1001 + SC_DEVICE_REPLAY);
            assert(!sc_dev_send_start(&d, &s, 1001 + SC_DEVICE_REPLAY,
                                      start_wire));
        }
        assert(d.state == SC_DEVICE_INVALID);
        assert(d.reason == SC_REASON_OVERFLOW);
    }

    /* An active session whose device lease is not refreshed times out. */
    memset(&p, 0, sizeof p);
    p.send_ok = 1;
    sc_circle_init(&p.host, install, send, clear, &p, 5);
    open_peer(&p, 51, 900);
    {
        struct sc_frame original = snapshot(51, 900);
        assert(sc_dev_send_start(&p.dev, &original, 900, wire));
        feed(&p, wire, sizeof wire);
        sc_circle_advance_block(&p.host, 906);
        sc_circle_process(&p.host, 1);
        assert(!sc_circle_audio_allowed(&p.host));
        assert(p.host.host.reason == SC_REASON_TIMEOUT);
    }

    puts("control handshake / ACK / silence / loss / overflow / timeout / reconnect: PASS");
    return 0;
}
