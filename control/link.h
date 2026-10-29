/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIDECAR_LINK_H
#define SIDECAR_LINK_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* One audio track. Full state travels with every start: no separate parameter
 * packet can arrive too late for its trigger. Caller supplies a fresh nonzero
 * session on boot/reconnect; sequence numbers never wrap within a session. */
#define SC_WIRE_SIZE 61
#define SC_COMPACT_WIRE_SIZE 45
#define SC_START 1u

/* Session control frames are a separate, additive protocol version. They do
 * not change the v1 START snapshot above. The 24-byte layout is:
 * F0 7D 'D' 'T' 'S' 'C' 02 kind session(5x7) sequence(5x7) value(5x7) F7.
 * Numeric fields are explicit little-endian 7-bit groups (35 bits available,
 * top group <= 15) and are rejected when overlong; no packed structs. */
#define SC_CONTROL_WIRE_SIZE 24
#define SC_CONTROL_VERSION 2u

enum sc_control_kind {
    SC_HELLO = 1,       /* device (Digitakt) -> host (Pi): begin a session */
    SC_HELLO_ACK = 2,   /* host (Pi) -> device: session accepted */
    SC_ACTIVATE = 3,    /* host (Pi) -> device: START installed, output may flow */
    SC_DEACTIVATE = 4,  /* either direction: close and require a new session */
    SC_KEEPALIVE = 5    /* host: accepted START/block; device: startup confirm */
};

enum sc_deactivate_reason {
    SC_REASON_NONE = 0,
    SC_REASON_TIMEOUT = 1,
    SC_REASON_SEQUENCE = 2,
    SC_REASON_OVERFLOW = 3,
    SC_REASON_RECONNECT = 4,
    SC_REASON_INSTALL = 5,
    SC_REASON_MALFORMED = 6,
    SC_REASON_SESSION = 7,
    SC_REASON_TRANSPORT = 8
};

struct sc_frame {
    uint32_t session, sequence, block, voice; /* voice = hit generation, NOT track ID */
    int32_t note;                 /* Digitakt 16.16; no pitch policy here */
    int16_t params[8], velocity;  /* Digitakt signed 8.8 */
    uint8_t flags;
};
struct sc_receiver {
    struct sc_frame state;
    uint32_t session, next, last_block;
    uint8_t active;
};
struct sc_control {
    uint32_t session, sequence, value;
    uint8_t kind;
};
/* Experimental noncommercial SysEx ID 0x7d, signature DTSC, version 1.
 * No packed C structs go on the wire. Returns zero on invalid input. */
int sc_encode(uint8_t out[SC_WIRE_SIZE], const struct sc_frame *f);
/* Version 3 retains the entire snapshot and every identity/timestamp field.
 * Only integer 0..127 SRC values get one-byte encoding; otherwise exact v1
 * fallback. Returns wire size (45 or 61), or zero. Output capacity is 61. */
unsigned sc_encode_compact(uint8_t out[SC_WIRE_SIZE], const struct sc_frame *f);
int sc_decode(struct sc_frame *f, const uint8_t *in, unsigned size);
/* Version 2 session control. Returns zero on invalid/unknown input. */
int sc_encode_control(uint8_t out[SC_CONTROL_WIRE_SIZE],
                      const struct sc_control *c);
int sc_decode_control(struct sc_control *c, const uint8_t *in, unsigned size);
/* True when the DTSC signature/version is present, including malformed
 * control-length messages that a receiver should report as malformed. */
int sc_is_dtsc(const uint8_t *in, unsigned size);
void sc_reset(struct sc_receiver *r, uint32_t session);
/* An invalid/missing/out-of-order frame latches silence until a new session.
 * Apply state.params before acting on SC_START. ACK only accepted frames. */
int sc_accept(struct sc_receiver *r, const struct sc_frame *f);
/* current_block must use the sender's block-clock domain, not host uptime. */
int sc_expire(struct sc_receiver *r, uint32_t current_block, uint32_t lease);
#ifdef __cplusplus
}
#endif
#endif
