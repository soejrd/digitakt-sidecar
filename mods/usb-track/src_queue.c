/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "src_queue.h"

#define barrier() __asm__ __volatile__ ("" ::: "memory")
#define load(p) (*(p))
#define store(p, v) (*(p) = (v))

int ut_session_open(struct ut_session *s, uint32_t session)
{
    uint32_t i;
    if (!s || !session)
        return 0;
    /* Refuse a re-open while armed or after a failure: an explicit close must
     * first disarm. The reset guard below covers an interrupt already entering. */
    if (load(&s->valid) || load(&s->fatal))
        return 0;
    store(&s->reset, 1);
    barrier();
    store(&s->valid, 0);
    store(&s->fatal, 0);
    store(&s->queue.head, 0);
    store(&s->queue.tail, 0);
    store(&s->queue.overflow, 0);
    s->id = session;
    s->block = 0;
    s->next = 1;
    s->consume_next = 1;
    s->machine = 0;
    s->have_machine = 0;
    for (i = 0; i < UT_QUEUE_CAPACITY; ++i) {
        s->queue.ring[i].session = 0;
        s->queue.ring[i].block = 0;
        s->queue.ring[i].sequence = 0;
    }
    barrier();
    store(&s->valid, 1);
    store(&s->reset, 0);
    barrier();
    return 1;
}

void ut_session_close(struct ut_session *s)
{
    if (!s)
        return;
    store(&s->reset, 1);
    barrier();
    store(&s->valid, 0);
    store(&s->fatal, 0);
    store(&s->reset, 0);
    barrier();
}

void ut_session_fail(struct ut_session *s)
{
    if (!s)
        return;
    store(&s->queue.overflow, 1);
    store(&s->fatal, 1);
    store(&s->valid, 0);
    barrier();
    /* Called by the producer or the UI; no allocation or waiting here. */
}

void ut_session_invalidate(struct ut_session *s)
{
    if (!s)
        return;
    store(&s->valid, 0);
    barrier();
}

int ut_session_invalidated(const struct ut_session *s)
{
    return s && !load(&s->valid) && !load(&s->fatal);
}

int ut_session_failed(const struct ut_session *s)
{
    return s && load(&s->fatal);
}

int ut_session_ready(const struct ut_session *s)
{
    return s && load(&s->valid) && !load(&s->fatal);
}

int ut_session_room(struct ut_session *s)
{
    uint32_t head, tail;
    if (!s || load(&s->reset) || load(&s->fatal) || !load(&s->valid))
        return 0;
    head = load(&s->queue.head);
    barrier();
    tail = load(&s->queue.tail);
    if (head - tail >= UT_QUEUE_CAPACITY || s->next == 0 || s->next == UINT32_MAX) {
        ut_session_fail(s);
        return 0;
    }
    return 1;
}

int ut_session_capture(struct ut_session *s, uint8_t machine,
                       const int16_t params[8], int32_t note, int16_t velocity,
                       uint8_t start)
{
    struct ut_record *r;
    uint32_t head, tail, i;
    if (!s || !params)
        return UT_CAPTURE_IDLE;
    if (load(&s->fatal))
        return UT_CAPTURE_FAILED;
    if (load(&s->reset) || !load(&s->valid))
        return UT_CAPTURE_IDLE;
    if (s->have_machine && s->machine != machine) {
        store(&s->valid, 0);
        barrier();
        return UT_CAPTURE_IDLE;
    }
    head = load(&s->queue.head);
    barrier();
    tail = load(&s->queue.tail);
    if (head - tail >= UT_QUEUE_CAPACITY || s->next == 0 || s->next == UINT32_MAX) {
        store(&s->queue.overflow, 1);
        store(&s->fatal, 1);
        store(&s->valid, 0);
        barrier();
        return UT_CAPTURE_FAILED;
    }
    r = &s->queue.ring[head & (UT_QUEUE_CAPACITY - 1u)];
    r->session = s->id;
    r->block = s->block;
    r->sequence = s->next;
    r->note = note;
    for (i = 0; i < 8; ++i)
        r->params[i] = params[i];
    r->velocity = velocity;
    r->machine = machine;
    r->flags = start ? UT_START : 0;
    s->machine = machine;
    s->have_machine = 1;
    barrier();
    store(&s->queue.head, head + 1u);
    ++s->next;
    ++s->block;
    return UT_CAPTURE_OK;
}

int ut_session_peek(struct ut_session *s, struct ut_record *out)
{
    uint32_t head, tail, i;
    const struct ut_record *r;
    if (!s || !out || !load(&s->valid) || load(&s->fatal))
        return 0;
    head = load(&s->queue.head);
    barrier();
    tail = load(&s->queue.tail);
    if (head == tail)
        return 0;
    r = &s->queue.ring[tail & (UT_QUEUE_CAPACITY - 1u)];
    if (r->session != s->id || r->sequence != s->consume_next) {
        ut_session_fail(s);
        return 0;
    }
    out->session = r->session;
    out->block = r->block;
    out->sequence = r->sequence;
    out->note = r->note;
    for (i = 0; i < 8; ++i)
        out->params[i] = r->params[i];
    out->velocity = r->velocity;
    out->machine = r->machine;
    out->flags = r->flags;
    return 1;
}

int ut_session_pop(struct ut_session *s, struct ut_record *out)
{
    uint32_t tail;
    if (!ut_session_peek(s, out))
        return 0;
    if (out->session != s->id || out->sequence != s->consume_next) {
        ut_session_fail(s);
        return 0;
    }
    tail = load(&s->queue.tail);
    barrier();
    store(&s->queue.tail, tail + 1u);
    ++s->consume_next;
    return 1;
}

void ut_fake_transport_init(struct ut_fake_transport *t)
{
    if (!t)
        return;
    t->sent = 0;
    t->busy = 0;
}

unsigned ut_fake_transport_drain(struct ut_session *s, struct ut_fake_transport *t,
                                 unsigned budget,
                                 int (*sink)(void *ctx, const struct ut_record *r),
                                 void *ctx)
{
    struct ut_record consumed;
    unsigned n = 0;
    if (!s || !t || !sink)
        return 0;
    while (n < budget) {
        if (!ut_session_ready(s)) {
            t->busy = 0;
            break;
        }
        if (!t->busy) {
            if (!ut_session_peek(s, &t->in_flight))
                break;
            t->busy = 1;
        }
        if (!sink(ctx, &t->in_flight))
            break;
        if (!ut_session_pop(s, &consumed))
            break;
        t->busy = 0;
        ++t->sent;
        ++n;
    }
    return n;
}
