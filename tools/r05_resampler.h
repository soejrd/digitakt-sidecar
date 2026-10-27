/* SPDX-License-Identifier: CC0-1.0
 * Streaming 160:147 resampler for TRX-B2's native 44.1 kHz output.
 * Fixed state; no allocation. Input and output may be processed in arbitrary
 * block sizes while preserving phase across calls.
 */
#ifndef R05_RESAMPLER_H
#define R05_RESAMPLER_H
#include <stddef.h>
#include <stdint.h>

#define R05_RATE_IN 44100u
#define R05_RATE_OUT 48000u
#define R05_UP 160u
#define R05_DOWN 147u
#define R05_TAPS 32u
#define R05_HISTORY 64u

typedef struct r05_resampler {
    float history[R05_HISTORY];
    uint64_t consumed;
    uint64_t produced;
} r05_resampler;

void r05_resampler_init(r05_resampler *s);
/* Convert `in` to `out`; `out` holds at least ceil(frames*160/147) + 1. */
size_t r05_resampler_process(r05_resampler *s, const float *in, size_t frames,
                             float *out, size_t out_capacity);
/* Produce the remaining output for all input consumed so far, zero-padding the
 * causal filter tail. Returns 0 defects; output count uses exact 160/147. */
size_t r05_resampler_finish(r05_resampler *s, float *out, size_t out_capacity);
#endif
