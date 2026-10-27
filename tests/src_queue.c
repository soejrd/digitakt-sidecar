/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "src_queue.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

struct capture {
    struct ut_record records[4096];
    uint32_t count;
    uint32_t reject_at;
};

static int sink(void *ctx, const struct ut_record *r)
{
    struct capture *c = ctx;
    if (c->reject_at && c->count == c->reject_at)
        return 0;
    assert(c->count < sizeof c->records / sizeof c->records[0]);
    c->records[c->count++] = *r;
    return 1;
}

static void params_for(int16_t p[8], int32_t n)
{
    int i;
    for (i = 0; i < 8; ++i)
        p[i] = (int16_t)(n * 977 + i * 211);
}

static void test_dense_and_starts(void)
{
    struct ut_session s;
    struct ut_fake_transport t;
    struct capture c;
    int16_t p[8], last[8];
    int32_t n, starts = 0;
    memset(&c, 0, sizeof c);
    ut_fake_transport_init(&t);
    memset(&s, 0, sizeof s);
    assert(ut_session_open(&s, 77));
    assert(!ut_session_open(&s, 78)); /* no re-open while producer is armed */
    for (n = 0; n < 1200; ++n) {
        params_for(p, n);
        memcpy(last, p, sizeof p);
        /* Repeated starts: every third block, including adjacent blocks. */
        assert(ut_session_capture(&s, 121, p, n * 0x10000, (int16_t)(n * 13),
                                   n % 3 == 0) == UT_CAPTURE_OK);
        if (n % 3 == 0)
            ++starts;
        /* Dense modulation with a sustainable, variable drain rate: the queue
         * must wrap without dropping. Congestion is tested separately. */
        if ((n & 3) == 0)
            (void)ut_fake_transport_drain(&s, &t, 2, sink, &c);
        if ((n % 5) == 0)
            (void)ut_fake_transport_drain(&s, &t, 6, sink, &c);
    }
    while (ut_fake_transport_drain(&s, &t, 17, sink, &c)) {}
    assert(!ut_session_failed(&s));
    assert(c.count == 1200 && t.sent == 1200);
    for (n = 0; n < 1200; ++n) {
        const struct ut_record *r = &c.records[n];
        assert(r->session == 77);
        assert(r->sequence == (uint32_t)n + 1);
        assert(r->block == (uint32_t)n);
        assert(r->note == n * 0x10000);
        assert(r->velocity == (int16_t)(n * 13));
        assert(r->machine == 121);
        assert(r->flags == ((n % 3 == 0) ? UT_START : 0));
        params_for(p, n);
        assert(!memcmp(r->params, p, sizeof p));
    }
    assert(starts == 400);
    assert(!memcmp(last, c.records[c.count - 1].params, sizeof last));
    puts("dense modulation / repeated starts: PASS");
}

static void test_congestion_preserves_order(void)
{
    struct ut_session s;
    struct ut_fake_transport t;
    struct capture c;
    int16_t p[8];
    memset(&c, 0, sizeof c);
    ut_fake_transport_init(&t);
    memset(&s, 0, sizeof s);
    assert(ut_session_open(&s, 88));
    params_for(p, 1);
    assert(ut_session_capture(&s, 121, p, 1, 1, 1) == UT_CAPTURE_OK);
    params_for(p, 2);
    assert(ut_session_capture(&s, 121, p, 2, 2, 1) == UT_CAPTURE_OK);
    c.reject_at = 1;
    assert(ut_fake_transport_drain(&s, &t, 8, sink, &c) == 1);
    assert(t.busy && t.sent == 1 && c.count == 1);
    /* A later drain cannot overtake the in-flight record. */
    assert(ut_fake_transport_drain(&s, &t, 8, sink, &c) == 0);
    assert(c.count == 1);
    c.reject_at = 0;
    assert(ut_fake_transport_drain(&s, &t, 8, sink, &c) == 1);
    assert(c.count == 2);
    assert(c.records[0].sequence == 1 && c.records[1].sequence == 2);
    puts("congestion retry / ordering: PASS");
}

static void test_adjacent_starts(void)
{
    struct ut_session s;
    struct ut_fake_transport t;
    struct capture c;
    int16_t p[8];
    memset(&c, 0, sizeof c);
    ut_fake_transport_init(&t);
    memset(&s, 0, sizeof s);
    assert(ut_session_open(&s, 89));
    params_for(p, 0);
    assert(ut_session_capture(&s, 121, p, 1, 1, 1) == UT_CAPTURE_OK);
    assert(ut_session_capture(&s, 121, p, 2, 2, 1) == UT_CAPTURE_OK);
    assert(ut_session_capture(&s, 121, p, 3, 3, 1) == UT_CAPTURE_OK);
    assert(ut_fake_transport_drain(&s, &t, 8, sink, &c) == 3);
    assert(c.records[0].flags == UT_START);
    assert(c.records[1].flags == UT_START);
    assert(c.records[2].flags == UT_START);
    assert(c.records[0].sequence + 1 == c.records[1].sequence);
    assert(c.records[1].sequence + 1 == c.records[2].sequence);
    puts("adjacent repeated starts retained in order: PASS");
}

static void test_overflow_forces_failure(void)
{
    struct ut_session s;
    struct ut_record r;
    int16_t p[8];
    uint32_t n;
    memset(&s, 0, sizeof s);
    assert(ut_session_open(&s, 99));
    params_for(p, 1);
    for (n = 0; n < UT_QUEUE_CAPACITY; ++n)
        assert(ut_session_capture(&s, 121, p, 0, 0, 0) == UT_CAPTURE_OK);
    assert(ut_session_capture(&s, 121, p, 0, 0, 0) == UT_CAPTURE_FAILED);
    assert(ut_session_failed(&s));
    assert(!ut_session_ready(&s));
    assert(!ut_session_peek(&s, &r));
    assert(ut_session_capture(&s, 121, p, 0, 0, 0) == UT_CAPTURE_FAILED);
    /* A fresh session is refused until the consumer deliberately closes. */
    assert(!ut_session_open(&s, 100));
    ut_session_close(&s);
    assert(ut_session_open(&s, 100));
    assert(ut_session_capture(&s, 121, p, 0, 0, 0) == UT_CAPTURE_OK);
    puts("overflow invalidation / fresh session gate: PASS");
}

static void test_machine_change_invalidates(void)
{
    struct ut_session s;
    struct ut_record r;
    int16_t p[8];
    memset(&s, 0, sizeof s);
    assert(ut_session_open(&s, 123));
    params_for(p, 0);
    assert(ut_session_capture(&s, 121, p, 0, 0, 0) == UT_CAPTURE_OK);
    assert(ut_session_capture(&s, 120, p, 0, 0, 0) == UT_CAPTURE_IDLE);
    assert(!ut_session_failed(&s));
    assert(ut_session_invalidated(&s));
    assert(!ut_session_peek(&s, &r));
    assert(ut_session_open(&s, 124)); /* fresh session for the new machine */
    puts("machine change invalidates without fatal silence: PASS");
}

static void test_reset_guard(void)
{
    struct ut_session s;
    int16_t p[8];
    memset(&s, 0, sizeof s);
    assert(ut_session_open(&s, 321));
    params_for(p, 0);
    s.reset = 1; /* the UI is rearming: a render interrupt must not write */
    assert(ut_session_capture(&s, 121, p, 0, 0, 0) == UT_CAPTURE_IDLE);
    assert(s.queue.head == 0 && s.queue.tail == 0 && s.next == 1);
    s.reset = 0;
    assert(ut_session_capture(&s, 121, p, 0, 0, 0) == UT_CAPTURE_OK);
    puts("render reset guard: PASS");
}

static void test_sequence_fault_invalidates(void)
{
    struct ut_session s;
    struct ut_record r;
    int16_t p[8];
    memset(&s, 0, sizeof s);
    assert(ut_session_open(&s, 456));
    params_for(p, 0);
    assert(ut_session_capture(&s, 121, p, 0, 0, 0) == UT_CAPTURE_OK);
    s.queue.ring[0].sequence = 2; /* simulate corruption in the consumer view */
    assert(!ut_session_peek(&s, &r));
    assert(ut_session_failed(&s));
    puts("consumer sequence validation: PASS");
}

static void test_no_allocation_or_calls_in_render(void)
{
    /* Allocation/call freedom is proved on the ColdFire object by
     * scripts/test_i03_queue.sh; a host heap check cannot see a sanitizer's
     * lazy runtime allocation and is deliberately not used as evidence. */
    puts("render producer allocation: cross-object check in test_i03_queue.sh");
}

int main(void)
{
    test_dense_and_starts();
    test_congestion_preserves_order();
    test_adjacent_starts();
    test_overflow_forces_failure();
    test_machine_change_invalidates();
    test_reset_guard();
    test_sequence_fault_invalidates();
    test_no_allocation_or_calls_in_render();
    puts("SRC render queue: PASS");
    return 0;
}
