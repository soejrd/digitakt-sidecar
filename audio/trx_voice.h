/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef DTSC_TRX_VOICE_H
#define DTSC_TRX_VOICE_H
#include "link.h"
#include "mdkit_trx_b2.h"
#include "src_44148.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Core 3 owns all operations. Init before audio starts; no allocation or USB.
 * PTCH is authoritative; note is ignored. Velocity is a linear per-hit gain, applied once after the return limiter.
 * Final SRC snapshots bypass the pinned control-clock target smoother; native
 * DSP/render timing remains unchanged. START retriggers; updates keep the tail. */
typedef struct sc_trx_voice {
    mdkit_trx_b2 engine;
    sc_src converter;
    float velocity_gain;
    uint64_t frames, native_frames, starts, consumed_starts;
} sc_trx_voice;
void sc_trx_init(sc_trx_voice *v);
void sc_trx_clear(sc_trx_voice *v);
int sc_trx_install(sc_trx_voice *v, const struct sc_frame *f);
void sc_trx_render(sc_trx_voice *v, int16_t (*out)[8], unsigned frames);
#ifdef __cplusplus
}
#endif
#endif
