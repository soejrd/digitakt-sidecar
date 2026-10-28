#include "trx_voice.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static sc_trx_voice a, b;
static int16_t whole[48000][8], ragged[48000][8];
static struct sc_frame frame(void) {
    struct sc_frame f = {0};
    const int values[8] = {60,65,41,62,91,44,56,58};
    f.voice=1; f.flags=SC_START;
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
    f.voice=2; assert(!sc_trx_install(&a,&f));
    puts("I04 real TRX-B2: ragged PCM equality, exact rate, note/velocity policy, retriggers, bounds, reset: PASS");
}
