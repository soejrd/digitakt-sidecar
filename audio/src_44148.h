/* SPDX-License-Identifier: CC0-1.0 */
#ifndef SC_SRC_44148_H
#define SC_SRC_44148_H
#include <stdint.h>
#define SC_SRC_TAPS 128u
#define SC_SRC_UP 160u
#define SC_SRC_DOWN 147u
/* Causal 44.1 -> 48 kHz pull converter. Single owner; init before audio.
 * Read until it returns zero, then push exactly one native sample. */
typedef struct sc_src {
    float history[SC_SRC_TAPS];
    uint64_t consumed, produced;
} sc_src;
#ifdef __cplusplus
extern "C" {
#endif
void sc_src_init(sc_src *s);
int sc_src_push(sc_src *s, float sample);
int sc_src_read(sc_src *s, float *sample);
#ifdef __cplusplus
}
#endif
#endif
