/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "trx_voice.h"
#include <assert.h>
#include <stdio.h>

static sc_trx_voice voice;
static int16_t pcm[96][8];
static struct sc_frame snapshot(unsigned start, int dirt, int dist)
{
    struct sc_frame f={0};
    const int values[8]={60,65,41,62,91,44,dirt,dist};
    f.voice=start ? 2 : 1; f.flags=start ? SC_START : 0;
    f.velocity=100*256;
    for(unsigned k=0;k<8;++k) f.params[k]=values[k]*256;
    return f;
}
int main(void)
{
    struct sc_frame f=snapshot(1,0,0);
    sc_trx_init(&voice);
    assert(sc_trx_install(&voice,&f));
    sc_trx_render(&voice,pcm,96);
    f=snapshot(1,127,127);
    const unsigned phase=voice.engine.pos, clock=voice.engine.clock.block;
    const uint32_t osc_phase=voice.engine.phase, amplitude=voice.engine.amplitude;
    assert(sc_trx_install(&voice,&f));
    assert(voice.engine.pos==phase && voice.engine.clock.block==clock);
    assert(voice.engine.phase==osc_phase && voice.engine.amplitude==amplitude);
    sc_trx_render(&voice,pcm,96);
    printf("locked DIRT/DIST: requested 127/127, engine %.3f/%.3f at consumed START\n",
           voice.engine.clock.current[6],voice.engine.clock.current[7]);
    fflush(stdout);
    assert(voice.consumed_starts==2);
    assert(voice.engine.clock.current[6]==127.0f);
    assert(voice.engine.clock.current[7]==127.0f);
    /* Lock restoration must take effect on a continuing tail too. */
    f=snapshot(0,0,0);
    assert(sc_trx_install(&voice,&f));
    sc_trx_render(&voice,pcm,96);
    assert(voice.engine.clock.current[6]==0.0f);
    assert(voice.engine.clock.current[7]==0.0f);
    for(unsigned n=0;n<64;++n) {
        sc_trx_render(&voice,pcm,96);
        assert(voice.engine.clock.current[6]==0.0f);
        assert(voice.engine.clock.current[7]==0.0f);
    }
    assert(voice.starts==2 && voice.consumed_starts==2);
    puts("I04 final SRC snapshots: locked DIRT/DIST at START and unlocked tail restore: PASS");
}
