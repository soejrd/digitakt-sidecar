/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "playout.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>
static sc_playout p;
static double now(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec+t.tv_nsec*1e-9;
}
int main(int argc,char **argv) {
    const unsigned frames=480000;
    unsigned next=0;
    double max=0,begin;
    int16_t out[12][8];
    sc_playout_init(&p); begin=now();
    while(p.clock<frames) {
        while((uint64_t)next*32<=p.clock) {
            struct sc_frame f={0};
            const int values[]={60,65,41,62,91,44,56,58};
            f.session=1; f.sequence=next+1; f.block=next; f.voice=1;
            f.flags=next%2==0 ? SC_START : 0; f.velocity=127*256;
            for(unsigned i=0;i<8;++i) f.params[i]=((values[i]+next)%128)*256;
            assert(sc_playout_enqueue(&p,&f)); ++next;
        }
        const double start=now();
        assert(sc_playout_render(&p,out,12));
        const double elapsed=now()-start;
        if(elapsed>max) max=elapsed;
    }
    const double wall=now()-begin;
    assert(p.peak<=SC_PLAYOUT_QUEUE && !p.fault);
    assert(p.voice.starts==p.voice.consumed_starts);
    FILE *f=argc>1 ? fopen(argv[1],"w") : stdout; assert(f);
    fprintf(f,"{\n  \"scope\": \"optimized desktop host, no USB/Pi, synthetic dense snapshots\",\n  \"audio_seconds\": 10,\n  \"output_block_frames\": 12,\n  \"playout_delay_frames\": %u,\n  \"wall_seconds\": %.9f,\n  \"max_observed_render_us\": %.3f,\n  \"queue_peak\": %u,\n  \"applied_snapshots\": %llu,\n  \"starts\": %llu,\n  \"consumed_starts\": %llu,\n  \"faults\": %u\n}\n", SC_PLAYOUT_DELAY,wall,max*1e6,p.peak,(unsigned long long)p.applied,(unsigned long long)p.voice.starts,(unsigned long long)p.voice.consumed_starts,p.fault);
    if(f!=stdout) fclose(f);
    return 0;
}
