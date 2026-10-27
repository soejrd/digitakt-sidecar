/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef DT1_NATIVE_IO_H
#define DT1_NATIVE_IO_H
static uint32_t test_ready, test_isr, test_index, test_queued, test_sysex;
static uintptr_t test_rx_slot;
static unsigned irq_depth, native_calls, partial_send, test_auto_complete;
static void (*test_tx_observer)(const uint8_t *,uint32_t);
static void (*test_restore_observer)(void);
#define USB_READY test_ready
#define USB_ISR_ON test_isr
#define USB_INDEX test_index
#define USB_QUEUED test_queued
#define USB_SYSEX test_sysex
#define USB_RX_SLOT test_rx_slot
static uint16_t irq_save(void) { uint16_t old=irq_depth; ++irq_depth; return old; }
static void irq_restore(uint16_t old) { irq_depth=old; if(!old && test_auto_complete) test_queued=0; if(!old && test_restore_observer) test_restore_observer(); }
static void test_native_send(uint32_t n, const uint8_t *p)
{
    assert(irq_depth && p[0]==0xf0 && p[n-1]==0xf7);
    ++native_calls;
    if (!partial_send) { test_queued += 4*((n+2)/3); if(test_tx_observer) test_tx_observer(p,n); }
}
#define USB_SEND(n,p) test_native_send(n,p)
#endif
