/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SC_PLAYOUT_H
#define SC_PLAYOUT_H
#include "trx_voice.h"
#define SC_PLAYOUT_QUEUE 256u
#ifndef SC_PLAYOUT_DELAY
#define SC_PLAYOUT_DELAY 2048u /* 42.667 ms: covers modelled 50-block worker batches; hardware qualification pending */
#endif
#define SC_PLAYOUT_HORIZON 48000u
/* All calls on core 3. Device timestamps are 32-frame 48 kHz blocks.
 * Enqueue owns complete snapshots before ACK; render applies only when due.
 * Deadline, overflow and order faults latch silence until a new session. */
enum sc_playout_fault_code {
    SC_PF_INVALID=1, SC_PF_ORDER, SC_PF_LATE, SC_PF_HORIZON,
    SC_PF_CAPACITY, SC_PF_START_SPACING, SC_PF_APPLY, SC_PF_DEADLINE
};
struct sc_playout_failure {
    uint32_t code, block, flags, delta;
    uint64_t clock, due;
};
typedef struct sc_playout {
    sc_trx_voice voice;
    struct { struct sc_frame frame; uint64_t due; } events[SC_PLAYOUT_QUEUE];
    uint64_t clock, last_due, last_start_due, applied;
    uint32_t session, last_block, last_sequence, read, write, peak;
    struct sc_playout_failure failure;
    unsigned fault, anchored, playing, had_start;
} sc_playout;
#ifdef __cplusplus
extern "C" {
#endif
void sc_playout_init(sc_playout *p);
int sc_playout_enqueue(sc_playout *p, const struct sc_frame *f);
int sc_playout_render(sc_playout *p, int16_t (*out)[8], unsigned frames);
#ifdef __cplusplus
}
#endif
#endif
