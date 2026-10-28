#include "trx_voice.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static sc_trx_voice a, b;
static int16_t whole[48000][8], ragged[48000][8];
static struct sc_frame frame(void) {
    struct sc_frame f = {0};
    const int values[8] = {60,65,41,62,91,44,56,58};
    f.voice=1; f.flags=SC_START; f.velocity=127*256;
    for (unsigned i=0;i<8;++i) f.params[i]=values[i]*256;
    return f;
}
int main(void) {
    struct sc_frame f=frame();
    sc_trx_init(&a); sc_trx_init(&b);
    assert(sc_trx_install(&a,&f));
    f.note=123*65536; f.velocity=127*256;
    assert(sc_trx_install(&b,&f));
    sc_trx_render(&a,whole,48000);
    unsigned pos=0,k=0, nonzero=0;
    const unsigned sizes[]={1,12,32,7,257};
    while(pos<48000) {
        unsigned n=sizes[k++%5]; if(n>48000-pos) n=48000-pos;
        sc_trx_render(&b,ragged+pos,n); pos+=n;
    }
    assert(!memcmp(whole,ragged,sizeof whole));
    assert(a.native_frames==b.native_frames && a.native_frames==44100);
    for(unsigned i=0;i<48000;++i) {
        assert(whole[i][0]>=-4095 && whole[i][0]<=4095);
        nonzero+=whole[i][0]!=0;
        for(unsigned ch=1;ch<8;++ch) assert(whole[i][ch]==0);
    }
    assert(nonzero>1000);
    f.flags=0; assert(sc_trx_install(&a,&f)); assert(a.starts==1);
    f.flags=SC_START; assert(sc_trx_install(&a,&f)); assert(a.starts==2);
    assert(!sc_trx_install(&a,&f)); assert(a.starts==2);
    sc_trx_render(&a,whole,48000);
    nonzero=0; for(unsigned i=0;i<48000;++i) nonzero+=whole[i][0]!=0;
    assert(nonzero>1000 && a.consumed_starts==2);
    sc_trx_clear(&a); sc_trx_init(&b);
    assert(sc_trx_install(&a,&f) && sc_trx_install(&b,&f));
    sc_trx_render(&a,whole,48000); sc_trx_render(&b,ragged,48000);
    assert(!memcmp(whole,ragged,sizeof whole));
    sc_trx_clear(&a); sc_trx_clear(&b);
    int16_t raw_pcm[32][8], effective_pcm[32][8], previous[8]={0};
    unsigned generation=0, effective_installs=0;
    f=frame();
    for(unsigned n=0;n<128;++n) {
        f.flags=n%64==0 ? SC_START : 0;
        if(f.flags) ++generation;
        f.voice=generation; f.params[0]=(int16_t)(36*256-(int)n*16);
        struct sc_frame q=f;
        for(unsigned k=0;k<8;++k) {
            int value=q.params[k]/256;
            if(value<0) value=0; if(value>127) value=127;
            q.params[k]=(int16_t)(value*256);
        }
        assert(sc_trx_install(&a,&f));
        if(n==0 || f.flags || memcmp(q.params,previous,sizeof previous)) {
            assert(sc_trx_install(&b,&q)); ++effective_installs;
            memcpy(previous,q.params,sizeof previous);
        }
        sc_trx_render(&a,raw_pcm,32); sc_trx_render(&b,effective_pcm,32);
        assert(!memcmp(raw_pcm,effective_pcm,sizeof raw_pcm));
    }
    assert(effective_installs<16 && a.consumed_starts==2 && b.consumed_starts==2);
    puts("Raw fractional updates vs effective integer-only controls: identical real-engine PCM, both STARTs consumed: PASS");
    f.flags=0;
    f.voice=2; assert(sc_trx_install(&a,&f));
    f.voice=0; assert(!sc_trx_install(&a,&f));
    puts("I04 real TRX-B2: ragged PCM equality, exact rate, note independence, retriggers, bounds, reset: PASS");
}
