/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef DT1_NATIVE_IO_H
#define DT1_NATIVE_IO_H
static uint32_t test_ready, test_isr, test_index, test_queued, test_sysex;
static uintptr_t test_rx_slot;
static unsigned irq_depth, native_calls, partial_send, test_auto_complete;
/* Simulates the stock USB stack loading the queue between an unlocked
 * preflight and the masked call: runs once, inside irq_save(), and only for
 * the call that marks itself with test_race_next(). */
static unsigned test_race_armed, test_race_hits;
static uint32_t test_race_queued, test_race_index;
#define test_race_next() (test_race_armed = 1u)
static void (*test_tx_observer)(const uint8_t *,uint32_t);
static void (*test_restore_observer)(void);
#define USB_READY test_ready
#define USB_ISR_ON test_isr
#define USB_INDEX test_index
#define USB_QUEUED test_queued
#define USB_SYSEX test_sysex
#define USB_RX_SLOT test_rx_slot
static uint16_t irq_save(void)
{
    uint16_t old=irq_depth; ++irq_depth;
    if (test_race_armed) {
        test_race_armed=0; ++test_race_hits;
        test_queued=test_race_queued; test_index=test_race_index;
    }
    return old;
}
static void irq_restore(uint16_t old) { irq_depth=old; if(!old && test_auto_complete) test_queued=0; if(!old && test_restore_observer) test_restore_observer(); }
static void test_native_send(uint32_t n, const uint8_t *p)
{
    uint32_t encoded=4*((n+2)/3);
    assert(irq_depth && p[0]==0xf0 && p[n-1]==0xf7);
    /* Entering 0x40004990 in either wait state spins on a value only the
     * completion interrupt lowers, and we hold that interrupt masked: the
     * device never returns. A test that reaches this point would hang the
     * Digitakt, so assert rather than model the spin away. */
    /* Every byte must pass 0x400049FE..0x40004A0C: it waits while
     * queued + written > 0xffb, so the whole encoding must fit below it. */
    assert(test_queued + encoded <= 0xffb);             /* 0x400049FE */
    /* 0x400049D0: the wrap wait, whenever the native code would wrap. */
    assert(!(n > 0x2f && test_index > 0xfbf &&
             test_queued >= test_index - 0x3f));
    /* A message may cross 0xFFF only when that wrap handles it first;
     * otherwise 0x40004C22 submits a descriptor mid-message. */
    assert(!(n > 0x2f && test_index <= 0xfbf &&
             test_index + encoded > 0xfff));
    ++native_calls;
    if (!partial_send) { test_queued += 4*((n+2)/3); if(test_tx_observer) test_tx_observer(p,n); }
}
#define USB_SEND(n,p) test_native_send(n,p)
#endif
