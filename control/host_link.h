/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIDECAR_HOST_LINK_H
#define SIDECAR_HOST_LINK_H
#include "link.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Protocol roles in this repository's USB topology:
 *   host   = Pi/Circle receiver, parameter installer, ACK sender and PCM gate
 *   device = simulated Digitakt sender of snapshots and keepalives
 *
 * The host is silent by default. HELLO opens a pending session but never the
 * PCM gate. A valid START is installed first; ACTIVATE must then be queued
 * successfully. Only then does the Pi open its gate; the Digitakt opens its
 * own gate only after receiving that matching ACTIVATE. Any sequence gap,
 * conflicting duplicate, malformed frame, queue overflow, timeout or
 * reconnect closes the gate and clears stale audio state. A byte-identical
 * snapshot replay re-emits ACTIVATE without
 * reinstalling. Timeouts use the host audio-block clock, not uptime.
 */

#define SC_HOST_RECENT_SESSIONS 8u
#define SC_HOST_PENDING_NONE 0u
#define SC_HOST_PENDING_ACTIVATE 1u

enum sc_host_state {
    SC_HOST_SILENT = 0,
    SC_HOST_HEARD = 1,
    SC_HOST_ACTIVE = 2,
    SC_HOST_INVALID = 3
};

struct sc_host_update {
    uint8_t activated, invalidated, reason;
};

typedef int (*sc_install_fn)(void *ctx, const struct sc_frame *state);
typedef void (*sc_clear_fn)(void *ctx);

struct sc_host {
    struct sc_receiver rx;
    uint32_t session, hello_sequence, hello_block;
    uint32_t last_sequence, last_block;
    uint32_t confirmed_sequence, confirmed_block;
    uint32_t last_rx_block;
    uint32_t pending_sequence, pending_block;
    uint32_t recent_closed[SC_HOST_RECENT_SESSIONS];
    uint8_t recent_count, recent_next;
    uint8_t state, reason, pending, hello_ack_sent, reply_ready;
    sc_install_fn install;
    sc_clear_fn clear;
    void *ctx;
};

/* install runs before ACTIVATE; clear runs on every invalidation and must only
 * zero bounded stale audio/parameter state. Neither may allocate or wait. */
void sc_host_init(struct sc_host *h, sc_install_fn install,
                  sc_clear_fn clear, void *ctx);
/* HELLO/DEACTIVATE/device KEEPALIVE. Returns 1 when out holds any reply,
 * including a DEACTIVATE for a fault;
 * call sc_host_reply_sent with the nonblocking sender's result. */
int sc_host_receive_control(struct sc_host *h, const struct sc_control *c,
                            uint32_t current_block,
                            uint8_t out[SC_CONTROL_WIRE_SIZE]);
/* Validates/installs a snapshot and emits ACTIVATE, or DEACTIVATE on a fault.
 * A nonzero return always means out holds a complete reply. The first snapshot of a
 * session must carry SC_START. The gate stays closed until sc_host_reply_sent
 * records a successful queue. */
int sc_host_receive_start(struct sc_host *h, const struct sc_frame *f,
                          uint32_t current_block,
                          uint8_t out[SC_CONTROL_WIRE_SIZE]);
/* Records the result of sending the reply just emitted. queued=0 closes the
 * gate with reason; queued=1 completes HELLO_ACK or opens the PCM gate for
 * ACTIVATE. */
int sc_host_reply_sent(struct sc_host *h, int queued, uint8_t reason,
                       uint32_t current_block);
/* Host->device liveness, naming the last accepted sequence; ACTIVE only. */
int sc_host_keepalive(struct sc_host *h, uint32_t current_block,
                      uint8_t out[SC_CONTROL_WIRE_SIZE]);
/* lease elapsed without a valid device frame: close and emit DEACTIVATE. */
int sc_host_tick(struct sc_host *h, uint32_t current_block, uint32_t lease,
                 uint8_t out[SC_CONTROL_WIRE_SIZE]);
int sc_host_invalidate(struct sc_host *h, uint8_t reason,
                       uint8_t out[SC_CONTROL_WIRE_SIZE]);
int sc_host_audio_allowed(const struct sc_host *h);
/* Optional synchronous reader for HTTP/status; does not mutate state. */
const char *sc_host_state_name(const struct sc_host *h);
#ifdef __cplusplus
}
#endif
#endif
