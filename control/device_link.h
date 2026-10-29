/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIDECAR_DEVICE_LINK_H
#define SIDECAR_DEVICE_LINK_H
#include "link.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Simulated Digitakt-side sender for the host session/ACK state machine.
 * It is transport-neutral: callers pass complete SysEx frames. The replay
 * window keeps every START whose ACTIVATE has not been observed, so resend
 * after an ACK loss is byte-identical (same sequence and block). */

#define SC_DEVICE_REPLAY 128u

enum sc_device_state {
    SC_DEVICE_CLOSED = 0,
    SC_DEVICE_WAIT_HELLO_ACK = 1,
    SC_DEVICE_WAIT_ACTIVATE = 2,
    SC_DEVICE_ACTIVE = 3,
    SC_DEVICE_INVALID = 4
};

struct sc_device_update {
    uint8_t activated, invalidated, reason;
};

struct sc_device_replay {
    struct sc_frame frame;
    uint8_t used, compact;
};

struct sc_device_link {
    struct sc_device_replay replay[SC_DEVICE_REPLAY];
    uint32_t session, next_sequence, last_rx_block, last_tx_block;
    uint32_t hello_block;
    uint32_t accepted_sequence, accepted_block;
    uint8_t head, count;
    uint8_t state, reason;
    uint8_t audio_allowed;
};

/* Fresh nonzero session. Emits HELLO; no snapshot may precede HELLO_ACK. */
int sc_dev_open(struct sc_device_link *d, uint32_t session, uint32_t block,
                uint8_t out[SC_CONTROL_WIRE_SIZE]);
/* Emits a START only after HELLO_ACK. The frame's session/sequence/block are
 * owned here. The START is retained until its matching ACTIVATE is received. */
int sc_dev_send_start(struct sc_device_link *d, const struct sc_frame *source,
                      uint32_t block, uint8_t out[SC_WIRE_SIZE]);
/* Exact full-snapshot compact variant; returns actual wire size, or zero.
 * Legacy fallback preserves fractional/out-of-range SRC state exactly. */
unsigned sc_dev_send_compact_start(struct sc_device_link *d, const struct sc_frame *source,
                                  uint32_t block, uint8_t out[SC_WIRE_SIZE]);
/* Matching replay API, preserving original wire encoding and size. */
unsigned sc_dev_resend_frame(struct sc_device_link *d, uint8_t out[SC_WIRE_SIZE]);
/* Resends the oldest unacknowledged START without consuming a sequence. */
int sc_dev_resend_start(struct sc_device_link *d, uint8_t out[SC_WIRE_SIZE]);
/* Applies HELLO_ACK/ACTIVATE/DEACTIVATE/KEEPALIVE. Only ACTIVATE that names the
 * outstanding START+block opens the local gate. */
struct sc_device_update sc_dev_receive_control(struct sc_device_link *d,
                                               const struct sc_control *c,
                                               uint32_t current_block);
/* Device->host liveness, naming the last ACTIVATE-accepted snapshot. */
int sc_dev_keepalive(struct sc_device_link *d, uint32_t current_block,
                     uint8_t out[SC_CONTROL_WIRE_SIZE]);
/* Lease expiry and transport/queue overflow latch silence. DEACTIVATE is
 * emitted when a session was open. */
int sc_dev_tick(struct sc_device_link *d, uint32_t current_block,
                uint32_t lease, uint8_t out[SC_CONTROL_WIRE_SIZE]);
int sc_dev_invalidate(struct sc_device_link *d, uint8_t reason,
                      uint8_t out[SC_CONTROL_WIRE_SIZE]);
int sc_dev_audio_allowed(const struct sc_device_link *d);
const char *sc_dev_state_name(const struct sc_device_link *d);
#ifdef __cplusplus
}
#endif
#endif
