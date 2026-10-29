/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "track_playout.h"
#include "return_lanes.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static sc_track_playout a,b;
static int16_t whole[4096][8], ragged[4096][8];
static struct sc_track_frame make(unsigned t,unsigned seq,uint32_t block,
                                   unsigned generation,unsigned start)
{
    struct sc_track_frame e={0};
    const int defaults[]={60,65,41,62,91,44,56,58};
    e.track=(uint8_t)t;
    e.state.session=17; e.state.sequence=seq; e.state.block=block;
    e.state.voice=generation; e.state.flags=start ? SC_START : 0;
    e.state.velocity=127*256;
    for (unsigned i=0;i<8;++i) e.state.params[i]=(defaults[i]+(int)t)*256;
    return e;
}
static void codec(void)
{
    struct sc_track_frame e=make(7,1,UINT32_MAX,1,1), decoded;
    uint8_t wire[SC_TRACK_WIRE_SIZE], bad[SC_TRACK_WIRE_SIZE];
    struct sc_frame legacy;
    e.state.note=INT32_MIN; e.state.params[0]=-123; e.state.params[1]=32767;
    e.state.velocity=-256;
    assert(sc_track_encode(wire,&e));
    assert(sc_track_decode(&decoded,wire,sizeof wire));
    assert(decoded.track==7 && decoded.state.note==INT32_MIN);
    assert(decoded.state.params[0]==-123 && decoded.state.params[1]==32767);
    assert(decoded.state.velocity==-256 && decoded.state.block==UINT32_MAX);
    assert(!sc_decode(&legacy,wire,SC_TRACK_WIRE_SIZE));
    assert(!sc_decode(&legacy,wire,SC_WIRE_SIZE));
    for (unsigned i=0;i<SC_TRACK_WIRE_SIZE;++i)
        assert(!sc_track_decode(&decoded,wire,i));
    const unsigned positions[]={6,11,34,60,61};
    for (unsigned i=0;i<5;++i) {
        memcpy(bad,wire,sizeof bad); bad[positions[i]]=128;
        assert(!sc_track_decode(&decoded,bad,sizeof bad));
    }
    e.track=8; assert(!sc_track_encode(wire,&e));
    e.track=0; e.state.voice=0; assert(!sc_track_encode(wire,&e));
    assert(!sc_track_decode(0,wire,sizeof wire));
    assert(!sc_track_decode(&decoded,0,sizeof wire));
}
static void independent(void)
{
    struct sc_track_frame e[]={make(0,1,0xfffffffeu,1,1),make(3,2,0xfffffffeu,1,1),
        make(0,3,0xffffffffu,1,0),make(3,4,0,2,1),make(0,5,0,2,1),make(3,6,1,2,0)};
    e[2].state.params[0]=24*256; e[3].state.velocity=0;
    assert(sc_track_playout_init(&a,17,9,128));
    assert(sc_track_playout_init(&b,17,9,128));
    for (unsigned i=0;i<6;++i) {
        uint8_t wire[SC_TRACK_WIRE_SIZE]; struct sc_track_frame decoded;
        assert(sc_track_encode(wire,&e[i]));
        assert(sc_track_decode(&decoded,wire,sizeof wire));
        assert(sc_track_playout_enqueue(&a,&decoded));
    }
    assert(sc_track_playout_enqueue(&b,&e[0]));
    assert(sc_track_playout_render(&b,ragged,64));
    /* Second track's first START arrives later, but shares the first anchor. */
    for (unsigned i=1;i<6;++i) assert(sc_track_playout_enqueue(&b,&e[i]));
    assert(sc_track_playout_render(&a,whole,4096));
    const unsigned sizes[]={1,7,32,12,257};
    for (unsigned pos=64,k=0;pos<4096;++k) {
        unsigned n=sizes[k%5]; if (n>4096-pos) n=4096-pos;
        assert(sc_track_playout_render(&b,ragged+pos,n)); pos+=n;
    }
    assert(!memcmp(whole,ragged,sizeof whole));
    sc_trx_voice ref[8];
    for (unsigned t=0;t<8;++t) sc_trx_init(&ref[t]);
    unsigned next=0,audible=0;
    const unsigned due[]={128,128,160,192,192,224};
    for (unsigned sample=0;sample<4096;++sample) {
        while (next<6 && due[next]==sample) {
            assert(sc_trx_install(&ref[e[next].track],&e[next].state)); ++next;
        }
        for (unsigned t=0;t<8;++t) {
            const int16_t want=ref[t].starts ? sc_trx_render_mono(&ref[t]) : 0;
            assert(whole[sample][t]==want); audible+=want!=0;
            if (t==3 && sample>=192) assert(want==0);
        }
    }
    assert(audible>100 && a.applied==6 && b.applied==6);
    assert(a.voice[0].consumed_starts==2 && a.voice[3].consumed_starts==2);
    sc_track_playout_stop(&a);
    memset(whole,0x55,sizeof whole);
    assert(!sc_track_playout_render(&a,whole,4096));
    for (unsigned i=0;i<4096;++i) for (unsigned t=0;t<8;++t) assert(!whole[i][t]);
    assert(sc_track_playout_init(&a,18,9,128));
    assert(!a.applied && !a.voice[0].starts && a.read==a.write);
    assert(!sc_track_playout_enqueue(&a,&e[0])); /* old session */
}
static void faults(void)
{
    for (unsigned scenario=0;scenario<11;++scenario) {
        assert(sc_track_playout_init(&a,17,3,128));
        struct sc_track_frame e=make(0,1,0,1,1);
        assert(sc_track_playout_enqueue(&a,&e));
        e=make(0,2,2,1,0);
        if (scenario==0) e.track=8;
        if (scenario==1) e.track=2;
        if (scenario==2) e.state.voice=2;
        if (scenario==3) e.state.block=0;
        if (scenario==4) e.state.sequence=3;
        if (scenario==5) e.state.session=18;
        if (scenario==6) {
            assert(sc_track_playout_render(&a,whole,256));
            e.track=1; e.state.flags=SC_START; /* late first START on another track */
        }
        if (scenario==7) e.state.block=2000;
        if (scenario==8) { e.state.block=1; e.state.voice=2; e.state.flags=SC_START; }
        if (scenario==9) e.state.block=0x80000000u;
        if (scenario==10) e.track=1; /* first update without START */
        assert(!sc_track_playout_enqueue(&a,&e));
        const unsigned codes[]={SC_PF_INVALID,SC_PF_INVALID,SC_PF_ORDER,SC_PF_ORDER,
            SC_PF_ORDER,SC_PF_ORDER,SC_PF_LATE,SC_PF_HORIZON,SC_PF_START_SPACING,
            SC_PF_ORDER,SC_PF_INVALID};
        assert(a.failure.code==codes[scenario]);
        assert(!sc_track_playout_render(&a,whole,4096));
        for (unsigned i=0;i<4096;++i) for (unsigned t=0;t<8;++t) assert(!whole[i][t]);
    }
    assert(sc_track_playout_init(&a,17,1,128));
    for (unsigned i=0;i<SC_TRACK_QUEUE;++i) {
        struct sc_track_frame e=make(0,i+1,i,1,i==0);
        assert(sc_track_playout_enqueue(&a,&e));
    }
    struct sc_track_frame e=make(0,SC_TRACK_QUEUE+1,SC_TRACK_QUEUE,1,0);
    assert(!sc_track_playout_enqueue(&a,&e) && a.failure.code==SC_PF_CAPACITY);
    assert(!sc_track_playout_init(&a,0,1,128));
    assert(!sc_track_playout_init(&a,17,256,128));
}
static void routing(void)
{
    int32_t tracks[256], saved[256]; volatile int32_t halves[128];
    const int8_t map[]={0,3};
    for (unsigned selector=0;selector<2;++selector) {
        for (unsigned i=0;i<256;++i) tracks[i]=saved[i]=(int32_t)i+1234;
        for (unsigned i=0;i<128;++i) halves[i]=(int32_t)i*12345-4567;
        assert(sc_return_lanes(tracks,halves,selector,1,map));
        for (unsigned t=0;t<8;++t) for (unsigned i=0;i<32;++i)
            assert(tracks[t*32+i]==((t==0 || t==3) ?
                (int32_t)(selector*64+i*2+(t==3))*12345-4567 : saved[t*32+i]));
        for (unsigned i=0;i<128;++i)
            assert(halves[i]==(i/64==selector ? 0 : (int32_t)i*12345-4567));
    }
    assert(!sc_return_lanes(tracks,halves,8,1,map));
    for (unsigned i=0;i<128;++i) assert(!halves[i]);
    for (unsigned i=0;i<32;++i) assert(!tracks[i] && !tracks[96+i]);
    const int8_t invalid[]={0,0};
    memcpy(saved,tracks,sizeof saved);
    assert(!sc_return_lanes(tracks,halves,0,1,invalid));
    assert(!memcmp(saved,tracks,sizeof saved));
}
int main(void)
{
    codec(); independent(); faults(); routing();
    puts("X01: v4 exact codec, independent PCM, shared deadlines/wrap/ragged arrivals, velocity isolation, faults/reset, two-lane routing: PASS");
}
