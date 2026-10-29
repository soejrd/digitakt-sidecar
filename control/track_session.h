/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SC_TRACK_SESSION_H
#define SC_TRACK_SESSION_H
#include "track_frame.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SC_TRACK_CONTROL_SIZE 27u
#define SC_TRACK_WINDOW 32u
#define SC_TRACK_UNUSED 127u
/* v5 management explicitly negotiates v4 snapshots + two 48k mono lanes.
 * Layout: DTSC prefix/version5, kind, session/sequence/value (5x7 each),
 * lane0/lane1 track IDs (127=unused), snapshot version4, F7. Stable map/session.
 * ACTIVATE is cumulative but must match an actually outstanding tuple. */
struct sc_track_control { struct sc_control state; uint8_t lanes[2]; };
unsigned sc_track_mask(const uint8_t lanes[2]);
int sc_track_control_encode(uint8_t out[SC_TRACK_CONTROL_SIZE], const struct sc_track_control *c);
int sc_track_control_decode(struct sc_track_control *c, const uint8_t *in, unsigned size);
typedef int (*sc_track_send_fn)(void *, const uint8_t *, unsigned);
struct sc_track_device {
    struct { struct sc_track_frame frame; uint32_t sent_at; } replay[SC_TRACK_WINDOW];
    uint32_t session, next_sequence, ack_sequence, ack_block, last_rx, hello_block;
    uint32_t last_block, track_block[8];
    unsigned head, count, mask, seen, active_mask, negotiated, failed;
    uint8_t lanes[2];
};
int sc_track_dev_open(struct sc_track_device *d, uint32_t session, uint32_t now,
                      const uint8_t lanes[2], uint8_t out[SC_TRACK_CONTROL_SIZE]);
int sc_track_dev_hello(struct sc_track_device *d,uint8_t out[SC_TRACK_CONTROL_SIZE]);
int sc_track_dev_send(struct sc_track_device *d, const struct sc_track_frame *source,
                      uint32_t now, uint8_t out[SC_TRACK_WIRE_SIZE]);
int sc_track_dev_receive(struct sc_track_device *d, const uint8_t *wire,
                         unsigned size, uint32_t now);
int sc_track_dev_replay(struct sc_track_device *d, uint8_t out[SC_TRACK_WIRE_SIZE]);
int sc_track_dev_keepalive(struct sc_track_device *d, uint8_t out[SC_TRACK_CONTROL_SIZE]);
int sc_track_dev_tick(struct sc_track_device *d, uint32_t now, unsigned lease);
int sc_track_dev_allowed(const struct sc_track_device *d);
void sc_track_dev_close(struct sc_track_device *d, unsigned reason);
#ifdef __cplusplus
}
#endif
#endif
