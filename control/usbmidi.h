/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIDECAR_USBMIDI_H
#define SIDECAR_USBMIDI_H
#include <stdint.h>

/* Device-neutral USB-MIDI 1.0 packet layer plus the bounded byte queues an
 * interrupt-level producer and a task-level transport share.
 *
 * Ownership
 *   - The producer (render interrupt) only calls usbmidi_pack(), which writes
 *     whole 4-byte USB-MIDI event packets into a fixed scratch buffer. It
 *     never touches a queue, a descriptor or the firmware.
 *   - The transport task owns every queue head/tail update: it drains the
 *     scratch packets into a byte ring, and it pops host bytes out of the
 *     receive ring. Single writer per index; indices are volatile and every
 *     access in the task is ordered, so the producer's commit is visible
 *     before its index store.
 *
 * Everything here is allocation-free and bounded: a full queue refuses the
 * record and reports the count, it never overwrites or waits. */

/* USB-MIDI 1.0 Code Index Numbers: the number of bytes after the header.
 * 0x0 and 0x1 are 3 bytes (system common, the second is undefined); 0x4, 0x5,
 * 0x6, 0x7 are 2 (SysEx start/end and 7-bit system common); 0x2, 0x3, 0x8-0xe
 * are 3; 0xf is 1 (single byte). */
#define USBMIDI_PACKET 4u
#define USBMIDI_QUEUE 4096u          /* matches the firmware's SysEx staging area */
#define USBMIDI_SCRATCH 256u         /* packets the producer may stage per block */

/* A parsed 4-byte packet. `cin` is the code index, `len` 1-3 data bytes. */
struct usbmidi_packet {
    uint8_t cin, len, data[3];
};

/* Split one 4-byte packet. Refuses a header with a nonzero high nibble, a
 * data byte with bit 7 set, and CIN/status mismatches. -> 1 if valid. */
int usbmidi_parse(struct usbmidi_packet *p, const uint8_t raw[USBMIDI_PACKET]);
/* Build a packet from up to three status/data bytes. Chooses the narrowest
 * legal CIN from the status byte. -> 1 if valid, 0 otherwise. */
int usbmidi_packetize(uint8_t raw[USBMIDI_PACKET], const uint8_t *msg, unsigned n);

/* A run of MIDI bytes with no packet framing (what the codec sees). */
struct usbmidi_bytes {
    uint8_t *buf;
    uint32_t size, head, tail, dropped;
};

/* ---- producer side: render interrupt. No queue access, no waiting. ---- */
struct usbmidi_scratch {
    uint8_t raw[USBMIDI_SCRATCH * USBMIDI_PACKET];
    volatile uint32_t count;         /* committed packets */
};
void usbmidi_scratch_init(struct usbmidi_scratch *s);
/* Append one packet. Refuses (returns 0) when full; the caller drops the
 * message whole rather than truncating it. */
int usbmidi_scratch_push(struct usbmidi_scratch *s,
                         const uint8_t raw[USBMIDI_PACKET]);
/* Append a whole message as consecutively packed CIN 0x4 packets, as the
 * firmware's own serializer does for SysEx. All or nothing. */
int usbmidi_scratch_push_message(struct usbmidi_scratch *s,
                                 const uint8_t *msg, unsigned n,
                                 uint8_t *work, unsigned work_size);

/* ---- transport task side: move scratch into the byte ring, in order. ---- */
void usbmidi_bytes_init(struct usbmidi_bytes *q, uint8_t *buf, uint32_t size);
/* Move up to `budget` committed packets from the scratch to the ring and
 * clear the scratch. A packet that does not fit stops the drain: no partial
 * record is ever moved, so no message can be reordered or split. */
uint32_t usbmidi_drain(struct usbmidi_bytes *q, struct usbmidi_scratch *s,
                       unsigned budget);
/* Number of complete MIDI messages available at the head of the ring. */
uint32_t usbmidi_messages(const struct usbmidi_bytes *q);
/* Pop one complete message (no framing) into out. Returns its length, or 0
 * when the head is not a complete message. Never splits a SysEx. */
uint32_t usbmidi_pop_message(struct usbmidi_bytes *q, uint8_t *out,
                             uint32_t out_size);
/* Bytes currently held in the ring (queued but not yet consumed). */
uint32_t usbmidi_bytes_used(const struct usbmidi_bytes *q);

/* Receive side also needs the reverse: a host byte stream arrives as bytes,
 * the task collects it in the same ring shape and the parser consumes it. */
uint32_t usbmidi_rx_put(struct usbmidi_bytes *q, const uint8_t *data,
                        uint32_t n);
uint32_t usbmidi_rx_get(struct usbmidi_bytes *q, uint8_t *out, uint32_t n);

#endif
