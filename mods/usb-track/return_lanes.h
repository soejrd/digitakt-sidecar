/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SC_RETURN_LANES_H
#define SC_RETURN_LANES_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Offline two-lane routing candidate; not installed by usb_track_render.
 * tracks: eight contiguous mono 32-frame blocks. halves: two stereo halves.
 * Map each lane to a distinct track 0..7, or -1 to discard it.
 * ready is supplied by a future multitrack lifecycle owner, never by USB I/O.
 * A valid map with !ready/invalid selector silences mapped tracks and flushes
 * both halves. Invalid maps leave tracks intact and flush USB. */
int sc_return_lanes(int32_t tracks[256], volatile int32_t halves[128],
                    unsigned selector, unsigned ready, const int8_t map[2]);
#ifdef __cplusplus
}
#endif
#endif
