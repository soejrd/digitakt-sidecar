#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "v01_diag.h"
int main(void) {
 uint32_t a[V01_FIELDS],b[V01_FIELDS],keep[V01_FIELDS]; uint8_t wire[V01_WIRE];
 for(unsigned i=0;i<V01_FIELDS;++i)a[i]=i%2 ? UINT32_MAX-i : 0x12345678u+i;
 v01_encode(wire,a); assert(v01_message(wire,sizeof wire));
 for(unsigned i=1;i<sizeof wire-1;++i)assert(wire[i]<128);
 assert(v01_decode(b,wire,sizeof wire) && !memcmp(a,b,sizeof a));
 memcpy(keep,b,sizeof b);
 for(unsigned n=0;n<sizeof wire;++n)assert(!v01_decode(b,wire,n));
 wire[10]=16; assert(!v01_decode(b,wire,sizeof wire)); assert(!memcmp(keep,b,sizeof b));
 v01_encode(wire,a);wire[6]=128;assert(!v01_decode(b,wire,sizeof wire));
 v01_encode(wire,a);wire[5]=2;assert(!v01_decode(b,wire,sizeof wire));
 v01_encode(wire,a);wire[sizeof wire-1]=0;assert(!v01_decode(b,wire,sizeof wire));
 puts("V01 remote codec: bounds, 7-bit canonicality, full uint32 range, atomic rejection: PASS");
}
