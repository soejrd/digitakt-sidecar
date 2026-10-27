/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "usbmidi.h"

#define barrier() __asm__ __volatile__ ("" ::: "memory")

/* Freestanding: the firmware build has no libc. */
static void copy_bytes(void *dst, const void *src, unsigned n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--)
        *d++ = *s++;
}

static void zero_bytes(void *dst, unsigned n)
{
    uint8_t *d = dst;
    while (n--)
        *d++ = 0;
}
#define load(p) (*(p))
#define store(p, v) (*(p) = (v))

/* OS 1.54's receive path reads CIN 0x4 as three data bytes and copies
 * (nibble + 1) bytes after the header (0x400047A8: d0 = 0x0f & cin, then
 * d0 = table[d0] with the table at 0x4017CE70 = 0,0,2,3,3,1,2,3,3,3,3,3,2,2,3,1).
 * It calls that copy with d0-1 and stops at the first byte whose high bit is
 * set, so status bytes survive but CIN 0x0/0x1 packets are not a general
 * carrier. The sender is the mirror (0x40004A1A onward). Anything we hand the
 * firmware as a carrier is therefore 4-byte CIN 0x4 packets, the SysEx CIN,
 * exactly as 0x40004990 writes them for a multi-byte message. */
#define CIN_SYSEX 0x4u

/* Data bytes a packet carries, per OS 1.54's receiver table
 * (0x4017CE70: 0,0,2,3,3,1,2,3,3,3,3,3,2,2,3,1). It is the USB-MIDI byte
 * count with 0x0/0x1 short-circuited to zero, and the firmware copies that
 * many bytes from +1 without inspecting them. The sender writes 3-byte
 * groups with CIN 0x4, so a continuation packet legitimately has no status
 * byte. */
static unsigned cin_len(uint8_t cin)
{
    static const uint8_t n[16] = {
        0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1
    };
    return n[cin & 0x0f];
}

/* The CIN a status byte's message needs, following USB-MIDI 1.0 table 4-1. */
static uint8_t cin_for(uint8_t status)
{
    uint8_t hi = status & 0xf0;
    if (status >= 0xf0) {
        switch (status) {
        case 0xf0: return 0x4;              /* SysEx start (with F0) */
        case 0xf1: case 0xf3: return 0x2;   /* MTC quarter frame, song select */
        case 0xf2: return 0x3;              /* song position pointer */
        case 0xf6: return 0x5;              /* tune request */
        case 0xf7: return 0x5;              /* EOX: CIN 0x5, one data byte */
        case 0xf8: case 0xfa: case 0xfb:
        case 0xfc: case 0xfe: case 0xff: return 0xf;
        default: return 0x0;                /* undefined system common */
        }
    }
    /* Channel voice: CIN is the high nibble; C and D carry one data byte. */
    if (hi == 0x80) return 0x8;
    if (hi == 0x90) return 0x9;
    if (hi == 0xa0) return 0xa;
    if (hi == 0xb0) return 0xb;
    if (hi == 0xc0) return 0xc;
    if (hi == 0xd0) return 0xd;
    if (hi == 0xe0) return 0xe;
    return 0x0;
}

int usbmidi_parse(struct usbmidi_packet *p, const uint8_t raw[USBMIDI_PACKET])
{
    unsigned n;
    if (!p || !raw)
        return 0;
    /* Only the header's high nibble is structural (cable number; must be 0
     * for the stream this adapter owns). The length is the firmware's own
     * table lookup, and its copy applies no further test, so neither does
     * this: a CIN 0x4 continuation packet legitimately begins with F0/F7 in
     * the first data position. */
    if (raw[0] & 0xf0)
        return 0;
    n = cin_len(raw[0]);
    if (!n)
        return 0;
    p->cin = raw[0];
    p->len = (uint8_t)n;
    p->data[0] = raw[1];
    p->data[1] = raw[2];
    p->data[2] = raw[3];
    return 1;
}

int usbmidi_packetize(uint8_t raw[USBMIDI_PACKET], const uint8_t *msg, unsigned n)
{
    uint8_t cin;
    if (!raw || !msg || n < 1 || n > 3 || !(msg[0] & 0x80))
        return 0;
    cin = cin_for(msg[0]);
    if (!cin)
        return 0;
    /* Fixed-length messages must be exactly as long as their CIN says; a
     * SysEx group may be short (the firmware pads the rest with zeroes, which
     * its receiver then forwards as data, so we only ever emit full groups
     * from usbmidi_scratch_push_message). */
    if (cin != 0x4 && (cin == 0x0 || cin == 0x1 ? 3u : cin_len(cin)) != n)
        return 0;
    raw[0] = cin;
    raw[1] = msg[0];
    raw[2] = n > 1 ? msg[1] : 0;
    raw[3] = n > 2 ? msg[2] : 0;
    return 1;
}

void usbmidi_scratch_init(struct usbmidi_scratch *s)
{
    if (s) {
        zero_bytes(s->raw, sizeof s->raw);
        store(&s->count, 0);
    }
}

int usbmidi_scratch_push(struct usbmidi_scratch *s,
                         const uint8_t raw[USBMIDI_PACKET])
{
    uint32_t n;
    if (!s || !raw)
        return 0;
    n = load(&s->count);
    if (n >= USBMIDI_SCRATCH)
        return 0;
    copy_bytes(s->raw + n * USBMIDI_PACKET, raw, USBMIDI_PACKET);
    /* Publish the payload before the index: the reader only looks at packets
     * below count. */
    barrier();
    store(&s->count, n + 1);
    return 1;
}

int usbmidi_scratch_push_message(struct usbmidi_scratch *s,
                                 const uint8_t *msg, unsigned n,
                                 uint8_t *work, unsigned work_size)
{
    uint8_t packet[USBMIDI_PACKET];
    unsigned i, need;
    uint32_t have;
    if (!s || !msg || !n)
        return 0;
    (void)work;
    (void)work_size;
    need = (n + 2) / 3;
    have = load(&s->count);
    if (need > USBMIDI_SCRATCH - have)
        return 0;
    /* The firmware's own SysEx serializer splits a message into 4-byte CIN
     * 0x4 packets without interpreting the payload (0x40004A1A..0x40004C12),
     * and its receiver copies the three data bytes of each CIN 0x4 packet
     * into the SysEx staging area. Validate only the first status byte so an
     * arbitrary 7-bit SysEx body is carried whole; refuse the message rather
     * than stage part of it. */
    /* F0..F7 may contain status bytes: the frame itself starts at F0 and
     * ends at F7, and that is what the firmware's sender transmits verbatim.
     * Anything else must be 7-bit. */
    if (msg[0] != 0xf0 || msg[n - 1] != 0xf7)
        return 0;
    for (i = 1; i + 1 < n; ++i)
        if (msg[i] & 0x80)
            return 0;
    for (i = 0; i < n; i += 3) {
        unsigned take = n - i < 3 ? n - i : 3;
        zero_bytes(packet, USBMIDI_PACKET);
        packet[0] = CIN_SYSEX;
        copy_bytes(packet + 1, msg + i, take);
        if (!usbmidi_scratch_push(s, packet))
            return 0;
    }
    return 1;
}

void usbmidi_bytes_init(struct usbmidi_bytes *q, uint8_t *buf, uint32_t size)
{
    if (!q)
        return;
    q->buf = buf;
    q->size = size;
    q->head = 0;
    q->tail = 0;
    q->dropped = 0;
}

uint32_t usbmidi_bytes_used(const struct usbmidi_bytes *q)
{
    return q ? q->head - q->tail : 0;
}

static int bytes_room(const struct usbmidi_bytes *q, uint32_t n)
{
    return q && q->buf && q->size && q->head - q->tail + n <= q->size;
}

static void bytes_put(struct usbmidi_bytes *q, const uint8_t *p, uint32_t n,
                      int *ok)
{
    uint32_t i;
    if (!bytes_room(q, n)) {
        *ok = 0;
        return;
    }
    for (i = 0; i < n; ++i)
        q->buf[(q->head + i) % q->size] = p[i];
    q->head += n;
}

static int message_length(const struct usbmidi_bytes *q, uint32_t at,
                          uint32_t avail, uint32_t *len)
{
    uint8_t b;
    uint32_t i = 0;
    if (!avail)
        return 0;
    b = q->buf[(at + i) % q->size];
    if (b == 0xf0) {
        for (i = 1; i < avail; ++i) {
            uint8_t c = q->buf[(at + i) % q->size];
            if (c == 0xf7) {
                *len = i + 1;
                return 1;
            }
            if ((c & 0x80) && c < 0xf8)
                return 0;           /* malformed: a new status inside SysEx */
        }
        return 0;                   /* still incomplete */
    }
    if (!(b & 0x80))
        return 0;
    if (b >= 0xf8) {
        *len = 1;
        return 1;
    }
    if (b >= 0xf4 && b <= 0xf6) {
        *len = 1;
        return avail >= 1;
    }
    if (b == 0xf7) {
        *len = 1;
        return 1;
    }
    if (b >= 0xf0) {
        uint32_t need = (b == 0xf1 || b == 0xf3) ? 2 : (b == 0xf2 ? 3 : 1);
        if (avail < need)
            return 0;
        *len = need;
        return 1;
    }
    {
        uint8_t hi = b & 0xf0;
        uint32_t need = (hi == 0xc0 || hi == 0xd0) ? 2 : 3;
        if (avail < need)
            return 0;
        *len = need;
        return 1;
    }
}

uint32_t usbmidi_messages(const struct usbmidi_bytes *q)
{
    uint32_t used, off, n = 0, len;
    if (!q)
        return 0;
    used = q->head - q->tail;
    for (off = 0; off < used; off += len) {
        if (!message_length(q, q->tail + off, used - off, &len))
            break;
        ++n;
    }
    return n;
}

uint32_t usbmidi_pop_message(struct usbmidi_bytes *q, uint8_t *out,
                             uint32_t out_size)
{
    uint32_t used, len, i;
    if (!q || !out)
        return 0;
    used = q->head - q->tail;
    if (!message_length(q, q->tail, used, &len) || len > out_size)
        return 0;
    for (i = 0; i < len; ++i)
        out[i] = q->buf[(q->tail + i) % q->size];
    q->tail += len;
    if (q->tail == q->head) {
        q->tail = 0;
        q->head = 0;
    }
    return len;
}

uint32_t usbmidi_drain(struct usbmidi_bytes *q, struct usbmidi_scratch *s,
                       unsigned budget)
{
    uint32_t count, moved = 0;
    struct usbmidi_packet p;
    int ok = 1;
    if (!q || !s)
        return 0;
    count = load(&s->count);
    barrier();
    while (moved < count && moved < budget) {
        const uint8_t *raw = s->raw + moved * USBMIDI_PACKET;
        if (!usbmidi_parse(&p, raw)) {
            q->dropped += 1;
            ++moved;
            continue;
        }
        /* OS 1.54's sender writes every SysEx group as a 4-byte CIN 0x4
         * packet, so a message that does not end on a 3-byte boundary is
         * padded with zeroes (0x40004BEC: it fills the group and clears the
         * rest). Nothing may follow EOX inside a packet, so trim at F7. */
        if ((p.cin == 0x4 || p.cin == 0x6 || p.cin == 0x7) && p.len) {
            unsigned k;
            for (k = 0; k < p.len; ++k) {
                if (p.data[k] == 0xf7) {
                    p.len = (uint8_t)(k + 1);
                    break;
                }
            }
        }
        if (!p.len)
            continue;
        if (!bytes_room(q, p.len)) {
            ok = 0;
            break;
        }
        bytes_put(q, p.data, p.len, &ok);
        if (!ok)
            break;
        ++moved;
    }
    if (moved) {
        uint32_t left = count - moved;
        if (left)
            copy_bytes(s->raw, s->raw + moved * USBMIDI_PACKET,
                       left * USBMIDI_PACKET);
        barrier();
        store(&s->count, left);
    }
    return moved;
}

uint32_t usbmidi_rx_put(struct usbmidi_bytes *q, const uint8_t *data,
                        uint32_t n)
{
    uint32_t i, taken = 0;
    if (!q || !data)
        return 0;
    for (i = 0; i < n; ++i) {
        if (!bytes_room(q, 1)) {
            q->dropped += n - i;
            break;
        }
        q->buf[q->head % q->size] = data[i];
        q->head += 1;
        ++taken;
    }
    return taken;
}

uint32_t usbmidi_rx_get(struct usbmidi_bytes *q, uint8_t *out, uint32_t n)
{
    uint32_t used, i, taken;
    if (!q || !out)
        return 0;
    used = q->head - q->tail;
    taken = used < n ? used : n;
    for (i = 0; i < taken; ++i)
        out[i] = q->buf[(q->tail + i) % q->size];
    q->tail += taken;
    if (q->tail == q->head) {
        q->tail = 0;
        q->head = 0;
    }
    return taken;
}
