#include "playout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static sc_playout a,b;
static int16_t burst[4096][8], timed[4096][8], scratch[16384][8];
static struct sc_frame event(unsigned n) {
    struct sc_frame f={0};
    const int defaults[]={60,65,41,62,91,44,56,58};
    f.session=9; f.sequence=n+1; f.block=0xfffffffdu+n; f.voice=1+n/2;
    f.flags=(n%2)==0 ? SC_START : 0;
    for(unsigned i=0;i<8;++i) f.params[i]=(defaults[i]+(n%8))*256;
    return f;
}
int main(void) {
    sc_playout_init(&a); sc_playout_init(&b);
    for(unsigned n=0;n<8;++n) { struct sc_frame f=event(n); assert(sc_playout_enqueue(&a,&f)); }
    sc_playout_render(&a,burst,4096);
    // Same first-arrival anchor, later events arrive in a jittered burst before due.
    struct sc_frame f=event(0); assert(sc_playout_enqueue(&b,&f));
    assert(sc_playout_render(&b,timed,900));
    for(unsigned n=1;n<8;++n) { f=event(n); assert(sc_playout_enqueue(&b,&f)); }
    assert(sc_playout_render(&b,timed+900,3196));
    assert(!memcmp(burst,timed,sizeof burst));
    assert(a.applied==8 && b.applied==8);
    assert(a.voice.starts==4 && a.voice.consumed_starts==4);
    for(unsigned i=0;i<SC_PLAYOUT_DELAY;++i) for(unsigned ch=0;ch<8;++ch) assert(burst[i][ch]==0);
    // Exact sample deadlines, including device-block wrap and parameter-only events.
    sc_playout_init(&a);
    for(unsigned n=0;n<8;++n) { f=event(n); assert(sc_playout_enqueue(&a,&f)); }
    for(unsigned i=0;i<SC_PLAYOUT_DELAY+400;++i) {
        unsigned expected=i<SC_PLAYOUT_DELAY ? 0 : (i-SC_PLAYOUT_DELAY)/32+1;
        if(expected>8) expected=8;
        assert(sc_playout_render(&a,scratch,1)); assert(a.applied==expected);
    }
    // 64 queued dense snapshots retain every supported START (one per 2 blocks).
    sc_playout_init(&a);
    for(unsigned n=0;n<SC_PLAYOUT_QUEUE;++n) { f=event(n); assert(sc_playout_enqueue(&a,&f)); }
    assert(a.peak==SC_PLAYOUT_QUEUE);
    assert(sc_playout_render(&a,scratch,16384));
    assert(a.applied==SC_PLAYOUT_QUEUE && a.voice.starts==SC_PLAYOUT_QUEUE/2 && a.voice.consumed_starts==SC_PLAYOUT_QUEUE/2);
    // Capacity fault, late deadline, order, session, horizon, too-close START fail closed.
    for(unsigned scenario=0;scenario<7;++scenario) {
        sc_playout_init(&a); f=event(0); assert(sc_playout_enqueue(&a,&f));
        f=event(1);
        if(scenario==0) {
            for(unsigned n=1;n<SC_PLAYOUT_QUEUE;++n) { f=event(n); assert(sc_playout_enqueue(&a,&f)); }
            f=event(SC_PLAYOUT_QUEUE);
        }
        if(scenario==1) assert(sc_playout_render(&a,scratch,SC_PLAYOUT_DELAY+76));
        if(scenario==2) ++f.sequence;
        if(scenario==3) ++f.session;
        if(scenario==4) f.block+=2000;
        if(scenario==5) f.flags=SC_START;
        if(scenario==6) f.block+=0x80000000u;
        assert(!sc_playout_enqueue(&a,&f));
        const unsigned codes[]={SC_PF_CAPACITY,SC_PF_LATE,SC_PF_ORDER,SC_PF_ORDER,
                                SC_PF_HORIZON,SC_PF_START_SPACING,SC_PF_ORDER};
        assert(a.failure.code==codes[scenario] && a.failure.block==f.block);
        assert(a.failure.clock==a.clock);
        memset(scratch,0x55,sizeof scratch);
        assert(!sc_playout_render(&a,scratch,16384));
        for(unsigned i=0;i<16384;++i) for(unsigned ch=0;ch<8;++ch) assert(scratch[i][ch]==0);
        sc_playout_init(&a); f=event(0); f.session=10; assert(sc_playout_enqueue(&a,&f));
    }
    puts("I04 playout: exact deadlines, burst/jitter PCM equality, wrap, bounded dense snapshots/consumed STARTs, fail-closed bounds and reset: PASS");
}
