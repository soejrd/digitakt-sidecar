/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef X01_TRACK_H
#define X01_TRACK_H
#include "usb_track.h"
#include "../../control/track_session.h"
extern volatile uint32_t x01_route_lanes;
int x01_capture_open(uint32_t session,const uint8_t lanes[2]);
void x01_capture_close(void);
int x01_capture_invalid(void);
int x01_capture_peek(unsigned *lane,struct ut_record *record);
int x01_capture_pop(unsigned lane,struct ut_record *record);
#endif
