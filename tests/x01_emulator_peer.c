/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "track_host.h"
#include <assert.h>
static struct sc_track_host peer;
static uint8_t replies[256][SC_TRACK_CONTROL_SIZE];
static unsigned read_at,write_at;
static int send(void *ctx,const uint8_t *wire,unsigned n)
{
    (void)ctx;
    if (write_at-read_at>=256 || n!=SC_TRACK_CONTROL_SIZE) return 0;
    for (unsigned i=0;i<n;++i) replies[write_at%256][i]=wire[i];
    ++write_at; return 1;
}
void peer_init(void) { read_at=write_at=0; sc_track_host_init(&peer,send,0,128,750); }
void peer_frame(const uint8_t *wire,unsigned n,uint32_t block)
{ (void)sc_track_host_receive(&peer,wire,n,block); }
unsigned peer_reply(uint8_t *out)
{
    if (read_at==write_at) return 0;
    for (unsigned i=0;i<SC_TRACK_CONTROL_SIZE;++i) out[i]=replies[read_at%256][i];
    ++read_at; return SC_TRACK_CONTROL_SIZE;
}
void peer_render(int32_t out[32][2])
{
    int16_t pcm[32][8]; (void)sc_track_host_render(&peer,pcm,32);
    for (unsigned i=0;i<32;++i) for (unsigned lane=0;lane<2;++lane)
        out[i][lane]=(int32_t)pcm[i][lane]*65536;
}
void peer_tick(uint32_t block) { sc_track_host_tick(&peer,block); }
void peer_lost(void) { sc_track_host_stop(&peer,SC_REASON_TRANSPORT); }
unsigned peer_allowed(void) { return (unsigned)sc_track_host_allowed(&peer); }
unsigned peer_session(void) { return peer.bank.session; }
unsigned peer_starts(unsigned track) { assert(track<8); return (unsigned)peer.bank.voice[track].consumed_starts; }
unsigned peer_param(unsigned track,unsigned control)
{ assert(track<8 && control<8); return (unsigned)(peer.bank.voice[track].engine.clock.raw[control]*256.0f); }
unsigned peer_installs(void) { return (unsigned)peer.bank.applied; }
unsigned peer_reason(void) { return peer.reason; }
unsigned peer_fault(void) { return peer.bank.failure.code; }
