/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "track_playout.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static sc_track_playout p;
static double samples[40100];
static double now(void)
{
    struct timespec t; assert(!clock_gettime(CLOCK_MONOTONIC,&t));
    return t.tv_sec+t.tv_nsec*1e-9;
}
static int compare(const void *a,const void *b)
{ double x=*(const double *)a,y=*(const double *)b; return (x>y)-(x<y); }
int main(int argc,char **argv)
{
    struct timespec resolution;
    assert(!clock_getres(CLOCK_MONOTONIC,&resolution));
    FILE *file=argc>1 ? fopen(argv[1],"w") : stdout; assert(file);
    fprintf(file,"{\n  \"scope\": \"optimized desktop host; no Pi, USB, device worker or hardware performance claim\",\n"
        "  \"native_rate_hz\": 44100, \"output_rate_hz\": 48000,\n"
        "  \"engine\": \"real mdkit TRX-B2 with independent 160/147 converters\",\n"
        "  \"block_frames\": 12, \"nominal_block_budget_us\": 250,\n"
        "  \"playout_delay_frames\": 128, \"queue_capacity\": %u,\n"
        "  \"bank_bytes\": %zu, \"voice_bytes\": %zu,\n"
        "  \"host_pointer_bits\": %zu, \"clock_resolution_ns\": %llu,\n  \"runs\": [\n",
        SC_TRACK_QUEUE,sizeof p,sizeof(sc_trx_voice),sizeof(void *)*8,
        (unsigned long long)resolution.tv_sec*1000000000u+(unsigned long long)resolution.tv_nsec);
    unsigned row=0;
    const unsigned counts[]={1,2,8};
    for (unsigned profile=0;profile<2;++profile) for (unsigned count=0;count<3;++count) {
        const unsigned voices=counts[count], blocks=15000, end=blocks*32u+128u;
        unsigned next=0, seq=0, generation=0, iterations=0;
        uint64_t energy[8]={0},sum=0;
        double enqueue_max=0;
        assert(sc_track_playout_init(&p,17,(1u<<voices)-1u,128));
        const double begin=now();
        while (p.clock<end) {
            const double enqueue_begin=now();
            while (next<blocks && (uint64_t)next*32u<=p.clock) {
                if (profile || next%8u==0) {
                    unsigned start=next%(profile ? 2u : 192u)==0;
                    if (start) ++generation;
                    for (unsigned t=0;t<voices;++t) {
                        struct sc_track_frame e={0};
                        const unsigned defaults[]={60,65,41,62,91,44,56,58};
                        uint8_t wire[SC_TRACK_WIRE_SIZE]; struct sc_track_frame decoded;
                        e.track=(uint8_t)t; e.state.session=17; e.state.sequence=++seq;
                        e.state.block=next; e.state.voice=generation;
                        e.state.flags=start ? SC_START : 0; e.state.velocity=(100+(int)t)*256;
                        for (unsigned i=0;i<8;++i)
                            e.state.params[i]=(int16_t)(((defaults[i]+next+t)%128u)*256u);
                        assert(sc_track_encode(wire,&e));
                        assert(sc_track_decode(&decoded,wire,sizeof wire));
                        assert(sc_track_playout_enqueue(&p,&decoded));
                    }
                }
                ++next;
            }
            double elapsed=now()-enqueue_begin;
            if (elapsed>enqueue_max) enqueue_max=elapsed;
            int16_t out[12][8];
            const unsigned n=end-p.clock<12 ? (unsigned)(end-p.clock) : 12;
            const double render_begin=now();
            assert(sc_track_playout_render(&p,out,n));
            assert(iterations<sizeof samples/sizeof samples[0]);
            samples[iterations++]=(now()-render_begin)*1e6;
            for (unsigned i=0;i<n;++i) for (unsigned t=0;t<8;++t) {
                if (t>=voices) assert(!out[i][t]);
                energy[t]+=(unsigned)abs((int)out[i][t]);
            }
        }
        double wall=now()-begin;
        assert(p.applied==seq && p.read==p.write && !p.failure.code);
        for (unsigned t=0;t<voices;++t) {
            assert(p.voice[t].starts==generation && p.voice[t].consumed_starts==generation);
            assert(energy[t]>0 && p.voice[t].native_frames==441000);
            sum+=energy[t];
        }
        qsort(samples,iterations,sizeof samples[0],compare);
        fprintf(file,"%s    {\"voices\": %u, \"profile\": \"%s\", \"scheduled_audio_seconds\": 10, "
            "\"wall_seconds\": %.6f, \"realtime_factor\": %.2f, "
            "\"render_p50_us\": %.3f, \"render_p99_us\": %.3f, \"render_max_us\": %.3f, "
            "\"enqueue_batch_max_us\": %.3f, \"snapshots_applied\": %llu, "
            "\"starts_per_voice\": %u, \"native_samples_per_voice\": 441000, "
            "\"queue_peak\": %u, \"pcm_abs_sum\": %llu, \"faults\": %u}",
            row++ ? ",\n" : "",voices,profile ? "dense: every block, START every two blocks" :
            "sparse: every eight blocks, START every192 blocks",wall,10/wall,
            samples[iterations/2],samples[(iterations*99u)/100u],samples[iterations-1],
            enqueue_max*1e6,(unsigned long long)p.applied,generation,p.peak,
            (unsigned long long)sum,p.failure.code);
    }
    fprintf(file,"\n  ]\n}\n");
    if (file!=stdout) assert(!fclose(file));
    fputs("X01 1/2/8-engine benchmark: both profiles retained every snapshot/START and exact native sample counts: PASS\n",stderr);
}
