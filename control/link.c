/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "link.h"
#include <string.h>
#include <limits.h>
static void put(uint8_t **p, uint32_t v, unsigned n)
{
    while (n--) { *(*p)++ = v & 127u; v >>= 7; }
}
static uint32_t get(const uint8_t **p, unsigned n)
{
    uint32_t v = 0; unsigned shift = 0;
    while (n--) { v |= (uint32_t)*(*p)++ << shift; shift += 7; }
    return v;
}
int sc_encode(uint8_t out[SC_WIRE_SIZE], const struct sc_frame *f)
{
    uint8_t *p = out; unsigned i;
    if (!f->session || !f->sequence || f->sequence == UINT32_MAX ||
        f->flags & ~SC_START) return 0;
    *p++ = 0xf0; *p++ = 0x7d; *p++ = 'D'; *p++ = 'T';
    *p++ = 'S'; *p++ = 'C'; *p++ = 1;
    put(&p, f->session, 5); put(&p, f->sequence, 5);
    put(&p, f->block, 5); put(&p, f->voice, 5);
    put(&p, (uint32_t)f->note, 5);
    for (i=0; i<8; ++i) put(&p, (uint16_t)f->params[i], 3);
    put(&p, (uint16_t)f->velocity, 3);
    *p++ = f->flags; *p++ = 0xf7;
    return p-out == SC_WIRE_SIZE;
}
static int32_t signed32(uint32_t v)
{
    return v <= INT32_MAX ? (int32_t)v : -1-(int32_t)(UINT32_MAX-v);
}
static int16_t signed16(uint32_t v)
{
    return v <= INT16_MAX ? (int16_t)v : (int16_t)(-1-(int32_t)(UINT16_MAX-v));
}
int sc_decode(struct sc_frame *f, const uint8_t *in, unsigned size)
{
    const uint8_t *p; struct sc_frame x = {0}; unsigned i;
    static const uint8_t prefix[] = {0xf0,0x7d,'D','T','S','C',1};
    if (size != SC_WIRE_SIZE || memcmp(in,prefix,sizeof prefix) ||
        in[size-1] != 0xf7) return 0;
    for (i=7; i<size-1; ++i) if (in[i] & 128) return 0;
    /* Reject overlong integers rather than silently truncating. */
    for (i=11; i<=31; i+=5) if (in[i] > 15) return 0;
    for (i=34; i<=58; i+=3) if (in[i] > 3) return 0;
    p = in+7;
    x.session=get(&p,5); x.sequence=get(&p,5);
    x.block=get(&p,5); x.voice=get(&p,5); x.note=signed32(get(&p,5));
    for (i=0; i<8; ++i) x.params[i]=signed16(get(&p,3));
    x.velocity=signed16(get(&p,3)); x.flags=*p;
    if (!x.session || !x.sequence || x.sequence==UINT32_MAX || x.flags & ~SC_START) return 0;
    *f=x; return 1;
}
void sc_reset(struct sc_receiver *r, uint32_t session)
{
    memset(r,0,sizeof *r); r->session=session; r->next=1;
}
int sc_accept(struct sc_receiver *r, const struct sc_frame *f)
{
    if (!r->session || f->session != r->session || f->sequence != r->next ||
        f->sequence==UINT32_MAX || f->flags & ~SC_START ||
        (r->active && signed32(f->block-r->last_block) <= 0)) {
        sc_reset(r,0); return 0;
    }
    r->state=*f; r->last_block=f->block; ++r->next; r->active=1;
    return 1;
}
int sc_expire(struct sc_receiver *r, uint32_t current_block, uint32_t lease)
{
    if (r->active && current_block-r->last_block >= lease) sc_reset(r,0);
    return r->active;
}
