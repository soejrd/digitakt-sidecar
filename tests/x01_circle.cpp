// SPDX-License-Identifier: GPL-2.0-or-later
#include "dt_link.h"
#include "shared.h"
#include "track_session.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
Output g_Output;
Spsc<uint8_t,8192> g_MidiIn;
Spsc<uint8_t,kMidiOutBytes> g_MidiOut;
Spsc<TCmd,16> g_Cmds;
static sc_track_device device;
static uint32_t clock_at;
static unsigned nonzero[2];
static void replies() {
  while (g_MidiOut.size()) {
    uint8_t lo,hi,wire[SC_TRACK_CONTROL_SIZE];
    assert(g_MidiOut.pop(&lo) && g_MidiOut.pop(&hi));
    unsigned n=lo+256u*hi; assert(n==sizeof wire);
    for (auto &v:wire) assert(g_MidiOut.pop(&v));
    (void)sc_track_dev_receive(&device,wire,n,clock_at);
  }
}
static void snapshot(unsigned track,unsigned block,unsigned generation,bool start) {
  sc_track_frame f={}; uint8_t wire[SC_TRACK_WIRE_SIZE];
  const int defaults[]={60,65,41,62,91,44,56,58};
  f.track=(uint8_t)track; f.state.block=block; f.state.voice=generation;
  f.state.flags=start ? SC_START : 0; f.state.velocity=100*256;
  for (unsigned k=0;k<8;++k) f.state.params[k]=(int16_t)((defaults[k]+track)*256);
  assert(sc_track_dev_send(&device,&f,clock_at,wire));
  assert(dt_link::SysEx(wire,sizeof wire)); replies();
}
static void handshake(unsigned id,const uint8_t lanes[2]) {
  uint8_t wire[SC_TRACK_CONTROL_SIZE];
  assert(sc_track_dev_open(&device,id,clock_at,lanes,wire));
  assert(dt_link::SysEx(wire,sizeof wire)); replies(); assert(device.negotiated);
  snapshot(lanes[0],clock_at,1,true);
  assert(!sc_track_dev_allowed(&device));
  snapshot(lanes[1],clock_at,1,true);
  assert(sc_track_dev_allowed(&device));
}
static void render(unsigned blocks) {
  for (unsigned b=0;b<blocks;++b) {
    int16_t pcm[32][8]; int32_t usb[32][12];
    dt_link::RenderVoice(pcm,32); dt_link::AdvanceFrames(32); ++clock_at;
    dt_link::Poll();
    assert(g_Output.Push(pcm,32)==32); g_Output.Pull(usb,32);
    for (unsigned i=0;i<32;++i) {
      for (unsigned lane=0;lane<2;++lane) nonzero[lane]+=usb[i][lane]!=0;
      for (unsigned c=2;c<12;++c) assert(!usb[i][c]);
    }
  }
}
int main() {
  assert(!dt_link::VoiceCommand(true));
  dt_link::Select(); dt_link::Poll(0);
  uint8_t lanes[]={0,3}; handshake(41,lanes);
  assert(!dt_link::State(nullptr));
  render(10); assert(!nonzero[0] && !nonzero[1]);
  assert(dt_link::VoiceCommand(true)); dt_link::Poll();
  snapshot(0,clock_at,2,true); snapshot(3,clock_at,2,true);
  render(60); assert(nonzero[0] && nonzero[1]);
  assert(g_Output.GetMode()==Output::kStereoReturn);
  // The actual Output consumer must preserve polarity and ratio, not sum L/R.
  int16_t fixed[32][8]={}; int32_t usb[32][12];
  for (auto &f:fixed) { f[0]=1000; f[1]=-3000; f[5]=12000; }
  assert(g_Output.Push(fixed,32)==32); g_Output.Pull(usb,32);
  for (auto &f:usb) {
    assert(f[0]>0 && f[1]<0 && std::abs(f[1]+3*f[0])<=16);
    for (unsigned c=2;c<12;++c) assert(!f[c]);
  }
  assert(g_Output.Push(fixed,32)==32);
  dt_link::Lost(); /* completion/core0 context: immediate epoch closure */
  g_Output.Pull(usb,32);
  for (auto &f:usb) for (auto v:f) assert(!v);
  dt_link::Poll(); assert(!dt_link::VoiceCommand(true));
  lanes[0]=3; lanes[1]=5; handshake(42,lanes);
  render(10); assert(!(g_Output.ControlToken()&1u));
  assert(dt_link::VoiceCommand(true)); dt_link::Poll();
  // A retired HELLO cannot replace the current session or disarm it.
  sc_track_device old; uint8_t wire[SC_TRACK_WIRE_SIZE];
  assert(sc_track_dev_open(&old,41,0,lanes,wire));
  assert(dt_link::SysEx(wire,SC_TRACK_CONTROL_SIZE));
  dt_link::Poll(); assert(g_Output.ControlToken()&1u);
  // Failed ACK publication closes Output immediately, including old FIFO PCM.
  for (unsigned i=0;i<kMidiOutBytes;++i) assert(g_MidiOut.push(0));
  sc_track_frame f={}; f.track=3; f.state.voice=1; f.state.block=clock_at+1;
  assert(sc_track_dev_send(&device,&f,clock_at,wire));
  assert(dt_link::SysEx(wire,sizeof wire)); assert(!(g_Output.ControlToken()&1u));
  memset(usb,0x55,sizeof usb); g_Output.Pull(usb,32);
  for (auto &row:usb) for (auto v:row) assert(!v);
  assert(!dt_link::VoiceCommand(true));
  puts("X01 actual Circle glue/Output: v5/v4 activation, explicit arm, dual independent PCM, native channel mapping, asynchronous loss/epochs, retired sessions and ACK queue refusal: PASS");
}
