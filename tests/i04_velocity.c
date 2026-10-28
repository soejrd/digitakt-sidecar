/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "trx_voice.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static sc_trx_voice high,low;
static int16_t a[512][8],b[512][8];
static struct sc_frame event(int velocity)
{
    struct sc_frame f={0};
    const int defaults[8]={60,65,41,62,91,44,56,58};
    f.voice=1; f.flags=SC_START; f.velocity=(int16_t)velocity;
    for(unsigned k=0;k<8;++k) f.params[k]=defaults[k]*256;
    return f;
}
static void compare(void)
{
    unsigned nonzero=0;
    sc_trx_render(&high,a,512); sc_trx_render(&low,b,512);
    for(unsigned n=0;n<512;++n) {
        nonzero+=a[n][0]!=0;
        assert(abs((int)b[n][0]*127-(int)a[n][0]*20)<=127);
        for(unsigned k=1;k<8;++k) assert(a[n][k]==0 && b[n][k]==0);
    }
    assert(nonzero>100);
}
int main(void)
{
    struct sc_frame f=event(127*256),q=event(20*256);
    sc_trx_init(&high); sc_trx_init(&low);
    assert(sc_trx_install(&high,&f) && sc_trx_install(&low,&q));
    q.velocity=64*256;
    assert(!sc_trx_install(&low,&q)); /* rejected START must not alter gain */
    compare();
    q.flags=0; q.velocity=127*256;
    assert(sc_trx_install(&low,&q));
    compare(); /* a non-START metadata update must not change tail gain */
    sc_trx_clear(&low);q=event(0); assert(sc_trx_install(&low,&q));
    sc_trx_render(&low,b,512);
    for(unsigned n=0;n<512;++n) for(unsigned k=0;k<8;++k) assert(b[n][k]==0);
    sc_trx_clear(&low);q=event(-256); assert(sc_trx_install(&low,&q));
    sc_trx_render(&low,b,512);
    for(unsigned n=0;n<512;++n) assert(b[n][0]==0);
    sc_trx_clear(&high); sc_trx_clear(&low);
    f=event(127*256);q=event(32767);
    assert(sc_trx_install(&high,&f) && sc_trx_install(&low,&q));
    sc_trx_render(&high,a,512);sc_trx_render(&low,b,512);
    for(unsigned n=0;n<512;++n) for(unsigned k=0;k<8;++k) assert(a[n][k]==b[n][k]);
    puts("I04 per-hit velocity: 20/127 linear PCM gain, zero/negative mute, tail gain latched: PASS");
}
