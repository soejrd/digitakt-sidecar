/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>

/* H02 diagnostic only: 3 kHz at 48 kHz, <= -36 dBFS before MAIN's
 * additional 0.5 gain (-42 dBFS on MAIN), 200 ms fade on every enable.
 * Single owner: Circle core 3. No allocation, USB access or waits. */
class H02TestTone {
 public:
  bool Enable(bool on, bool allowed, uint32_t session) {
    Disable();
    if (!on) return true;
    if (!allowed || !session) return false;
    session_ = session;
    return true;
  }
  void Disable() { session_ = 0; phase_ = faded_ = 0; }
  bool Enabled() const { return session_ != 0; }
  void Render(int16_t (*out)[8], unsigned frames, bool allowed, uint32_t session) {
    if (!allowed || session != session_) Disable();
    static const int16_t wave[16] = {
      0,199,367,479,519,479,367,199,0,-199,-367,-479,-519,-479,-367,-199
    };
    for (unsigned i = 0; i < frames; ++i) {
      for (unsigned v = 0; v < 8; ++v) out[i][v] = 0;
      if (!Enabled()) continue;
      const uint32_t gain = faded_ < 9600 ? faded_++ : 9600;
      out[i][0] = (int16_t)(wave[phase_++ & 15u] * (int32_t)gain / 9600);
    }
  }
 private:
  uint32_t session_ = 0, phase_ = 0, faded_ = 0;
};
