/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIDECAR_DT1_TRANSPORT_H
#define SIDECAR_DT1_TRANSPORT_H
#include <stdint.h>

/* Digitakt mk1 OS 1.54 USB-MIDI transport adapter.
 *
 * The device side has one rule: never call the firmware's raw-MIDI sender
 * from render, and never enter its busy waits. This adapter therefore owns
 * the firmware interaction and exposes a small, bounded, nonblocking surface
 * to the rest of the mod:
 *
 *   render (interrupt)  -> dt1_tx_stage()    scratch only, never blocks
 *   UI/tick task        -> dt1_tx_service()  packs and submits to firmware
 *                       -> dt1_rx_service()  drains received bytes
 *
 * The firmware calls (hooks) are supplied by the caller, so this file has no
 * firmware addresses and is host-testable. mods/usb-midi supplies the OS
 * 1.54 implementation. */
#include "usbmidi.h"
#include "sysex_split.h"

#define DT1_TX_RING   64u           /* staged messages between render and task */
#define DT1_MSG_MAX   96u           /* longest message: v1 START is 61 bytes */
/* The firmware takes messages longer than 0x2F bytes down its wrap path
 * (0x400049B6), which busy-waits when index > 0xFBF and queued reaches
 * index - 0x3F. Both the v1 snapshot (61) and the v2 control frame (24) are
 * submitted as one whole message; can_send() refuses whenever that wait would
 * occur, so the message is deferred instead. */
#define DT1_RX_FRAME  1024u         /* SysEx reassembly, one frame at a time */

enum dt1_tx_result {
    DT1_TX_OK = 0,          /* staged and, from the task, submitted */
    DT1_TX_DEFERRED = 1,    /* staged; firmware busy, retry from the task */
    DT1_TX_DROPPED = 2,     /* whole message refused: queue full or too long */
    DT1_TX_LINK_DOWN = 3    /* no live USB link */
};

/* The firmware-facing operations, one per documented OS 1.54 behaviour. */
struct dt1_fw_ops {
    /* USB link up and the raw-MIDI sender ready (ready == 1, ISR on). */
    int (*link_up)(void *ctx);
    /* Will dt1_fw_send accept `n` bytes without entering a busy wait?
     * Mirrors 0x400049B6..0x40004A0C against the live counters. */
    int (*can_send)(void *ctx, uint32_t n);
    /* 0x40004990(byte_count, pointer): queue bytes for the USB IN endpoint.
     * Only called when can_send() just returned true. Returns bytes queued. */
    uint32_t (*send)(void *ctx, const uint8_t *msg, uint32_t n);
    /* Replace the receive callback slot 0x4061046C. `handler` is called as
     * handler(handler_ctx, bytes, count); the previous (handler, context) pair
     * is returned in prev_handler/prev_ctx so this adapter can chain ordinary
     * MIDI traffic to it. */
    void *(*set_rx_handler)(void *ctx, void *handler, void *handler_ctx,
                            void **prev_ctx);
};

/* One staging slot. Written whole by render, then published by head. */
struct dt1_tx_slot {
    uint32_t len;
    uint8_t msg[DT1_MSG_MAX];
};

struct dt1_transport {
    struct dt1_fw_ops fw;
    void *fw_ctx;
    struct dt1_tx_slot tx_ring[DT1_TX_RING];    /* render writes, task reads */
    volatile uint32_t tx_head;                  /* render-owned */
    volatile uint32_t tx_tail;                  /* task-owned */
    struct usbmidi_bytes rx;                    /* task-owned byte ring */
    uint8_t rx_buf[USBMIDI_QUEUE];
    struct sysex_split sysex;                   /* task-owned */
    uint8_t sysex_buf[DT1_RX_FRAME];
    uint8_t sysex_tags[(DT1_RX_FRAME + 7) / 8];
    void *prev_rx;                              /* chained stock handler */
    void *prev_rx_ctx;
    /* Our frames. `accept` decides which SysEx is ours; everything else, and
     * all non-SysEx traffic, is replayed to `chain` exactly as it arrived. */
    void (*on_sysex)(void *ctx, const uint8_t *frame, uint32_t n);
    int (*accept)(void *ctx, const uint8_t *frame, uint32_t n);
    void (*chain)(void *ctx, const uint8_t *data, uint32_t n);
    void *cb_ctx;
    /* Diagnostics, task-owned unless noted. */
    uint32_t staged, submitted, dropped, deferrals, submit_failures;
    uint32_t tx_overflow;                       /* render-owned count */
    uint32_t tx_discarded;                      /* dropped at link loss */
    uint32_t rx_bytes, rx_frames, rx_oversize, rx_chain_bytes, rx_padding;
    uint8_t rx_pad;                             /* zero pad bytes still allowed */
    volatile uint8_t link;                      /* last observed link state */
};

void dt1_transport_init(struct dt1_transport *t, const struct dt1_fw_ops *fw,
                        void *fw_ctx);
/* Install the receive callback, chaining the previous one. -> 1 on success. */
int dt1_transport_open(struct dt1_transport *t);
/* Remove our callback and restore the previous one. */
void dt1_transport_close(struct dt1_transport *t);

/* ---- render task: staging only ---- */
/* Stage one complete MIDI message (a START snapshot or a control frame). No
 * firmware call, no queue index update, no waiting. Whole message or nothing. */
enum dt1_tx_result dt1_tx_stage(struct dt1_transport *t,
                                const uint8_t *msg, uint32_t n);

/* ---- UI/tick task ---- */
/* Move staged messages into the firmware while it will not spin, then push
 * what the firmware queued out to USB. Bounded by `budget` messages. A link
 * loss discards everything staged: a new session needs fresh bytes. */
uint32_t dt1_tx_service(struct dt1_transport *t, uint32_t budget);
/* Drain host bytes that arrived for our queue. */
uint32_t dt1_rx_service(struct dt1_transport *t, uint32_t budget);

/* Link bookkeeping, called by the task when link_up() changes. On loss:
 * staged, queued and reassembly state are discarded so stale traffic can
 * never cross a reconnect. -> 1 when the link is up after the call. */
int dt1_link_update(struct dt1_transport *t, int up);

/* The receive callback body, exposed so the OS layer's trampoline is a
 * one-line call. Runs in the USB completion context: it only appends to the
 * byte ring, trimming the zero padding OS 1.54's sender leaves when a SysEx
 * does not end on a packet boundary (see the .c). Chained handling happens in
 * dt1_rx_service, from the task. */
void dt1_rx_bytes(struct dt1_transport *t, const uint8_t *data, uint32_t n);
#endif
