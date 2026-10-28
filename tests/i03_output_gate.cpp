#include "output.h"
#include <cassert>
#include <cstdio>
int main() {
  Output o; int16_t frames[32][8]; int32_t out[32][12];
  for(auto &f:frames) for(auto &x:f) x=20000;
  auto closed=o.ControlToken(); o.ControlClose();
  o.ControlSet(closed,true); // stale ACK cannot reopen after asynchronous loss
  assert(!(o.ControlToken()&1)); o.Push(frames,32); o.Pull(out,32);
  for(auto &f:out) for(auto x:f) assert(x==0);
  auto fresh=o.ControlToken(); o.ControlSet(fresh,true); assert(o.ControlToken()&1);
  o.Push(frames,32); o.Pull(out,32); // transition flushes all old FIFO data
  for(auto &f:out) for(auto x:f) assert(x==0);
  o.Push(frames,32); o.Pull(out,32); assert(out[31][0]!=0);
  o.ControlClose(); o.Push(frames,32); o.Pull(out,32);
  for(auto &f:out) for(auto x:f) assert(x==0);
  assert(o.RequestedFrames()==128);
  puts("Circle Output: closed gate, stale ACK rejection, FIFO epoch/fade, fresh PCM: PASS");
}
