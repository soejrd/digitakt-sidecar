// Execute the actual Circle dt_link glue with its real queues and Output.
#include "dt_link.h"
#include "shared.h"
#include "circle_link_adapter.h"
#include <cassert>
#include <cstdio>
Output g_Output;
Spsc<uint8_t,8192> g_MidiIn;
Spsc<uint8_t,kMidiOutBytes> g_MidiOut;
Spsc<TCmd,16> g_Cmds;
static uint8_t pop() { uint8_t b; assert(g_MidiOut.pop(&b)); return b; }
static sc_control reply() {
  assert(g_MidiOut.size()>=2); unsigned n=pop(); n|=pop()<<8;
  assert(n==SC_CONTROL_WIRE_SIZE && g_MidiOut.size()>=n);
  uint8_t b[SC_CONTROL_WIRE_SIZE]; for(unsigned i=0;i<n;++i) b[i]=pop();
  sc_control c; assert(sc_decode_control(&c,b,n)); return c;
}
static void hello(unsigned id,unsigned block) {
  uint8_t b[SC_CONTROL_WIRE_SIZE]; sc_control c={id,1,block,SC_HELLO};
  assert(sc_encode_control(b,&c)); assert(dt_link::SysEx(b,sizeof b));
  do { c=reply(); } while(c.kind==SC_DEACTIVATE);
  assert(c.kind==SC_HELLO_ACK && c.session==id);
}
static void start(unsigned id,unsigned block) {
  uint8_t b[SC_WIRE_SIZE]; sc_frame f={}; f.session=id; f.sequence=1;
#ifdef I04_RENDER_TEST
  f.block=block; f.voice=1; f.flags=SC_START; f.params[0]=60<<8; f.params[1]=65<<8; f.params[3]=62<<8; f.params[4]=91<<8;
#else
  f.block=block; f.voice=1; f.flags=SC_START; f.params[0]=60<<8;
#endif
  assert(sc_encode(b,&f)); assert(dt_link::SysEx(b,sizeof b));
  auto c=reply(); assert(c.kind==SC_ACTIVATE && c.value==block);
}
int main() {
  sc_frame f;
  int16_t pcm[32][8];
  auto silent = [&]() {
    dt_link::RenderTestTone(pcm,32);
    for (auto &row:pcm) for(auto x:row) assert(x==0);
  };
  assert(!dt_link::TestToneCommand(true)); silent();
#ifdef I04_RENDER_TEST
  assert(!dt_link::VoiceCommand(true));
#endif
  assert(!dt_link::Selected()); dt_link::Select(); dt_link::Poll(1);
  assert(!(g_Output.ControlToken()&1));
  hello(41,10); assert(!dt_link::State(&f)); start(41,11); dt_link::Poll(2);
  assert(dt_link::State(&f) && f.params[0]==60<<8 && (g_Output.ControlToken()&1));
#ifdef I04_RENDER_TEST
  silent(); // diagnostic tone still needs explicit enable
  for(unsigned block=0; block<100; ++block) {
    dt_link::RenderVoice(pcm,32);
    for(auto &row:pcm) for(auto x:row) assert(x==0);
  }
  assert(dt_link::VoiceCommand(true));
  dt_link::RenderVoice(pcm,32);
  unsigned synth_nonzero=0;
  for(unsigned block=0; block<100; ++block) {
    dt_link::RenderVoice(pcm,32);
    for(auto &row:pcm) {
      synth_nonzero += row[0]!=0;
      assert(row[0]>=-4095 && row[0]<=4095);
      for(unsigned ch=1;ch<8;++ch) assert(row[ch]==0);
    }
  }
  assert(synth_nonzero>100);
#else
  silent(); // handshake never enables the tone implicitly
#endif
  assert(dt_link::TestToneCommand(true));
  dt_link::RenderTestTone(pcm,32); assert(pcm[0][0]==0);
  for(auto &row:pcm) assert(row[0]>=-2 && row[0]<=2);
  unsigned nonzero=0;
  for(unsigned block=0; block<400; ++block) {
    dt_link::RenderTestTone(pcm,32);
    for(auto &row:pcm) {
      assert(row[0]>=-519 && row[0]<=519);
      nonzero += row[0]!=0;
      for(unsigned v=1;v<8;++v) assert(row[v]==0);
    }
  }
  assert(nonzero>10000);
  dt_link::RenderTestTone(pcm,32);
  assert(pcm[4][0]==519 && pcm[12][0]==-519);
  assert(dt_link::TestToneCommand(false)); silent();
  assert(dt_link::TestToneCommand(true));
  dt_link::Lost(); assert(!(g_Output.ControlToken()&1)); dt_link::Poll(3);
  silent(); assert(!dt_link::TestToneCommand(true));
#ifdef I04_RENDER_TEST
  dt_link::RenderVoice(pcm,32);
  for(auto &row:pcm) for(auto x:row) assert(x==0);
#endif
  assert(!dt_link::State(&f));
  hello(42,20); start(42,21); dt_link::Poll(4); assert(dt_link::State(&f));
  silent(); // reconnect needs a new explicit tone enable
#ifdef I04_RENDER_TEST
  dt_link::RenderVoice(pcm,32);
  for(auto &row:pcm) for(auto x:row) assert(x==0);
#endif
  assert(dt_link::TestToneCommand(true));
  // Production Circle renders 12-frame blocks; leases use 32-frame units.
  // Advance the protocol clock independently of resettable engine statistics.
  dt_link::AdvanceFrames(12); dt_link::Poll(); assert(dt_link::State(&f));
  dt_link::AdvanceFrames(20); dt_link::Poll(); assert(dt_link::State(&f));
  dt_link::AdvanceFrames(32*747); dt_link::Poll(); assert(dt_link::State(&f));
  dt_link::AdvanceFrames(32); dt_link::Poll();
  assert(!(g_Output.ControlToken()&1) && !dt_link::State(&f));
  silent(); assert(!dt_link::TestToneCommand(true));
#ifdef I04_RENDER_TEST
  dt_link::RenderVoice(pcm,32);
  for(auto &row:pcm) for(auto x:row) assert(x==0);
#endif
  while(g_MidiOut.size()) pop();
  while(g_MidiOut.size()<kMidiOutBytes-1) g_MidiOut.push(0);
  uint8_t b[SC_CONTROL_WIRE_SIZE]; sc_control c={43,1,30,SC_HELLO};
  assert(sc_encode_control(b,&c)); assert(dt_link::SysEx(b,sizeof b)); dt_link::Poll(755);
  assert(!(g_Output.ControlToken()&1) && !dt_link::State(&f));
#ifdef I04_RENDER_TEST
  while(g_MidiOut.size()) pop();
  hello(44,30); start(44,31); dt_link::Poll(756);
  sc_frame dense={}; dense.session=44; dense.sequence=2; dense.block=32;
  dense.voice=1; dense.flags=SC_START;
  uint8_t wire[SC_WIRE_SIZE]; assert(sc_encode(wire,&dense));
  assert(dt_link::SysEx(wire,sizeof wire));
  assert(reply().kind==SC_DEACTIVATE); dt_link::Poll(757);
  assert(!dt_link::State(&f) && !(g_Output.ControlToken()&1));
  unsigned stats[8]; dt_link::VoiceStatus(stats);
  assert(stats[0]==0 && stats[1]==0 && stats[3]==1);
  unsigned ft[6]; dt_link::VoiceFault(ft);
  assert(ft[0]==6 && ft[1]==32 && ft[2]==SC_START && ft[3]==1);
  assert(!dt_link::VoiceCommand(true));
  hello(45,40); start(45,41); dt_link::Poll(758);
  dt_link::VoiceFault(ft); assert(ft[0]==6); // survives session clear
  dt_link::RenderVoice(pcm,32);
  for(auto &row:pcm) for(auto x:row) assert(x==0);
#endif
  puts("actual Circle dt_link: handshake, state installation, loss, reconnect, lease, ACK congestion, explicit faded bounded H02 tone: PASS");
}
