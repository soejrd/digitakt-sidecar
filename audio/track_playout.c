/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "track_playout.h"
#include <limits.h>
#include <string.h>
static int fail(sc_track_playout *p, unsigned code, const struct sc_frame *f, uint64_t due)
{
    if (!p->failure.code) {
        p->failure.code=code; p->failure.clock=p->clock; p->failure.due=due;
        if (f) { p->failure.block=f->block; p->failure.flags=f->flags;
                 p->failure.delta=f->block-p->block; }
    }
    return 0;
}
int sc_track_playout_init(sc_track_playout *p, uint32_t session,
                          unsigned mask, unsigned delay_frames)
{
    memset(p,0,sizeof *p);
    if (!session || !mask || mask>255u || delay_frames>SC_PLAYOUT_HORIZON)
        return fail(p,SC_PF_INVALID,0,0);
    p->session=session; p->mask=mask; p->last_due=delay_frames;
    for (unsigned t=0;t<SC_TRACKS;++t) sc_trx_init(&p->voice[t]);
    return 1;
}
void sc_track_playout_stop(sc_track_playout *p)
{ fail(p,SC_PF_INVALID,0,0); }
int sc_track_playout_enqueue(sc_track_playout *p, const struct sc_track_frame *event)
{
    const struct sc_frame *f=&event->state;
    const unsigned t=event->track;
    uint64_t due=p->last_due;
    if (p->failure.code) return 0;
    if (t>=SC_TRACKS || !(p->mask&(1u<<t)) || !f->voice ||
        (f->flags&~SC_START)) return fail(p,SC_PF_INVALID,f,0);
    if (f->session!=p->session || f->sequence!=p->sequence+1u ||
        f->sequence==UINT32_MAX) return fail(p,SC_PF_ORDER,f,0);
    if (!p->anchored) {
        due+=p->clock;
    } else {
        const uint32_t delta=f->block-p->block;
        if (delta>=0x80000000u) return fail(p,SC_PF_ORDER,f,0);
        due+=(uint64_t)delta*32u;
    }
    if (p->seen&(1u<<t)) {
        const uint32_t delta=f->block-p->track_block[t];
        if (!delta || delta>=0x80000000u) return fail(p,SC_PF_ORDER,f,due);
        if (f->flags&SC_START) {
            if (p->generation[t]==UINT32_MAX || f->voice!=p->generation[t]+1u)
                return fail(p,SC_PF_ORDER,f,due);
            if (due-p->last_start[t]<35u) return fail(p,SC_PF_START_SPACING,f,due);
        } else if (f->voice!=p->generation[t]) return fail(p,SC_PF_ORDER,f,due);
    } else if (!(f->flags&SC_START)) return fail(p,SC_PF_INVALID,f,due);
    if (due<p->clock) return fail(p,SC_PF_LATE,f,due);
    if (due-p->clock>SC_PLAYOUT_HORIZON) return fail(p,SC_PF_HORIZON,f,due);
    if (p->write-p->read>=SC_TRACK_QUEUE) return fail(p,SC_PF_CAPACITY,f,due);
    p->events[p->write%SC_TRACK_QUEUE].frame=*event;
    p->events[p->write%SC_TRACK_QUEUE].due=due;
    ++p->write;
    if (p->write-p->read>p->peak) p->peak=p->write-p->read;
    p->seen|=1u<<t; p->track_block[t]=f->block; p->generation[t]=f->voice;
    if (f->flags&SC_START) p->last_start[t]=due;
    p->last_due=due; p->block=f->block; p->sequence=f->sequence; p->anchored=1;
    return 1;
}
int sc_track_playout_render(sc_track_playout *p, int16_t (*out)[8], unsigned frames)
{
    for (unsigned i=0;i<frames;++i) {
        memset(out[i],0,sizeof out[i]);
        if (!p->failure.code) {
            /* At most eight distinct tracks can occupy a timestamp. */
            while (p->read!=p->write && p->events[p->read%SC_TRACK_QUEUE].due<=p->clock) {
                const unsigned at=p->read%SC_TRACK_QUEUE;
                const struct sc_track_frame *f=&p->events[at].frame;
                if (p->events[at].due<p->clock) {
                    fail(p,SC_PF_DEADLINE,&f->state,p->events[at].due); break;
                }
                if (!sc_trx_install(&p->voice[f->track],&f->state)) {
                    fail(p,SC_PF_APPLY,&f->state,p->events[at].due); break;
                }
                ++p->read; ++p->applied;
            }
            if (!p->failure.code) for (unsigned t=0;t<SC_TRACKS;++t)
                if (p->voice[t].starts) out[i][t]=sc_trx_render_mono(&p->voice[t]);
        }
        ++p->clock;
    }
    if (p->failure.code) memset(out,0,frames*sizeof *out);
    return !p->failure.code;
}
