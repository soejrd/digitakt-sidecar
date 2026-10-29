/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define DT1_NATIVE_TEST
#include "../mods/usb-midi/usb_midi.c"
static unsigned ours, foreign;
static uint8_t stock_bytes[256];
static void stock(const uint8_t *p, uint32_t n)
{
    assert(foreign+n<=sizeof stock_bytes);
    memcpy(stock_bytes+foreign,p,n); foreign+=n;
}
static void receive(void *ctx, const uint8_t *p, uint32_t n)
{
    assert(ctx==&ours && n==24 && p[0]==0xf0 && p[n-1]==0xf7); ++ours;
}
int main(void)
{
    uint8_t f[24]={0xf0,0x7d,'D','T','S','C',2};
    const uint8_t notes[]={0x90,60,0,0xf8,0xf0,1,2,0xf7,0,0};
    f[23]=0xf7;
    test_ready=test_isr=1; test_rx_slot=(uintptr_t)stock;
    usbmidi_device_init(receive,&ours); usbmidi_device_open();
    assert(test_rx_slot==(uintptr_t)native_rx);
    /* Invoke EXACT native ABI, with no context slot. */
    ((void (*)(const uint8_t *,uint32_t))test_rx_slot)(f,sizeof f);
    ((void (*)(const uint8_t *,uint32_t))test_rx_slot)(notes,sizeof notes);
    usbmidi_tick(0);
    assert(ours==1 && foreign==sizeof notes && !memcmp(notes,stock_bytes,sizeof notes));
    /* Reserve every encoded byte, not just the first input byte. */
    test_queued=0xffb-31; assert(!fw_can_send(0,sizeof f));
    assert(usbmidi_device_send(f,sizeof f)==0); usbmidi_tick(0);
    assert(native_calls==0 && transport.tx_head-transport.tx_tail==1);
    test_queued=0xffb-32; usbmidi_tick(0);
    assert(native_calls==1 && test_queued==0xffb && transport.tx_head==transport.tx_tail);
    test_queued=0; test_sysex=0xf0; assert(!fw_can_send(0,sizeof f)); test_sysex=0;
    /* The preflight must be authoritative inside the mask. Call fw_send with
     * a queue that passes the unlocked check, then let the stock USB stack
     * load it at the instant interrupts are masked, exactly as attach does.
     * Before the fix this entered 0x40004990 and the capacity wait at
     * 0x400049FE could never return: the device hung. */
    {
        unsigned before=native_calls;
        test_queued=0; test_index=0; test_race_hits=0;
        test_race_queued=0xffb; test_race_next();
        assert(fw_send(0,f,sizeof f)==0);          /* refused, not hung */
        assert(test_race_hits==1);                 /* the race really happened */
        assert(native_calls==before);              /* sender never entered */
        assert(!irq_depth && test_queued==0xffb);
    }
    /* Same window, but the stock stack fills the queue only up to the native
     * capacity limit: the message still fits, so it must be sent. A guard that
     * refuses here would stall, because only our own sends drain the index. */
    {
        unsigned before=native_calls;
        test_queued=0; test_index=0xf00; test_race_hits=0;
        test_race_queued=0xffb-32; test_race_index=0xf00;
        test_race_next();
        assert(fw_send(0,f,sizeof f)==sizeof f);
        assert(test_race_hits==1 && native_calls==before+1);
    }
    /* And the healthy case still sends. */
    test_queued=0; test_index=0; test_race_queued=0; test_race_index=0;
    {
        unsigned before=native_calls;
        assert(fw_send(0,f,sizeof f)==sizeof f);
        assert(native_calls==before+1 && test_queued==32);
    }
    /* The index wrap must never be refused early: only our own sends advance
     * the index, so a refusal band below 0xFBF stalls traffic forever (the
     * emulator showed exactly that on an earlier version of this guard).
     * A 24-byte message needs 0x24 bytes, so 0xFBF-0x24 still fits without
     * crossing 0xFFF, and 0xFC0+ only wraps when the native code would. */
    test_queued=0; test_index=0xfbfu-0x24; assert(fw_can_send(0,sizeof f));
    test_index=0xfbfu-0x23; assert(fw_can_send(0,sizeof f)); /* native wraps */
    test_queued=0xffb-32; assert(fw_can_send(0,sizeof f));   /* queued drained enough */
    /* Transport-level deferral: a message refused with the queue loaded stays
     * at the head and is submitted on a later tick once completions drain it. */
    test_index=0; test_queued=0xffb; test_race_queued=0; test_race_index=0;
    assert(usbmidi_device_send(f,sizeof f)==0);
    usbmidi_tick(0);
    assert(transport.tx_head-transport.tx_tail==1);
    test_queued=0;
    usbmidi_tick(0);
    assert(test_queued==32 && transport.tx_head==transport.tx_tail);
    test_queued=0;
    /* Diagnostics observe the production adapter without changing transport. */
    struct dt1_transport saved = transport;
    assert(usbmidi_device_diag(0)==125);
    assert(usbmidi_device_diag(1)==2 && usbmidi_device_diag(3)==2);
    assert(usbmidi_device_diag(2)==sizeof f+sizeof notes);
    assert(!memcmp(&saved,&transport,sizeof saved));
    usbmidi_device_close(); assert(test_rx_slot==(uintptr_t)stock);
    usbmidi_device_init(receive,&ours); usbmidi_device_open();
    partial_send=1; assert(usbmidi_device_send(f,sizeof f)==0); usbmidi_tick(0);
    assert(usbmidi_device_failed());
    usbmidi_device_close(); assert(!irq_depth);
    puts("native adapter: 2-argument RX, stock chaining, encoded capacity, queued send, void return, masked re-check race, no early-wrap stall, partial-fault: PASS");
}
