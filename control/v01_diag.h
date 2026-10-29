/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef V01_DIAG_H
#define V01_DIAG_H
#include <stdint.h>
/* Diagnostic-only, separate from DTSC voice/session messages. No ACK/reply. */
#define V01_FIELDS 17u
#define V01_WIRE (7u + 5u * V01_FIELDS)
enum { V01_SEQ, V01_BLOCK, V01_TIMER, V01_TIMER_MODE, V01_UI, V01_WORK,
       V01_UGAP, V01_WGAP, V01_BATCH, V01_SENDS, V01_SEND_TICKS,
       V01_SEND_MAX, V01_UI_TICKS_MAX, V01_BATCH_TICKS_MAX, V01_GUARDS, V01_WORK_TICKS, V01_RENDER_TICKS };
static inline int v01_message(const uint8_t *p, unsigned n)
{ return n>=5 && p[0]==0xf0 && p[1]==0x7d && p[2]=='V' && p[3]=='0' && p[4]=='1'; }
static inline void v01_encode(uint8_t out[V01_WIRE], const uint32_t v[V01_FIELDS])
{
    unsigned i,j,k=6;
    out[0]=0xf0; out[1]=0x7d; out[2]='V'; out[3]='0'; out[4]='1'; out[5]=2;
    for(i=0;i<V01_FIELDS;++i) for(j=0;j<5;++j) out[k++]=(v[i]>>(7*j))&127u;
    out[k]=0xf7;
}
static inline int v01_decode(uint32_t v[V01_FIELDS], const uint8_t *p, unsigned n)
{
    uint32_t temp[V01_FIELDS]={0}; unsigned i,j,k=6,fields;
    if(n<7 || !v01_message(p,n) || p[n-1]!=0xf7) return 0;
    if(p[5]==1 && n==82u) fields=15;
    else if(p[5]==2 && n==V01_WIRE) fields=V01_FIELDS;
    else return 0;
    for(i=0;i<fields;++i) {
        temp[i]=0;
        for(j=0;j<5;++j) {
            unsigned b=p[k++];
            if(b>127u || (j==4 && b>15u)) return 0;
            temp[i]|=(uint32_t)b<<(7*j);
        }
    }
    for(i=0;i<V01_FIELDS;++i) v[i]=temp[i];
    return 1;
}
#endif
