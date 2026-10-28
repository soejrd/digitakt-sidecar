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
void usb_track_ui_tick(void *ctrl);
void usb_track_worker_rx_wake(void);
void usb_track_control_set_wakeup(void (*fn)(void));
int usb_track_control_has_wakeup(void);
void usb_track_control_wake(void);
#ifdef USB_TRACK_DIAGNOSTIC
/* usb_track_clock advances once per 32-frame render at 48 kHz: 1500 Hz.
 * Round to nearest ms and cap BEFORE multiplication to avoid overflow. */
static inline uint32_t usb_track_diag_ms(uint32_t blocks)
{
    return blocks >= 15000u ? 9999u : (blocks * 2u + 1u) / 3u;
}
extern volatile uint32_t usb_track_diag_ui_ticks, usb_track_diag_render_wakes;
extern volatile uint32_t usb_track_diag_worker_entries, usb_track_diag_worker_gap_max;
extern volatile uint32_t usb_track_diag_batch_max, usb_track_diag_ui_gap_max;
extern volatile uint32_t usb_track_worker_guard_faults;
extern volatile uint32_t usb_track_diag_window, usb_track_diag_started_at;
extern volatile uint32_t usb_track_diag_worker_window;
void usb_track_diag_reset(void); /* UI task only; never clears safety faults. */
#endif
#endif
