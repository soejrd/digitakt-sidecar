// SPDX-License-Identifier: GPL-2.0-or-later
// Opt-in X01 Circle adapter. Default I04/V01 glue is not replaced in deps/.
#include "dt_link.h"
#include "track_host.h"
#include "v01_diag.h"
#include "shared.h"
#include <cstring>
namespace dt_link {
static sc_track_host peer;
static uint32_t selected,losses,seen_loss,clock_block,remainder,armed_session,faded;
static uint32_t diag_values[V01_FIELDS],diag_received,diag_bad,diag_version;
static bool initialized,trace_enabled;
static unsigned playout_faults;
static sc_playout_failure last_failure;
static void RememberFailure(bool was_open) {
  if (was_open && !peer.opened && peer.reason==SC_REASON_INSTALL) {
    ++playout_faults; last_failure=peer.bank.failure;
  }
}
static int Send(void *,const uint8_t *p,unsigned n) {
  if (n+2u>kMidiOutBytes-g_MidiOut.size()) return 0;
  g_MidiOut.push(n&255u); g_MidiOut.push(n>>8);
  for (unsigned i=0;i<n;++i) g_MidiOut.push(p[i]);
  return 1;
}
static void Init() {
  if (!initialized) { sc_track_host_init(&peer,Send,nullptr,128,750); initialized=true; }
}
void Select() {
  g_Output.ControlClose();
  __atomic_add_fetch(&losses,1u,__ATOMIC_RELEASE);
  __atomic_store_n(&selected,1u,__ATOMIC_RELEASE);
}
bool Selected() { return __atomic_load_n(&selected,__ATOMIC_ACQUIRE)!=0; }
void Lost() { if (Selected()) { g_Output.ControlClose(); __atomic_add_fetch(&losses,1u,__ATOMIC_RELEASE); } }
static void CheckLoss() {
  uint32_t lost=__atomic_load_n(&losses,__ATOMIC_ACQUIRE);
  if (lost!=seen_loss) {
    seen_loss=lost; armed_session=faded=0;
    sc_track_host_stop(&peer,SC_REASON_TRANSPORT);
    g_Output.ControlClose();
  }
}
void Poll(uint32_t block) {
  if (!Selected()) return;
  Init(); clock_block=block; CheckLoss();
  uint32_t token=g_Output.ControlToken(),lost=__atomic_load_n(&losses,__ATOMIC_ACQUIRE);
  sc_track_host_tick(&peer,block);
  if (peer.opened) g_Output.SetMode(Output::kStereoReturn);
  bool allowed=sc_track_host_allowed(&peer) && armed_session==peer.bank.session &&
               lost==seen_loss && lost==__atomic_load_n(&losses,__ATOMIC_ACQUIRE);
  if (!allowed) { armed_session=faded=0; }
  g_Output.ControlSet(token,allowed);
}
void Poll() { Poll(clock_block); }
void AdvanceFrames(unsigned frames) {
  uint64_t total=(uint64_t)remainder+frames;
  clock_block+=(uint32_t)(total/32u); remainder=(uint32_t)(total%32u);
}
bool SysEx(const uint8_t *p,unsigned n) {
  if (v01_message(p,n)) {
    if (v01_decode(diag_values,p,n)) { ++diag_received; diag_version=p[5]; } else ++diag_bad;
    return true;
  }
  if (!Selected() || !sc_is_dtsc(p,n)) return false;
  Init(); CheckLoss();
  uint32_t session=peer.bank.session;
  bool was_open=peer.opened!=0;
  (void)sc_track_host_receive(&peer,p,n,clock_block);
  RememberFailure(was_open);
  if (session!=peer.bank.session || !sc_track_host_allowed(&peer)) {
    armed_session=faded=0; g_Output.ControlClose();
  }
  return true;
}
void RenderVoice(int16_t (*out)[8],unsigned frames) {
  Init(); CheckLoss();
  uint32_t lost=__atomic_load_n(&losses,__ATOMIC_ACQUIRE);
  bool was_open=peer.opened!=0;
  bool accepted=sc_track_host_render(&peer,out,frames)!=0;
  RememberFailure(was_open);
  if (!accepted || armed_session!=peer.bank.session || lost!=seen_loss ||
      lost!=__atomic_load_n(&losses,__ATOMIC_ACQUIRE)) {
    memset(out,0,frames*sizeof *out);
    if (!accepted) { armed_session=faded=0; g_Output.ControlClose(); }
    return;
  }
  for (unsigned i=0;i<frames;++i) {
    uint32_t gain=faded<9600u ? faded++ : 9600u;
    for (unsigned lane=0;lane<2;++lane) out[i][lane]=(int16_t)((int32_t)out[i][lane]*(int32_t)gain/9600);
  }
}
bool VoiceCommand(bool on) {
  Init(); CheckLoss(); armed_session=faded=0; g_Output.ControlClose();
  if (!on) return true;
  if (trace_enabled || !Selected() || !sc_track_host_allowed(&peer)) return false;
  armed_session=peer.bank.session; return true;
}
void VoiceStatus(unsigned out[8]) {
  Init();
  out[0]=armed_session!=0; out[1]=peer.bank.write-peer.bank.read; out[2]=peer.bank.peak;
  out[3]=playout_faults; out[4]=(unsigned)peer.bank.applied; out[5]=out[6]=0;
  for (unsigned t=0;t<8;++t) { out[5]+=(unsigned)peer.bank.voice[t].starts; out[6]+=(unsigned)peer.bank.voice[t].consumed_starts; }
  out[7]=(unsigned)peer.bank.clock;
}
void VoiceFault(unsigned out[6]) {
  const auto &f=last_failure;
  out[0]=f.code; out[1]=f.block; out[2]=f.flags; out[3]=f.delta; out[4]=(unsigned)f.clock; out[5]=(unsigned)f.due;
}
// Legacy one-track snapshot/tone/trace interfaces are deliberately unavailable.
bool State(sc_frame *) { return false; }
bool TestToneCommand(bool on) { return !on; }
void RenderTestTone(int16_t (*out)[8],unsigned n) { memset(out,0,n*sizeof *out); }
void TraceEnable(bool on) { trace_enabled=on; VoiceCommand(false); }
unsigned TraceCount() { return 0; }
bool TraceFrozen() { return false; }
bool TraceRead(unsigned,TraceRecord *) { return false; }
unsigned RemoteVersion() { return diag_version; }
void RemoteStatus(unsigned out[19]) {
  out[0]=diag_received; out[1]=diag_bad;
  for (unsigned i=0;i<V01_FIELDS;++i) out[i+2]=diag_values[i];
}
}
