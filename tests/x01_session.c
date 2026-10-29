/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "track_host.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct sc_track_host h;
static struct sc_track_device d;
static uint8_t answer[SC_TRACK_CONTROL_SIZE];
static unsigned answers,refuse;
static int send(void *ctx,const uint8_t *wire,unsigned n)
{
    (void)ctx; assert(n==sizeof answer); memcpy(answer,wire,n); ++answers; return !refuse;
}
static struct sc_track_frame frame(unsigned track,uint32_t block,unsigned gen,unsigned start)
{
    struct sc_track_frame f={0}; const int values[]={60,65,41,62,91,44,56,58};
    f.track=(uint8_t)track; f.state.block=block; f.state.voice=gen;
    f.state.flags=start ? SC_START : 0; f.state.velocity=127*256;
    for (unsigned k=0;k<8;++k) f.state.params[k]=values[k]*256;
    return f;
}
static void open_pair(unsigned session)
{
    uint8_t wire[SC_TRACK_CONTROL_SIZE]; const uint8_t lanes[]={0,3};
    assert(sc_track_dev_open(&d,session,0,lanes,wire));
    assert(sc_track_host_receive(&h,wire,sizeof wire,0));
    assert(!sc_track_host_allowed(&h) && !sc_track_dev_allowed(&d));
    assert(sc_track_dev_receive(&d,answer,sizeof answer,0));
    assert(d.negotiated);
}
static void codec(void)
{
    uint8_t wire[SC_TRACK_CONTROL_SIZE],bad[SC_TRACK_CONTROL_SIZE];
    struct sc_track_control c={{19,1,0xffffffffu,SC_HELLO},{0,7}},out;
    struct sc_control old;
    assert(sc_track_control_encode(wire,&c));
    assert(sc_track_control_decode(&out,wire,sizeof wire));
    assert(out.lanes[1]==7 && out.state.value==0xffffffffu);
    assert(!sc_decode_control(&old,wire,sizeof wire));
    for (unsigned n=0;n<sizeof wire;++n) assert(!sc_track_control_decode(&out,wire,n));
    const unsigned positions[]={6,12,17,22,23,24,25,26};
    for (unsigned i=0;i<8;++i) {
        memcpy(bad,wire,sizeof bad); bad[positions[i]]=128;
        assert(!sc_track_control_decode(&out,bad,sizeof bad));
    }
    c.lanes[1]=0; assert(!sc_track_control_encode(wire,&c));
    const uint8_t unused[]={127,127}; assert(!sc_track_mask(unused));
}
static void negotiation(void)
{
    uint8_t wire[SC_TRACK_WIRE_SIZE],hello[SC_TRACK_CONTROL_SIZE],first[SC_TRACK_WIRE_SIZE];
    sc_track_host_init(&h,send,0,128,750);
    struct sc_track_frame f=frame(0,10,1,1); f.state.session=3; f.state.sequence=1;
    assert(sc_track_encode(wire,&f));
    assert(!sc_track_host_receive(&h,wire,sizeof wire,0) && !h.opened);
    struct sc_control old={3,1,0,SC_HELLO};
    assert(sc_encode_control(hello,&old));
    assert(!sc_track_host_receive(&h,hello,SC_CONTROL_WIRE_SIZE,0) && !h.opened);
    open_pair(3);
    assert(sc_track_dev_send(&d,&f,0,wire)); memcpy(first,wire,sizeof first);
    assert(sc_track_host_receive(&h,wire,sizeof wire,0));
    assert(!sc_track_host_allowed(&h)); /* first ACK lost */
    f=frame(3,10,1,1); assert(sc_track_dev_send(&d,&f,0,wire));
    assert(sc_track_host_receive(&h,wire,sizeof wire,0));
    assert(sc_track_host_allowed(&h) && !sc_track_dev_allowed(&d));
    assert(sc_track_dev_receive(&d,answer,sizeof answer,0));
    assert(sc_track_dev_allowed(&d) && !d.count && d.ack_sequence==2);
    uint32_t queued=h.bank.write;
    assert(sc_track_host_receive(&h,first,sizeof first,1));
    assert(h.bank.write==queued); /* no duplicated engine START */
    assert(!sc_track_dev_receive(&d,answer,sizeof answer,1) && !d.failed);
    first[SC_WIRE_SIZE-1]=1; /* conflicting replay changes the track */
    assert(!sc_track_host_receive(&h,first,sizeof first,2) && !h.opened);
    open_pair(4);
    const uint8_t lanes[]={0,3}; struct sc_track_device stale;
    assert(sc_track_dev_open(&stale,3,0,lanes,hello));
    assert(!sc_track_host_receive(&h,hello,sizeof hello,3));
    assert(h.opened && h.bank.session==4 && !sc_track_host_allowed(&h));
    /* Negotiation must echo the same mapping, snapshot version and hello tuple. */
    struct sc_track_control response={{4,1,0,SC_HELLO_ACK},{0,5}};
    assert(sc_track_control_encode(hello,&response));
    assert(!sc_track_dev_receive(&d,hello,sizeof hello,3) && d.failed);
}
static void window_and_echo(void)
{
    uint8_t wire[SC_TRACK_WIRE_SIZE],keepalive[SC_TRACK_CONTROL_SIZE],old_echo[SC_TRACK_CONTROL_SIZE];
    sc_track_host_init(&h,send,0,2048,750); open_pair(9);
    for (unsigned t=0;t<2;++t) {
        struct sc_track_frame f=frame(t ? 3 : 0,0,1,1);
        assert(sc_track_dev_send(&d,&f,0,wire));
        assert(sc_track_host_receive(&h,wire,sizeof wire,0));
        assert(sc_track_dev_receive(&d,answer,sizeof answer,0));
    }
    assert(sc_track_dev_keepalive(&d,keepalive));
    assert(sc_track_host_receive(&h,keepalive,sizeof keepalive,1));
    memcpy(old_echo,answer,sizeof old_echo);
    for (unsigned i=0;i<SC_TRACK_WINDOW;++i) {
        struct sc_track_frame f=frame(0,i+1,1,0);
        assert(sc_track_dev_send(&d,&f,i+1,wire));
        assert(sc_track_host_receive(&h,wire,sizeof wire,i+1));
    }
    assert(d.count==SC_TRACK_WINDOW);
    struct sc_track_frame f=frame(0,33,1,0);
    assert(!sc_track_dev_send(&d,&f,33,wire) && !d.failed);
    /* Last acknowledged tuple remains valid alongside a full window. */
    assert(sc_track_host_receive(&h,keepalive,sizeof keepalive,33));
    /* Use last newly accepted ACK, not the keepalive echo, to retire window. */
    struct sc_track_control c={{9,34,32,SC_ACTIVATE},{0,3}};
    assert(sc_track_control_encode(keepalive,&c));
    assert(sc_track_dev_receive(&d,keepalive,sizeof keepalive,34) && !d.count);
    assert(!sc_track_dev_receive(&d,old_echo,sizeof old_echo,35) && !d.failed);
    assert(d.last_rx==34); /* overtaken echo cannot renew the lease */
    c.state.sequence=35; c.state.value=999;
    assert(sc_track_control_encode(keepalive,&c));
    assert(!sc_track_dev_receive(&d,keepalive,sizeof keepalive,36) && d.failed);
    /* Refused HELLO_ACK never leaves the host bank/PCM gate live. */
    refuse=1;
    const uint8_t lanes[]={0,3};
    assert(sc_track_dev_open(&d,10,0,lanes,keepalive));
    assert(!sc_track_host_receive(&h,keepalive,sizeof keepalive,37));
    assert(!h.opened && !sc_track_host_allowed(&h)); refuse=0;
}
static void lease_and_wrap(void)
{
    uint8_t wire[SC_TRACK_WIRE_SIZE],control[SC_TRACK_CONTROL_SIZE];
    const uint8_t lanes[]={3,SC_TRACK_UNUSED};
    sc_track_host_init(&h,send,0,128,750);
    assert(sc_track_dev_open(&d,51,0xfffffff0u,lanes,control));
    assert(sc_track_host_receive(&h,control,SC_TRACK_CONTROL_SIZE,0xfffffff0u));
    assert(sc_track_dev_receive(&d,answer,sizeof answer,0xfffffff0u));
    struct sc_track_frame f=frame(3,0xfffffffeu,1,1);
    assert(sc_track_dev_send(&d,&f,0xfffffffeu,wire));
    assert(sc_track_host_receive(&h,wire,sizeof wire,0xfffffffeu));
    assert(sc_track_dev_receive(&d,answer,sizeof answer,0xfffffffeu));
    assert(sc_track_dev_allowed(&d) && sc_track_host_allowed(&h));
    f=frame(3,0,2,1);
    assert(sc_track_dev_send(&d,&f,0,wire));
    assert(sc_track_host_receive(&h,wire,sizeof wire,0));
    /* Deliberately lose this ACK. Valid echoes for the preceding tuple must
     * not keep an unacknowledged snapshot alive beyond its own lease. */
    for (unsigned now=64;now<750;now+=64) {
        assert(sc_track_dev_keepalive(&d,control));
        assert(sc_track_host_receive(&h,control,SC_TRACK_CONTROL_SIZE,now));
        assert(sc_track_dev_receive(&d,answer,sizeof answer,now));
        assert(!sc_track_dev_tick(&d,now,750));
    }
    assert(sc_track_dev_tick(&d,750,750) && !sc_track_dev_allowed(&d));
    assert(d.failed==SC_REASON_TIMEOUT);
    /* Host inactivity expiry also uses unsigned elapsed time across wrap. */
    sc_track_host_tick(&h,1500);
    assert(!sc_track_host_allowed(&h));
    /* A retired session's exact HELLO remains rejected after that timeout. */
    assert(sc_track_dev_open(&d,51,0xfffffff0u,lanes,control));
    assert(!sc_track_host_receive(&h,control,SC_TRACK_CONTROL_SIZE,1501));
}
int main(void)
{
    codec(); negotiation(); window_and_echo(); lease_and_wrap();
    puts("X01 single-lane mapping, timestamp/lease wrap, stalled ACK despite live keepalives, timeout retirement: PASS");
    puts("X01 session: explicit v5/v4 negotiation, cumulative tuples, duplicate/conflicting replays, stale sessions/maps/echoes, full window, refused replies: PASS");
}
