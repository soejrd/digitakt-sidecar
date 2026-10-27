/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef USB_TRACK_H
#define USB_TRACK_H
#include "src_queue.h"
#define USB_TRACK_LEASE 750u
extern volatile uint32_t usb_track_clock, usb_track_route, usb_track_gate, usb_track_ack_clock;
int usb_track_control_open(uint32_t session);
void usb_track_control_close(void);
void usb_track_control_fail(void);
int usb_track_control_failed(void);
int usb_track_control_invalidated(void);
int usb_track_control_peek(struct ut_record *r);
int usb_track_control_pop(struct ut_record *r);
void usb_track_tick(void *ctrl);
#endif
