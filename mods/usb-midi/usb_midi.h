/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef DT1_USB_MIDI_H
#define DT1_USB_MIDI_H
#include <stdint.h>
void usbmidi_device_init(void (*receive)(void *, const uint8_t *, uint32_t), void *ctx);
void usbmidi_device_open(void);
void usbmidi_device_close(void);
int usbmidi_device_send(const uint8_t *bytes, uint32_t n);
int usbmidi_device_link(void);
int usbmidi_device_failed(void);
void usbmidi_tick(void *ctrl);
/* Read-only diagnostics; no firmware calls, transmission or queue changes. */
uint32_t usbmidi_device_diag(unsigned field);
#endif
