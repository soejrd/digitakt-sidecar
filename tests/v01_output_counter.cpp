// Source-level diagnostic: intentional epoch flushes vs producer starvation.
#include "output.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main(int argc, char **argv) {
  Output output;
  int16_t pcm[32][8] = {};
  int32_t usb[32][12];
  for (auto &frame : pcm) frame[0] = 1000;
  auto pull = [&]() {
    output.Pull(usb, 32);
    for (auto &frame : usb) for (auto sample : frame) assert(sample == 0);
  };
  assert(output.Push(pcm, 32) == 32 && output.Level() == 32);
  output.ControlClose();
  pull();
  const unsigned close = output.Underflows();
  assert(close == 32 && output.Level() == 0);
  output.ControlSet(output.ControlToken(), true);
  assert(output.Push(pcm, 32) == 32 && output.Level() == 32);
  pull();
  const unsigned reopen = output.Underflows() - close;
  assert(reopen == 32);
  assert(output.Push(pcm, 32) == 32);
  output.Pull(usb, 32);
  assert(output.Underflows() == 64 && usb[31][0] != 0);
  output.ControlClose();
  assert(output.Push(pcm, 32) == 32 && output.Level() == 32);
  pull();
  const unsigned second_close = output.Underflows() - close - reopen;
  assert(second_close == 32);
  if (argc > 1 && std::strcmp(argv[1], "--require-no-flush-underflows") == 0) {
    std::fprintf(stderr, "Counter interpretation disproved: prefilled FIFO, close/reopen/close counted %u/%u/%u empty frames after intentional flushes\n",
                 close, reopen, second_close);
    return 1;
  }
  std::printf("{\"close_flush_count\":%u,\"reopen_flush_count\":%u,\"second_close_flush_count\":%u,\"steady_prefilled_count\":0,\"pass\":true}\n",
              close, reopen, second_close);
}
