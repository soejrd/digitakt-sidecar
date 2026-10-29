/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SC_TRACK_PLAYOUT_H
#define SC_TRACK_PLAYOUT_H
#include "playout.h"
#include "track_frame.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SC_TRACK_QUEUE 1024u
/* Single owner, common clock/anchor, globally sequenced snapshots. Equal block
 * timestamps are valid across different tracks, never twice on one track.
 * Caller owns session negotiation/leases and calls init on a fresh session.
 * Output columns are separate HOST engine lanes, not eight USB returns. */
typedef struct sc_track_playout {
    sc_trx_voice voice[SC_TRACKS];
    struct { struct sc_track_frame frame; uint64_t due; } events[SC_TRACK_QUEUE];
    uint64_t clock, last_due, applied, last_start[SC_TRACKS];
    uint32_t session, sequence, block, read, write, peak;
    uint32_t generation[SC_TRACKS], track_block[SC_TRACKS];
    unsigned mask, seen, anchored;
    struct sc_playout_failure failure;
} sc_track_playout;
int sc_track_playout_init(sc_track_playout *p, uint32_t session,
                          unsigned mask, unsigned delay_frames);
int sc_track_playout_enqueue(sc_track_playout *p, const struct sc_track_frame *f);
int sc_track_playout_render(sc_track_playout *p, int16_t (*out)[8], unsigned frames);
void sc_track_playout_stop(sc_track_playout *p);
#ifdef __cplusplus
}
#endif
#endif
