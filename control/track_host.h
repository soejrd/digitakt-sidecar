/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SC_TRACK_HOST_H
#define SC_TRACK_HOST_H
#include "track_session.h"
#include "track_playout.h"
#define SC_TRACK_HISTORY (SC_TRACK_WINDOW+1u)
#ifdef __cplusplus
extern "C" {
#endif
/* Core-3/task owned; receive/render/tick serialized. USB callbacks enqueue only.
 * Caller must close the output consumer's epoch gate on asynchronous loss. */
struct sc_track_host {
    sc_track_playout bank;
    uint8_t history[SC_TRACK_HISTORY][SC_TRACK_WIRE_SIZE];
    uint32_t history_sequence[SC_TRACK_HISTORY], history_block[SC_TRACK_HISTORY];
    uint32_t retired[8], retired_next, hello_block, last_rx, confirmed;
    unsigned opened, active_mask, reason, delay, lease;
    uint8_t lanes[2];
    sc_track_send_fn send;
    void *ctx;
};
void sc_track_host_init(struct sc_track_host *h,sc_track_send_fn send,void *ctx,
                        unsigned delay,unsigned lease);
int sc_track_host_receive(struct sc_track_host *h,const uint8_t *wire,unsigned size,uint32_t now);
void sc_track_host_stop(struct sc_track_host *h,unsigned reason);
void sc_track_host_tick(struct sc_track_host *h,uint32_t now);
int sc_track_host_allowed(const struct sc_track_host *h);
int sc_track_host_render(struct sc_track_host *h,int16_t (*out)[8],unsigned frames);
#ifdef __cplusplus
}
#endif
#endif
