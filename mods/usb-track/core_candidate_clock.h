/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CORE_CANDIDATE_CLOCK_H
#define CORE_CANDIDATE_CLOCK_H
#include <stdint.h>
/* Read-only OS 1.54 DMA timer 0 counter/mode. Never configure a timer.
 * Report RAW ticks and mode: hardware rate must be calibrated externally.
 * Emulator models this counter; its instruction rate is not hardware time. */
#if defined(USB_TRACK_IO_OVERRIDES) || defined(DT1_NATIVE_TEST)
extern uint32_t v01_test_clock, v01_test_clock_mode;
#define v01_clock() (v01_test_clock)
#define v01_clock_mode() (v01_test_clock_mode)
#else
static inline uint32_t v01_clock(void) { return *(volatile uint32_t *)0xfc07000cu; }
static inline uint32_t v01_clock_mode(void) { return *(volatile uint16_t *)0xfc070000u; }
#endif
#endif
