/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "circle_usbmidi.h"
#include <string.h>

static int send_reply(void *ctx, const uint8_t *frame, uint32_t size)
{
    struct sc_circle_usbmidi *u = ctx;
    if (u->midi == 0 || u->send_plain_midi == 0) return 0;
    return u->send_plain_midi(u->midi, 0, frame, size, 0) != 0;
}

void sc_circle_usbmidi_init(struct sc_circle_usbmidi *u, void *midi,
                            int (*send_plain_midi)(void *, unsigned,
                                                   const uint8_t *, unsigned,
                                                   unsigned),
                            sc_install_fn install, sc_circle_clear_fn clear,
                            void *ctx, uint32_t lease_blocks)
{
    (void)ctx;
    memset(u, 0, sizeof *u);
    u->midi = midi;
    u->send_plain_midi = send_plain_midi;
    sc_circle_init(&u->link, install, send_reply, clear, u, lease_blocks);
}

void sc_circle_usbmidi_feed(struct sc_circle_usbmidi *u, const uint8_t *bytes,
                            unsigned size)
{
    unsigned i;
    if (u == 0 || bytes == 0) return;
    for (i = 0; i < size; ++i) sc_circle_midi_byte(&u->link, bytes[i]);
}

unsigned sc_circle_usbmidi_process(struct sc_circle_usbmidi *u,
                                   unsigned budget)
{
    return u == 0 ? 0 : sc_circle_process(&u->link, budget);
}

void sc_circle_usbmidi_advance_block(struct sc_circle_usbmidi *u,
                                     uint32_t block)
{
    if (u != 0) sc_circle_advance_block(&u->link, block);
}

void sc_circle_usbmidi_transport_lost(struct sc_circle_usbmidi *u,
                                      uint8_t reason)
{
    if (u != 0) sc_circle_transport_lost(&u->link, reason);
}

int sc_circle_usbmidi_audio_allowed(const struct sc_circle_usbmidi *u)
{
    return u != 0 && sc_circle_audio_allowed(&u->link);
}
