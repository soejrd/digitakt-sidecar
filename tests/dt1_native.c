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
    usbmidi_device_close(); assert(test_rx_slot==(uintptr_t)stock);
    usbmidi_device_init(receive,&ours); usbmidi_device_open();
    partial_send=1; assert(usbmidi_device_send(f,sizeof f)==0); usbmidi_tick(0);
    assert(usbmidi_device_failed());
    usbmidi_device_close(); assert(!irq_depth);
    puts("native adapter: 2-argument RX, stock chaining, encoded capacity, queued send, void return, partial-fault: PASS");
}
