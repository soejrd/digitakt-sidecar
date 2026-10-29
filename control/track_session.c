/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "track_session.h"
#include "sc_memory.h"
#include <limits.h>
unsigned sc_track_mask(const uint8_t lanes[2])
{
    unsigned mask=0;
    if (!lanes) return 0;
    for (unsigned i=0;i<2;++i) {
        if (lanes[i]==SC_TRACK_UNUSED) continue;
        if (lanes[i]>=SC_TRACKS || (mask&(1u<<lanes[i]))) return 0;
        mask|=1u<<lanes[i];
    }
    return mask;
}
int sc_track_control_encode(uint8_t out[SC_TRACK_CONTROL_SIZE], const struct sc_track_control *c)
{
    uint8_t legacy[SC_CONTROL_WIRE_SIZE];
    if (!out || !c || !sc_track_mask(c->lanes) || !sc_encode_control(legacy,&c->state)) return 0;
    for (unsigned i=0;i<23;++i) out[i]=legacy[i];
    out[6]=5; out[23]=c->lanes[0]; out[24]=c->lanes[1]; out[25]=4; out[26]=0xf7;
    return 1;
}
int sc_track_control_decode(struct sc_track_control *c,const uint8_t *in,unsigned size)
{
    uint8_t legacy[SC_CONTROL_WIRE_SIZE]; struct sc_track_control x;
    if (!c || !in || size!=SC_TRACK_CONTROL_SIZE || in[6]!=5 ||
        in[25]!=4 || in[26]!=0xf7) return 0;
    for (unsigned i=0;i<23;++i) legacy[i]=in[i];
    legacy[6]=SC_CONTROL_VERSION; legacy[23]=0xf7;
    x.lanes[0]=in[23]; x.lanes[1]=in[24];
    if (!sc_track_mask(x.lanes) || !sc_decode_control(&x.state,legacy,sizeof legacy)) return 0;
    *c=x; return 1;
}
void sc_track_dev_close(struct sc_track_device *d,unsigned reason)
{ d->failed=reason ? reason : SC_REASON_TRANSPORT; d->active_mask=0; d->count=0; }
int sc_track_dev_open(struct sc_track_device *d,uint32_t session,uint32_t now,
                      const uint8_t lanes[2],uint8_t out[SC_TRACK_CONTROL_SIZE])
{
    sc_zero(d,sizeof *d);
    d->session=session; d->next_sequence=1; d->last_rx=d->hello_block=now;
    d->mask=sc_track_mask(lanes);
    if (!session || !d->mask) { sc_track_dev_close(d,SC_REASON_MALFORMED); return 0; }
    d->lanes[0]=lanes[0]; d->lanes[1]=lanes[1];
    return sc_track_dev_hello(d,out);
}
int sc_track_dev_hello(struct sc_track_device *d,uint8_t out[SC_TRACK_CONTROL_SIZE])
{
    struct sc_track_control c;
    if (d->failed || d->negotiated) return 0;
    c.state.session=d->session; c.state.sequence=1; c.state.value=d->hello_block; c.state.kind=SC_HELLO;
    c.lanes[0]=d->lanes[0]; c.lanes[1]=d->lanes[1];
    return sc_track_control_encode(out,&c);
}
int sc_track_dev_send(struct sc_track_device *d,const struct sc_track_frame *source,
                      uint32_t now,uint8_t out[SC_TRACK_WIRE_SIZE])
{
    struct sc_track_frame f=*source;
    if (d->failed || !d->negotiated || d->count>=SC_TRACK_WINDOW) return 0;
    if (f.track>=SC_TRACKS || !(d->mask&(1u<<f.track)) || !f.state.voice ||
        d->next_sequence==UINT32_MAX) goto invalid;
    if (d->seen && f.state.block-d->last_block>=0x80000000u) goto invalid;
    if (d->seen&(1u<<f.track)) {
        uint32_t delta=f.state.block-d->track_block[f.track];
        if (!delta || delta>=0x80000000u) goto invalid;
    } else if (!(f.state.flags&SC_START)) goto invalid;
    f.state.session=d->session; f.state.sequence=d->next_sequence;
    if (!sc_track_encode(out,&f)) goto invalid;
    unsigned at=(d->head+d->count)%SC_TRACK_WINDOW;
    d->replay[at].frame=f; d->replay[at].sent_at=now;
    ++d->count; ++d->next_sequence; d->last_block=f.state.block;
    d->track_block[f.track]=f.state.block; d->seen|=1u<<f.track;
    return 1;
invalid:
    sc_track_dev_close(d,SC_REASON_SEQUENCE); return 0;
}
int sc_track_dev_receive(struct sc_track_device *d,const uint8_t *wire,unsigned size,uint32_t now)
{
    struct sc_track_control c;
    if (d->failed) return 0;
    if (!sc_track_control_decode(&c,wire,size)) {
        sc_track_dev_close(d,SC_REASON_MALFORMED); return 0;
    }
    if (c.state.session!=d->session) return 0; /* stale traffic cannot activate */
    if (c.lanes[0]!=d->lanes[0] || c.lanes[1]!=d->lanes[1]) goto invalid;
    if (c.state.kind==SC_DEACTIVATE) {
        sc_track_dev_close(d,SC_REASON_TRANSPORT); return 0;
    }
    if (c.state.kind==SC_HELLO_ACK && c.state.sequence==1 && c.state.value==d->hello_block) {
        d->negotiated=1; d->last_rx=now; return 1;
    }
    if (!d->negotiated) goto invalid;
    if (c.state.kind==SC_KEEPALIVE && d->ack_sequence) {
        if (c.state.sequence<d->ack_sequence) return 0; /* echo overtaken by newer ACKs */
        if (c.state.sequence!=d->ack_sequence || c.state.value!=d->ack_block) goto invalid;
        d->last_rx=now; return 1;
    }
    if (c.state.kind!=SC_ACTIVATE) goto invalid;
    if (c.state.sequence<d->ack_sequence) return 0; /* delayed cumulative ACK */
    if (c.state.sequence==d->ack_sequence) {
        if (c.state.value!=d->ack_block) goto invalid;
        return 1;
    }
    for (unsigned i=0;i<d->count;++i) {
        const struct sc_track_frame *f=&d->replay[(d->head+i)%SC_TRACK_WINDOW].frame;
        if (f->state.sequence!=c.state.sequence) continue;
        if (f->state.block!=c.state.value) goto invalid;
        for (unsigned j=0;j<=i;++j) {
            const struct sc_track_frame *acked=&d->replay[(d->head+j)%SC_TRACK_WINDOW].frame;
            if (acked->state.flags&SC_START) d->active_mask|=1u<<acked->track;
        }
        d->head=(d->head+i+1u)%SC_TRACK_WINDOW; d->count-=i+1u;
        d->ack_sequence=c.state.sequence; d->ack_block=c.state.value;
        d->last_rx=now; return 1;
    }
invalid:
    sc_track_dev_close(d,SC_REASON_SEQUENCE); return 0;
}
int sc_track_dev_replay(struct sc_track_device *d,uint8_t out[SC_TRACK_WIRE_SIZE])
{ return !d->failed && d->count && sc_track_encode(out,&d->replay[d->head].frame); }
int sc_track_dev_keepalive(struct sc_track_device *d,uint8_t out[SC_TRACK_CONTROL_SIZE])
{
    struct sc_track_control c;
    if (d->failed || !d->ack_sequence) return 0;
    c.state.session=d->session; c.state.sequence=d->ack_sequence;
    c.state.value=d->ack_block; c.state.kind=SC_KEEPALIVE;
    c.lanes[0]=d->lanes[0]; c.lanes[1]=d->lanes[1];
    return sc_track_control_encode(out,&c);
}
int sc_track_dev_tick(struct sc_track_device *d,uint32_t now,unsigned lease)
{
    if (!d->failed && lease && (now-d->last_rx>=lease ||
        (d->count && now-d->replay[d->head].sent_at>=lease)))
        sc_track_dev_close(d,SC_REASON_TIMEOUT);
    return d->failed!=0;
}
int sc_track_dev_allowed(const struct sc_track_device *d)
{ return !d->failed && d->negotiated && d->active_mask==d->mask; }
