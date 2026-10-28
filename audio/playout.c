/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "playout.h"
#include <string.h>
void sc_playout_init(sc_playout *p)
{
    memset(p, 0, sizeof *p);
    sc_trx_init(&p->voice);
}
static int fault(sc_playout *p) { p->fault=1; return 0; }
int sc_playout_enqueue(sc_playout *p, const struct sc_frame *f)
{
    uint64_t due;
    if (p->fault || f->voice!=1 || !f->session || !f->sequence) return fault(p);
    if (!p->anchored) {
        if (!(f->flags&SC_START)) return fault(p);
        due=p->clock+SC_PLAYOUT_DELAY;
    } else {
        const uint32_t delta=f->block-p->last_block;
        if (f->session!=p->session || f->sequence!=p->last_sequence+1u ||
            !delta || delta>=0x80000000u) return fault(p);
        due=p->last_due+(uint64_t)delta*32u;
    }
    if (due<p->clock || due-p->clock>SC_PLAYOUT_HORIZON ||
        p->write-p->read>=SC_PLAYOUT_QUEUE) return fault(p);
    /* mdkit consumes triggers on native 32-sample boundaries. Less than 35
     * output samples cannot guarantee consumption before the next START.
     * Device block granularity makes the supported minimum 64 output frames.
     * Reject explicitly; do not silently merge adjacent-block STARTs. */
    if ((f->flags&SC_START) && p->had_start && due-p->last_start_due<35u)
        return fault(p);
    p->events[p->write%SC_PLAYOUT_QUEUE].frame=*f;
    p->events[p->write%SC_PLAYOUT_QUEUE].due=due;
    ++p->write;
    if (p->write-p->read>p->peak) p->peak=p->write-p->read;
    p->session=f->session; p->last_block=f->block; p->last_sequence=f->sequence;
    p->last_due=due; p->anchored=1;
    if (f->flags&SC_START) { p->last_start_due=due; p->had_start=1; }
    return 1;
}
int sc_playout_render(sc_playout *p, int16_t (*out)[8], unsigned frames)
{
    for (unsigned i=0; i<frames; ++i) {
        memset(out[i],0,sizeof out[i]);
        if (p->fault) continue;
        if (p->read!=p->write && p->events[p->read%SC_PLAYOUT_QUEUE].due<=p->clock) {
            const unsigned at=p->read%SC_PLAYOUT_QUEUE;
            if (p->events[at].due<p->clock ||
                !sc_trx_install(&p->voice,&p->events[at].frame)) { fault(p); continue; }
            ++p->read; ++p->applied; p->playing=1;
        }
        if (p->playing) sc_trx_render(&p->voice,out+i,1);
        ++p->clock;
    }
    if (p->fault) memset(out,0,frames*sizeof *out);
    return !p->fault;
}
