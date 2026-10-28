/* Replay sanitized physical timing through the real scheduler. No raw private
 * note/control payload is stored here. This proves only the retained window. */
#include "playout.h"
#include <assert.h>
#include <stdio.h>
static sc_playout original,effective;
static int16_t scratch[32][8];
static void advance(sc_playout *p,unsigned n) {
    while(n) { unsigned q=n>32 ? 32:n; assert(sc_playout_render(p,scratch,q)); n-=q; }
}
int main(int argc,char **argv) {
    assert(argc==2); FILE *in=fopen(argv[1],"r"); assert(in);
    int slack; assert(fscanf(in,"%d",&slack)==1 && slack>0 && (unsigned)slack<SC_PLAYOUT_DELAY);
    sc_playout_init(&original); sc_playout_init(&effective);
    struct sc_frame f={0}; f.session=1;f.sequence=1;f.block=100;f.voice=1;f.flags=SC_START;
    assert(sc_playout_enqueue(&original,&f) && sc_playout_enqueue(&effective,&f));
    advance(&original,SC_PLAYOUT_DELAY-(unsigned)slack);
    advance(&effective,SC_PLAYOUT_DELAY-(unsigned)slack);
    unsigned i,source,render,keep,old_failed=0,kept=1,block=100;
    while(fscanf(in,"%u %u %u %u",&i,&source,&render,&keep)==4) {
        if(!i) continue;
        assert(source%32==0); block+=source/32;
        advance(&original,render); advance(&effective,render);
        f.flags=0; f.block=block; f.sequence=i+1;
        int ok=sc_playout_enqueue(&original,&f);
        if(!ok) { assert(original.failure.code==SC_PF_LATE); ++old_failed; }
        if(keep) {
            f.sequence=++kept;
            assert(sc_playout_enqueue(&effective,&f));
        }
        if(old_failed) break;
    }
    fclose(in);
    assert(old_failed==1 && !effective.fault && kept==9);
    assert(original.failure.clock-original.failure.due==128);
    puts("Captured timing replay: original all-update path fails 128 frames late; 9 effective updates accepted without rebasing deadlines: PASS");
    return 0;
}
