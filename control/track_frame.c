/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "track_frame.h"
#include <string.h>
int sc_track_encode(uint8_t out[SC_TRACK_WIRE_SIZE], const struct sc_track_frame *f)
{
    if (!out || !f || f->track >= SC_TRACKS || !f->state.voice ||
        !sc_encode(out, &f->state)) return 0;
    out[6] = 4;
    out[SC_WIRE_SIZE-1] = f->track;
    out[SC_WIRE_SIZE] = 0xf7;
    return 1;
}
int sc_track_decode(struct sc_track_frame *f, const uint8_t *in, unsigned size)
{
    uint8_t legacy[SC_WIRE_SIZE];
    struct sc_track_frame x;
    if (!f || !in || size != SC_TRACK_WIRE_SIZE || in[6] != 4 ||
        in[SC_WIRE_SIZE] != 0xf7 || in[SC_WIRE_SIZE-1] >= SC_TRACKS) return 0;
    memcpy(legacy, in, sizeof legacy);
    legacy[6] = 1; legacy[SC_WIRE_SIZE-1] = 0xf7;
    if (!sc_decode(&x.state, legacy, sizeof legacy) || !x.state.voice) return 0;
    x.track = in[SC_WIRE_SIZE-1];
    *f = x;
    return 1;
}
