/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIDECAR_DT1_USBMIDI_MODEL_H
#define SIDECAR_DT1_USBMIDI_MODEL_H
#include <stdint.h>

/* Legacy byte-queue stand-in for portable staging/backpressure tests.
 * Occupancy here counts raw MIDI bytes, unlike the native USB-byte counter.
 * It is NOT an ABI, serializer, descriptor, DMA or native-capacity proof.
 * Production-adapter tests are tests/dt1_native.c; the compiled firmware
 * probe executes the actual serializer with modelled USB submission.
 * The CIN lengths below follow the native receiver / USB-MIDI format. */
#define DT1_READY        0x40610474u
#define DT1_ISR_ON       0xFC0B01C4u
#define DT1_WRAP_INDEX   0x4061047Cu
#define DT1_QUEUED       0x40610478u
#define DT1_STAGING_BASE 0x4BA84000u
#define DT1_STAGING_SIZE 0x1000u
#define DT1_WRAP_LIMIT   0xFBFu
#define DT1_WRAP_KEEP    0x3Fu      /* 0x400049CA: addi.l #-$3f: wait until queued < index - 0x3f */
#define DT1_QUEUE_LIMIT  0xFFBu     /* 0x40004A06: cmpi.l #$ffb */
/* Why the firmware's sender stopped early. */
enum {
    DT1_STOP_NONE = 0,
    DT1_STOP_NOT_READY = 1,
    DT1_STOP_WRAP_WAIT = 2,       /* would block at 0x400049D8 */
    DT1_STOP_CAPACITY_WAIT = 3    /* would block at 0x40004A0C */
};

struct dt1_usbmidi_model {
    uint8_t staging[DT1_STAGING_SIZE];
    uint32_t index;             /* DT1_WRAP_INDEX */
    uint32_t queued;            /* DT1_QUEUED: staged, not yet completed */
    uint8_t ready;              /* DT1_READY */
    uint8_t isr_on;             /* DT1_ISR_ON != 0 */
    /* Observability for tests and for the adapter's preflight. */
    uint32_t calls, refused_not_ready, wrap_waits, capacity_waits, wraps, staged;
    uint32_t last_stop;         /* why dt1_model_send returned short */
    uint32_t completions, completed_bytes;
    uint32_t max_queued;
};

void dt1_model_init(struct dt1_usbmidi_model *m);
/* Would the firmware's sender accept work now without entering either busy
 * wait? Our adapter only submits when this is true. */
int dt1_model_accepts(const struct dt1_usbmidi_model *m);
/* 0x40004990's queueing loop, including both busy waits. `spin_limit` bounds
 * the waits so the model terminates; each exhausted wait is counted and the
 * call fails like the firmware would never return. -> bytes accepted. */
uint32_t dt1_model_send(struct dt1_usbmidi_model *m, const uint8_t *msg,
                        uint32_t n, uint32_t spin_limit);
/* The USB completion callback's effect (0x40004404): bytes leave the queue. */
uint32_t dt1_model_complete(struct dt1_usbmidi_model *m, uint32_t n);
/* 0x40004760: de-frame one ISR call's worth of packets into the byte
 * streams. `out` receives at most `cap` bytes; -> count. */
uint32_t dt1_model_receive(struct dt1_usbmidi_model *m,
                           const uint8_t *packets, uint32_t packet_bytes,
                           uint8_t *out, uint32_t cap);
#endif
