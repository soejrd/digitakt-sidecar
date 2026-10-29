#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "device_link.h"
static void same(const struct sc_frame *a,const struct sc_frame *b) {
 assert(a->session==b->session && a->sequence==b->sequence && a->block==b->block);
 assert(a->voice==b->voice && a->note==b->note && a->velocity==b->velocity && a->flags==b->flags);
 for(unsigned i=0;i<8;++i)assert(a->params[i]==b->params[i]);
}
int main(void) {
 struct sc_frame f={0},a,b;uint8_t legacy[61],compact[61],replay[61];
 f.session=0xf1234567u;f.voice=UINT32_MAX;f.note=-1234567;f.velocity=-257;
 for(unsigned n=1;n<1000;++n) {
  f.sequence=n;f.block=UINT32_MAX-n;f.flags=n%2;
  for(unsigned k=0;k<8;++k)f.params[k]=((n+k)%128)<<8;
  assert(sc_encode(legacy,&f));assert(sc_encode_compact(compact,&f)==45);
  assert(sc_decode(&a,legacy,61) && sc_decode(&b,compact,45));same(&f,&a);same(&a,&b);
 }
 for(unsigned n=0;n<45;++n)assert(!sc_decode(&b,compact,n));
 compact[42]=4;assert(!sc_decode(&b,compact,45));
 assert(sc_encode_compact(compact,&f)==45);compact[32]=128;assert(!sc_decode(&b,compact,45));
 f.params[0]=1;assert(sc_encode_compact(compact,&f)==61);assert(sc_encode(legacy,&f));assert(!memcmp(legacy,compact,61));
 f.params[0]=-256;assert(sc_encode_compact(compact,&f)==61);assert(sc_decode(&a,compact,61));same(&f,&a);
 struct sc_device_link d;struct sc_control c={0};
 assert(sc_dev_open(&d,42,100,legacy));c.session=42;c.sequence=1;c.value=100;c.kind=SC_HELLO_ACK;
 assert(!sc_dev_receive_control(&d,&c,100).invalidated);
 f.flags=SC_START;f.params[0]=0;
 assert(sc_dev_send_compact_start(&d,&f,101,compact)==45);
 assert(sc_dev_resend_frame(&d,replay)==45 && !memcmp(compact,replay,45));
 assert(!sc_dev_resend_start(&d,replay));
 assert(sc_decode(&a,compact,45));c.kind=SC_ACTIVATE;c.sequence=a.sequence;c.value=a.block;
 assert(sc_dev_receive_control(&d,&c,102).activated);
 f.flags=0;f.params[0]=60<<8;
 assert(sc_dev_send_compact_start(&d,&f,103,compact)==45 && sc_decode(&b,compact,45));
 assert(b.sequence==a.sequence+1 && b.block==103 && b.voice==f.voice && !b.flags);
 assert(sc_dev_resend_frame(&d,replay)==45 && !memcmp(compact,replay,45));
 puts("V01 compact: exact full snapshots, signed note/velocity, malformed rejection, fallback, byte-identical replay and ordered generations: PASS");
}
