/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef USB_TRACK_QUEUE_IO_H
#define USB_TRACK_QUEUE_IO_H
#include <stdint.h>
extern uint32_t fw_io_selector, fw_io_mode, fw_io_sync;
extern volatile int32_t fw_io_halves[128];
#define USB_SELECTOR (*(volatile uint32_t *)&fw_io_selector)
#define USB_MODE     (*(volatile uint32_t *)&fw_io_mode)
#define USB_SYNC     (*(volatile uint32_t *)&fw_io_sync)
#define USB_HALVES   ((volatile int32_t *)fw_io_halves)
#endif
