/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIDECAR_CIRCLE_USBMIDI_H
#define SIDECAR_CIRCLE_USBMIDI_H
#include "circle_link_adapter.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Bridges Circle's CUSBMIDIDevice to sc_circle_link.
 * MIDI received from the Digitakt is fed only by FeedPacket (packet handler
 * context); replies are queued by the task-level Process through
 * SendPlainMIDI and are never sent from the render path. Ordinary MIDI is
 * untouched: only a complete, validated DTSC SysEx enters the link queue. */
struct sc_circle_usbmidi {
    struct sc_circle_link link;
    void *midi;                          /* CUSBMIDIDevice*, opaque here */
    int (*send_plain_midi)(void *midi, unsigned cable, const uint8_t *data,
                           unsigned size, unsigned chunk);
};

void sc_circle_usbmidi_init(struct sc_circle_usbmidi *u, void *midi,
                            int (*send_plain_midi)(void *, unsigned,
                                                   const uint8_t *, unsigned,
                                                   unsigned),
                            sc_install_fn install, sc_circle_clear_fn clear,
                            void *ctx, uint32_t lease_blocks);
/* Circle packet handler context; nLength is 1..3 MIDI bytes for one cable. */
void sc_circle_usbmidi_feed(struct sc_circle_usbmidi *u, const uint8_t *bytes,
                            unsigned size);
/* Task context. Returns frames processed. */
unsigned sc_circle_usbmidi_process(struct sc_circle_usbmidi *u,
                                   unsigned budget);
void sc_circle_usbmidi_advance_block(struct sc_circle_usbmidi *u,
                                     uint32_t block);
void sc_circle_usbmidi_transport_lost(struct sc_circle_usbmidi *u,
                                      uint8_t reason);
int sc_circle_usbmidi_audio_allowed(const struct sc_circle_usbmidi *u);
#ifdef __cplusplus
}
#endif
#endif
