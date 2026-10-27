/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "dt1_usbmidi_model.h"
#include <string.h>

/* OS 1.54's receive copy length, per CIN (0x4017CE70). */
static const uint8_t cin_copy[16] = {
    0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1
};

void dt1_model_init(struct dt1_usbmidi_model *m)
{
    if (!m)
        return;
    memset(m, 0, sizeof *m);
}

int dt1_model_accepts(const struct dt1_usbmidi_model *m)
{
    if (!m || !m->ready || !m->isr_on)
        return 0;
    if (m->queued > DT1_QUEUE_LIMIT)
        return 0;
    return 1;
}

uint32_t dt1_model_send(struct dt1_usbmidi_model *m, const uint8_t *msg,
                        uint32_t n, uint32_t spin_limit)
{
    uint32_t i, accepted = 0;
    (void)spin_limit;
    if (!m || !msg)
        return 0;
    ++m->calls;
    if (!m->ready || !m->isr_on) {
        ++m->refused_not_ready;
        m->last_stop = DT1_STOP_NOT_READY;
        return 0;
    }
    if (!n)
        return 0;
    m->last_stop = DT1_STOP_NONE;
    /* Only a message longer than 0x2F bytes evaluates the wrap condition
     * (0x400049B6..0x400049BA). The wait at 0x400049D8 cannot end inside
     * this call: only a USB completion lowers `queued`. The model therefore
     * reports the would-be wait and stops, as our adapter must never enter
     * it. */
    if (n > 0x2F && m->index > DT1_WRAP_LIMIT) {
        uint32_t floor = m->index - DT1_WRAP_KEEP;
        if (m->queued >= floor) {
            ++m->wrap_waits;
            m->last_stop = DT1_STOP_WRAP_WAIT;
            return 0;
        }
        m->index = 0;
        ++m->wraps;
    }
    for (i = 0; i < n; ++i) {
        /* 0x400049FE..0x40004A0C: d1 = queued + staged, block while > 0xFFB. */
        if (m->queued >= DT1_QUEUE_LIMIT) {
            ++m->capacity_waits;
            m->last_stop = DT1_STOP_CAPACITY_WAIT;
            break;
        }
        m->staging[m->index] = msg[i];
        m->index = (m->index + 1) & (DT1_STAGING_SIZE - 1);
        ++m->queued;
        ++m->staged;
        ++accepted;
        if (m->queued > m->max_queued)
            m->max_queued = m->queued;
    }
    return accepted;
}

uint32_t dt1_model_complete(struct dt1_usbmidi_model *m, uint32_t n)
{
    if (!m)
        return 0;
    if (n > m->queued)
        n = m->queued;
    m->queued -= n;
    ++m->completions;
    m->completed_bytes += n;
    return n;
}

uint32_t dt1_model_receive(struct dt1_usbmidi_model *m,
                           const uint8_t *packets, uint32_t packet_bytes,
                           uint8_t *out, uint32_t cap)
{
    uint32_t i, w = 0;
    (void)m;
    if (!packets || !out)
        return 0;
    for (i = 0; i + 4 <= packet_bytes && w < cap; i += 4) {
        const uint8_t *p = packets + i;
        unsigned take, k;
        if (p[0] & 0xf0)
            continue;
        take = cin_copy[p[0] & 0x0f];
        /* 0x400047C4..0x400047DA copies `take` data bytes blindly: no bit-7
         * test, so a SysEx status byte is carried like any other. */
        for (k = 0; k < take && w < cap; ++k)
            out[w++] = p[1 + k];
    }
    return w;
}
