/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef USB_TRACK_SRC_QUEUE_H
#define USB_TRACK_SRC_QUEUE_H
#include <stdint.h>

/* One producer at render interrupt level, one consumer in the transport/UI
 * task, on the single-core Digitakt. Indices are volatile and separated from
 * payload stores by compiler barriers. Open/close set a reset guard that makes
 * an entering render interrupt bail before it touches the ring, so the UI may
 * rearm without waiting. Fixed storage; a lost record is never skipped. Queue
 * faults latch silence while a machine change merely invalidates the session,
 * matching the handshake's MACHINE_SELECT rule. */
#define UT_QUEUE_CAPACITY 256u
#define UT_START 1u

enum {
    UT_CAPTURE_FAILED = -1,     /* fatal: force silence until a fresh session */
    UT_CAPTURE_IDLE = 0,        /* no control session: leave audio alone */
    UT_CAPTURE_OK = 1
};

struct ut_record {
    uint32_t session;
    uint32_t block;             /* session-local render block, from 0 */
    uint32_t sequence;          /* from 1, never wraps within a session */
    int32_t note;               /* Digitakt 16.16 */
    int16_t params[8];          /* final render A-H, signed 8.8 */
    int16_t velocity;           /* signed 8.8 */
    uint8_t machine;
    uint8_t flags;              /* UT_START */
};

struct ut_queue {
    struct ut_record ring[UT_QUEUE_CAPACITY];
    volatile uint32_t head;     /* producer-owned */
    volatile uint32_t tail;     /* consumer-owned */
    volatile uint32_t overflow; /* diagnostic only after fatal */
};

struct ut_session {
    struct ut_queue queue;
    uint32_t id;
    uint32_t block;             /* producer-owned */
    uint32_t next;              /* producer-owned */
    uint32_t consume_next;      /* consumer-owned */
    uint8_t machine;            /* producer-owned */
    uint8_t have_machine;
    volatile uint8_t reset;     /* UI-owned: render must bail while set */
    volatile uint8_t valid;
    volatile uint8_t fatal;
};

/* Open a fresh nonzero session from the UI task. A render interrupt that
 * enters during the reset sees reset=1 and returns without producing. */
int ut_session_open(struct ut_session *s, uint32_t session);
int ut_session_ready(const struct ut_session *s);
/* Render side: publishes one full record. */
int ut_session_capture(struct ut_session *s, uint8_t machine,
                       const int16_t params[8], int32_t note, int16_t velocity,
                       uint8_t start);
/* Render side: one record per audio block. A machine change invalidates the
 * session without latching fatal silence; a full ring or exhausted sequence
 * is fatal. Check ut_session_room() before gathering final parameters. */
int ut_session_room(struct ut_session *s);
/* Deliberate disarm from the UI task: no fault is latched. */
void ut_session_close(struct ut_session *s);
/* Machine change: stop producing and retire the session, but leave audio
 * unmuted so the new machine can be selected; a fresh session is required. */
void ut_session_invalidate(struct ut_session *s);
int ut_session_invalidated(const struct ut_session *s);
/* Latch after overflow or ordering loss. Once fatal, the
 * caller must use ut_session_close() before opening a fresh session. */
void ut_session_fail(struct ut_session *s);
int ut_session_failed(const struct ut_session *s);

/* Consumer side: oldest record first. A sequence mismatch also latches fatal. */
int ut_session_peek(struct ut_session *s, struct ut_record *out);
int ut_session_pop(struct ut_session *s, struct ut_record *out);

/* A bounded fake transport. The UI task drains it once per tick; the render
 * task never calls it. The sink may reject a record to simulate congestion;
 * a rejected record remains in flight and blocks later records in order.
 * The in-flight copy must be at least as large as struct ut_record. */
struct ut_fake_transport {
    struct ut_record in_flight;
    uint32_t sent;
    uint8_t busy;
};
void ut_fake_transport_init(struct ut_fake_transport *t);
unsigned ut_fake_transport_drain(struct ut_session *s, struct ut_fake_transport *t,
                                 unsigned budget,
                                 int (*sink)(void *ctx, const struct ut_record *r),
                                 void *ctx);
#endif
