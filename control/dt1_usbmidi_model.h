/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIDECAR_DT1_USBMIDI_MODEL_H
#define SIDECAR_DT1_USBMIDI_MODEL_H
#include <stdint.h>

/* A host-side model of the Digitakt mk1 OS 1.54 raw-MIDI queue, written from
 * the disassembly rather than from a guess. It exists to test our adapter's
 * ownership rules against the firmware's real observable behaviour on the
 * paths we intend to use, not to replace the firmware.
 *
 * Firmware facts encoded here (MAIN sha256 5c58bf9e..., all addresses from
 * evidence/i03/transport-154.json):
 *
 *   sender 0x40004990  - refuses unless `ready` (0x40610474) == 1 and the
 *                        USB clopper ISR is on (0xFC0B01C4 != 0)
 *                      - for a message longer than 0x2F bytes, wraps the
 *                        4096-byte staging ring (0x4BA84000 + index
 *                        0x4061047C) when index > 0xFBF: a busy wait until
 *                        queued < index - 0x3F (0x400049CA..0x400049D8), then
 *                        index = 0. Our adapter never sends a message that
 *                        long, so this path is unreachable from it
 *                      - second busy wait, per byte, while queued exceeds
 *                        0xFFB (0x400049FE..0x40004A0C)
 *                      - the queue drains only from a USB completion callback
 *                        (0x40610478 is subtracted in 0x40004404), so a
 *                        caller that spins can never make progress itself
 *                      - returns without queueing when the loop cannot run
 *   receiver 0x40004760 - pulls 4-byte packets from the ISR ring and copies
 *                        table[cin & 0xf] data bytes (longs at 0x4017CE70:
 *                        0,0,2,3,3,1,2,3,3,3,3,3,2,2,3,1 - the USB-MIDI data
 *                        byte counts) with no bit-7 test
 *                      - bit 4 of the packet header selects the stream: cable
 *                        0 packets (headers 0x00-0x0f, our SysEx CIN 0x4) go
 *                        to 0x4061015C and its handler slot 0x4061046C; cable
 *                        1 packets go to 0x406102DC / slot 0x40610470
 *   callback slots 0x4061046C (cable 0) and 0x40610470 (cable 1) are set at
 *   boot from 0x40069128 by 0x400045A6 to 0x400D3298 and 0x400D34CC, and are
 *   invoked with (ptr, count) from 0x40004808. */
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
