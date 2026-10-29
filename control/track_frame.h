/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SC_TRACK_FRAME_H
#define SC_TRACK_FRAME_H
#include "link.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SC_TRACKS 8u
#define SC_TRACK_WIRE_SIZE (SC_WIRE_SIZE + 1u)
/* Experimental version 4: exact v1 snapshot, track byte before F7.
 * sequence is session-global; voice remains a PER-TRACK hit generation.
 * This is an offline protocol candidate, not negotiated by the v1/v3 worker. */
struct sc_track_frame { struct sc_frame state; uint8_t track; };
int sc_track_encode(uint8_t out[SC_TRACK_WIRE_SIZE], const struct sc_track_frame *f);
int sc_track_decode(struct sc_track_frame *f, const uint8_t *in, unsigned size);
#ifdef __cplusplus
}
#endif
#endif
