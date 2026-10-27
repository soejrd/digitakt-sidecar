/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIDECAR_CIRCLE_LINK_ADAPTER_H
#define SIDECAR_CIRCLE_LINK_ADAPTER_H
#include <stdint.h>
#include "host_link.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Circle-shaped glue. It deliberately has no Circle, synthesis or USB header
 * dependency, so the same code is host-tested with control/circle_shim/.
 *
 * Callback context (Circle packet handler / xHCI):
 *   sc_circle_midi_byte: collect one complete F0..F7 frame, validate DTSC,
 *                and enqueue it. Never render, send, install parameters, clear
 *                PCM or wait. Queue overflow immediately latches silence.
 * Task/ev_tick context:
 *   sc_circle_process: parse bounded queued frames; install parameters before
 *                ACTIVATE; send replies; require the device's KEEPALIVE before
 *                opening the PCM gate. Timeout and lost-transport DEACTIVATE
 *                are emitted here.
 * Render context:
 *   AudioAllowed: one atomic read; no locks, allocations or calls.
 */

#define SC_CIRCLE_RX_QUEUE 8u
#define SC_CIRCLE_SYSEX_MAX 64u

typedef int (*sc_circle_send_fn)(void *ctx, const uint8_t *frame,
                                 uint32_t size);
typedef void (*sc_circle_clear_fn)(void *ctx);

struct sc_circle_frame {
    uint8_t bytes[SC_CIRCLE_SYSEX_MAX];
    uint8_t size;
};

struct sc_circle_link {
    struct sc_host host;
    struct sc_circle_frame rx[SC_CIRCLE_RX_QUEUE];
    uint32_t rx_read, rx_write;
    uint8_t sysex[SC_CIRCLE_SYSEX_MAX];
    uint8_t sysex_size, sysex_overflow;
    uint8_t gate, lost_reason;
    uint32_t block, lease;
    sc_circle_send_fn send;
    sc_circle_clear_fn clear;
    void *send_ctx;
};

/* The install callback belongs to the adapter owner: it writes preallocated
 * synthesis parameters, never allocates, and is called from Process. */
void sc_circle_init(struct sc_circle_link *l, sc_install_fn install,
                    sc_circle_send_fn send, sc_circle_clear_fn clear,
                    void *ctx, uint32_t lease_blocks);
/* Callback context: MIDI bytes need not respect USB-MIDI packet boundaries. */
void sc_circle_midi_byte(struct sc_circle_link *l, uint8_t byte);
/* Task context: returns frames processed. A timeout or callback-side transport
 * loss emits DEACTIVATE once and closes the gate. */
unsigned sc_circle_process(struct sc_circle_link *l, unsigned budget);
/* Task context after a validated incoming block completes. */
void sc_circle_advance_block(struct sc_circle_link *l, uint32_t block);
/* Callback context: latch silence immediately on transport/queue failure. The
 * task later emits DEACTIVATE; this only changes the gate. */
void sc_circle_transport_lost(struct sc_circle_link *l, uint8_t reason);
/* Render context: lock-free gate. */
int sc_circle_audio_allowed(const struct sc_circle_link *l);
/* Test/status accessors. */
unsigned sc_circle_rx_queued(const struct sc_circle_link *l);
#ifdef __cplusplus
}
#endif
#endif
