/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "link.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    struct sc_frame a={.session=42,.sequence=1,.block=100,.voice=1,
        .note=INT32_MIN,.velocity=INT16_MIN,.flags=SC_START};
    struct sc_frame b={0}; struct sc_receiver r;
    uint8_t wire[SC_WIRE_SIZE], bad[SC_WIRE_SIZE]; unsigned i;
    for(i=0;i<8;++i) a.params[i]=(i&1)?INT16_MIN:INT16_MAX;
    assert(sc_encode(wire,&a)); assert(sc_decode(&b,wire,sizeof wire));
    assert(b.note==a.note && b.velocity==a.velocity);
    for(i=0;i<8;++i) assert(b.params[i]==a.params[i]);
    for(i=0;i<sizeof wire;++i) {
        memcpy(bad,wire,sizeof wire); bad[i]=0xff;
        assert(!sc_decode(&b,bad,sizeof bad));
    }
    assert(!sc_decode(&b,wire,sizeof wire-1));
    memcpy(bad,wire,sizeof wire); bad[11]=16; assert(!sc_decode(&b,bad,sizeof bad));
    memcpy(bad,wire,sizeof wire); bad[34]=4; assert(!sc_decode(&b,bad,sizeof bad));
    sc_reset(&r,42); assert(!r.active); assert(sc_accept(&r,&a));
    assert(r.state.flags==SC_START && r.state.params[0]==INT16_MAX);
    assert(sc_expire(&r,109,10)); assert(!sc_expire(&r,110,10));
    assert(!sc_accept(&r,&a)); /* timeout requires a new handshake */
    sc_reset(&r,43); assert(!sc_accept(&r,&a)); /* old session */
    a.session=43; sc_reset(&r,43); assert(sc_accept(&r,&a));
    assert(!sc_accept(&r,&a)); /* duplicate latches silence */
    sc_reset(&r,43); a.sequence=2; assert(!sc_accept(&r,&a)); /* loss/congestion */
    a.sequence=1; sc_reset(&r,43); assert(sc_accept(&r,&a));
    a.sequence=2; assert(!sc_accept(&r,&a)); /* repeated timestamp */
    a.sequence=1; a.block=UINT32_MAX; sc_reset(&r,43); assert(sc_accept(&r,&a));
    a.sequence=2; a.block=0; assert(sc_accept(&r,&a)); /* block clock wrap */
    a.sequence=UINT32_MAX; assert(!sc_encode(wire,&a));

    {
        struct sc_control c={.session=0x12345678,.sequence=7,
                             .value=0x0abcdef,.kind=SC_ACTIVATE};
        struct sc_control d={0};
        uint8_t cw[SC_CONTROL_WIRE_SIZE], cb[SC_CONTROL_WIRE_SIZE];
        assert(SC_CONTROL_WIRE_SIZE==24);
        assert(sc_encode_control(cw,&c));
        assert(cw[0]==0xf0 && cw[1]==0x7d && cw[6]==2 && cw[7]==SC_ACTIVATE);
        assert(cw[23]==0xf7);
        assert(sc_decode_control(&d,cw,sizeof cw));
        assert(d.session==c.session && d.sequence==c.sequence &&
               d.value==c.value && d.kind==c.kind);
        for(i=0;i<sizeof cw;++i) {
            memcpy(cb,cw,sizeof cw); cb[i]=0xff;
            assert(!sc_decode_control(&d,cb,sizeof cb));
        }
        assert(!sc_decode_control(&d,cw,sizeof cw-1));
        memcpy(cb,cw,sizeof cw); cb[12]=16;
        assert(!sc_decode_control(&d,cb,sizeof cb));
        c.session=0; assert(!sc_encode_control(cw,&c));
        c.session=1; c.sequence=0; assert(!sc_encode_control(cw,&c));
        c.sequence=UINT32_MAX; assert(!sc_encode_control(cw,&c));
        c.sequence=1; c.kind=0; assert(!sc_encode_control(cw,&c));
        c.kind=6; assert(!sc_encode_control(cw,&c));
        assert(sc_is_dtsc(wire,6));
        assert(!sc_is_dtsc(wire,5));
        assert(sc_is_dtsc(wire,sizeof wire));
    }
    puts("control codec / ordering / congestion / reconnect / lease: PASS");
}
