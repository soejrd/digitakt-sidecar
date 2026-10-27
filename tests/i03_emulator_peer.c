/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Portable Circle peer, called by the emulator USB shim. */
#include "circle_link_adapter.h"
#include <string.h>
static struct sc_circle_link peer;
static struct sc_frame installed;
static uint8_t replies[256][SC_CONTROL_WIRE_SIZE];
static uint32_t wr,rd,installs,starts;
static int send_reply(void *ctx,const uint8_t *p,uint32_t n)
{
    (void)ctx;
    if(n!=SC_CONTROL_WIRE_SIZE || wr-rd==256) return 0;
    memcpy(replies[wr++%256],p,n); return 1;
}
static int install(void *ctx,const struct sc_frame *f)
{
    (void)ctx; installed=*f; ++installs; starts+=(f->flags&SC_START)!=0; return 1;
}
static void clear(void *ctx) { (void)ctx; memset(&installed,0,sizeof installed); }
void peer_init(void)
{
    wr=rd=installs=starts=0;
    sc_circle_init(&peer,install,send_reply,clear,0,750);
}
void peer_frame(const uint8_t *p,unsigned n,uint32_t block)
{
    unsigned i;
    sc_circle_advance_block(&peer,block);
    for(i=0;i<n;++i) sc_circle_midi_byte(&peer,p[i]);
    sc_circle_process(&peer,128);
}
unsigned peer_reply(uint8_t *p)
{
    if(rd==wr) return 0;
    memcpy(p,replies[rd++%256],SC_CONTROL_WIRE_SIZE); return SC_CONTROL_WIRE_SIZE;
}
unsigned peer_installs(void) { return installs; }
unsigned peer_starts(void) { return starts; }
unsigned peer_allowed(void) { return sc_circle_audio_allowed(&peer); }
int peer_param(unsigned k) { return k<8 ? installed.params[k] : 0; }
void peer_lost(void) { sc_circle_transport_lost(&peer,SC_REASON_TRANSPORT); sc_circle_process(&peer,128); }

void peer_advance(uint32_t block) { sc_circle_advance_block(&peer,block); sc_circle_process(&peer,128); }
unsigned peer_session(void) { return peer.host.session; }
