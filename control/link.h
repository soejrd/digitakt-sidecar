/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIDECAR_LINK_H
#define SIDECAR_LINK_H
#include <stdint.h>
/* One audio track. Full state travels with every start: no separate parameter
 * packet can arrive too late for its trigger. Caller supplies a fresh nonzero
 * session on boot/reconnect; sequence numbers never wrap within a session. */
#define SC_WIRE_SIZE 61
#define SC_START 1u
struct sc_frame {
    uint32_t session, sequence, block, voice;
    int32_t note;                 /* Digitakt 16.16; no pitch policy here */
    int16_t params[8], velocity;  /* Digitakt signed 8.8 */
    uint8_t flags;
};
struct sc_receiver {
    struct sc_frame state;
    uint32_t session, next, last_block;
    uint8_t active;
};
/* Experimental noncommercial SysEx ID 0x7d, signature DTSC, version 1.
 * No packed C structs go on the wire. Returns zero on invalid input. */
int sc_encode(uint8_t out[SC_WIRE_SIZE], const struct sc_frame *f);
int sc_decode(struct sc_frame *f, const uint8_t *in, unsigned size);
void sc_reset(struct sc_receiver *r, uint32_t session);
/* An invalid/missing/out-of-order frame latches silence until a new session.
 * Apply state.params before acting on SC_START. ACK only accepted frames. */
int sc_accept(struct sc_receiver *r, const struct sc_frame *f);
/* current_block must use the sender's block-clock domain, not host uptime. */
int sc_expire(struct sc_receiver *r, uint32_t current_block, uint32_t lease);
#endif
