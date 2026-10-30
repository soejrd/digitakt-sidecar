/* Replay timing-only hardware metadata through actual one-voice playout. */
#include "playout.h"
#include <stdio.h>

int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    FILE *input=fopen(argv[1],"r"); if(!input) return 2;
    sc_playout p; sc_playout_init(&p);
    unsigned sequence,block,clock,accepted=0;
    int16_t pcm[32][8];
    for(;;) {
        int read=fscanf(input,"%u %u %u",&sequence,&block,&clock);
        if(read==EOF) break;
        if(read!=3 || clock<p.clock) { fclose(input); return 2; }
        while(p.clock<clock) {
            unsigned n=clock-p.clock>32u ? 32u : (unsigned)(clock-p.clock);
            if(!sc_playout_render(&p,pcm,n)) { fclose(input); return 2; }
        }
        struct sc_frame f={0};
        f.session=1; f.sequence=sequence; f.block=block; f.voice=1;
        f.flags=sequence==1 ? SC_START : 0; f.note=60<<16; f.velocity=100<<8;
        const int defaults[]={60,65,41,62,91,44,56,58};
        for(unsigned k=0;k<8;++k) f.params[k]=(int16_t)(defaults[k]*256);
        if(!sc_playout_enqueue(&p,&f)) {
            printf("{\"rejected_sequence\":%u,\"accepted\":%u,\"code\":%u,\"clock\":%llu,\"due\":%llu,\"delta_blocks\":%u}\n",
                sequence,accepted,p.failure.code,(unsigned long long)p.failure.clock,
                (unsigned long long)p.failure.due,p.failure.delta);
            fclose(input); return 1;
        }
        ++accepted;
    }
    fclose(input); return 0;
}
