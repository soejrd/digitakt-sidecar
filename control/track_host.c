/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "track_host.h"
#include <string.h>
void sc_track_host_init(struct sc_track_host *h,sc_track_send_fn send,void *ctx,
                        unsigned delay,unsigned lease)
{
    memset(h,0,sizeof *h); h->send=send; h->ctx=ctx; h->delay=delay; h->lease=lease;
}
static int emit(struct sc_track_host *h,unsigned kind,uint32_t seq,uint32_t value)
{
    struct sc_track_control c; uint8_t wire[SC_TRACK_CONTROL_SIZE];
    c.state.session=h->bank.session; c.state.sequence=seq ? seq : 1;
    c.state.kind=(uint8_t)kind; c.state.value=value;
    c.lanes[0]=h->lanes[0]; c.lanes[1]=h->lanes[1];
    return sc_track_control_encode(wire,&c) && h->send && h->send(h->ctx,wire,sizeof wire);
}
void sc_track_host_stop(struct sc_track_host *h,unsigned reason)
{
    if (h->opened) {
        h->opened=0; h->retired[h->retired_next++%8u]=h->bank.session;
        sc_track_playout_stop(&h->bank);
        (void)emit(h,SC_DEACTIVATE,h->bank.sequence,reason);
    }
    h->active_mask=0; h->reason=reason;
}
static int fail(struct sc_track_host *h,unsigned reason)
{ sc_track_host_stop(h,reason); return 0; }
static int reply(struct sc_track_host *h,unsigned kind,uint32_t seq,uint32_t block)
{ return emit(h,kind,seq,block) ? 1 : fail(h,SC_REASON_TRANSPORT); }
int sc_track_host_receive(struct sc_track_host *h,const uint8_t *wire,unsigned size,uint32_t now)
{
    struct sc_track_control c;
    if (sc_track_control_decode(&c,wire,size)) {
        if (c.state.kind==SC_HELLO) {
            if (c.state.sequence!=1) return fail(h,SC_REASON_SEQUENCE);
            if (h->opened && c.state.session==h->bank.session) {
                if (c.state.value!=h->hello_block || c.lanes[0]!=h->lanes[0] ||
                    c.lanes[1]!=h->lanes[1]) return fail(h,SC_REASON_SESSION);
                return reply(h,SC_HELLO_ACK,1,h->hello_block);
            }
            for (unsigned i=0;i<8;++i) if (h->retired[i]==c.state.session) return 0;
            if (h->opened) sc_track_host_stop(h,SC_REASON_RECONNECT);
            h->lanes[0]=c.lanes[0]; h->lanes[1]=c.lanes[1];
            h->hello_block=c.state.value; h->last_rx=now; h->active_mask=h->confirmed=0;
            memset(h->history_sequence,0,sizeof h->history_sequence);
            if (!sc_track_playout_init(&h->bank,c.state.session,sc_track_mask(c.lanes),h->delay))
                return fail(h,SC_REASON_INSTALL);
            h->opened=1; h->reason=0;
            return reply(h,SC_HELLO_ACK,1,h->hello_block);
        }
        if (!h->opened || c.state.session!=h->bank.session) return 0;
        if (c.lanes[0]!=h->lanes[0] || c.lanes[1]!=h->lanes[1]) return fail(h,SC_REASON_SESSION);
        if (c.state.kind==SC_DEACTIVATE) return fail(h,SC_REASON_TRANSPORT);
        const unsigned at=(c.state.sequence-1u)%SC_TRACK_HISTORY;
        if (c.state.kind!=SC_KEEPALIVE || h->history_sequence[at]!=c.state.sequence ||
            h->history_block[at]!=c.state.value || c.state.sequence<h->confirmed)
            return fail(h,SC_REASON_SEQUENCE);
        h->confirmed=c.state.sequence; h->last_rx=now;
        return reply(h,SC_KEEPALIVE,c.state.sequence,c.state.value);
    }
    struct sc_track_frame f;
    if (!sc_track_decode(&f,wire,size)) return fail(h,SC_REASON_MALFORMED);
    if (!h->opened || f.state.session!=h->bank.session) return 0;
    const unsigned at=(f.state.sequence-1u)%SC_TRACK_HISTORY;
    if (f.state.sequence<=h->bank.sequence) {
        if (h->history_sequence[at]!=f.state.sequence || memcmp(h->history[at],wire,size))
            return fail(h,SC_REASON_SEQUENCE);
        h->last_rx=now;
        return reply(h,SC_ACTIVATE,f.state.sequence,f.state.block);
    }
    if (!sc_track_playout_enqueue(&h->bank,&f)) return fail(h,SC_REASON_INSTALL);
    memcpy(h->history[at],wire,size); h->history_sequence[at]=f.state.sequence;
    h->history_block[at]=f.state.block; h->last_rx=now;
    if (!reply(h,SC_ACTIVATE,f.state.sequence,f.state.block)) return 0;
    if (f.state.flags&SC_START) h->active_mask|=1u<<f.track;
    return 1;
}
void sc_track_host_tick(struct sc_track_host *h,uint32_t now)
{ if (h->opened && h->lease && now-h->last_rx>=h->lease) sc_track_host_stop(h,SC_REASON_TIMEOUT); }
int sc_track_host_allowed(const struct sc_track_host *h)
{ return h->opened && !h->bank.failure.code && h->active_mask==h->bank.mask; }
int sc_track_host_render(struct sc_track_host *h,int16_t (*out)[8],unsigned frames)
{
    int16_t lanes[1][8];
    const int allowed=sc_track_host_allowed(h);
    for (unsigned i=0;i<frames;++i) {
        memset(out[i],0,sizeof out[i]);
        if (!h->opened) continue;
        if (!sc_track_playout_render(&h->bank,lanes,1)) {
            memset(out,0,frames*sizeof *out); return fail(h,SC_REASON_INSTALL);
        }
        if (allowed) for (unsigned lane=0;lane<2;++lane)
            if (h->lanes[lane]<SC_TRACKS) out[i][lane]=lanes[0][h->lanes[lane]];
    }
    return h->opened!=0;
}
