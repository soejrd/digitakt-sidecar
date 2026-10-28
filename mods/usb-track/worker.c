/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Task-level queue -> protocol -> USB bridge. The render only publishes its
 * clock/route and reads gate + ACK clock; it never enters this worker. */
#include "usb_track.h"
#include "../usb-midi/usb_midi.h"
#include "../../control/device_link.h"
static struct sc_device_link device;
static uint32_t epoch, voice, keepalive_at;
static uint8_t initialized, first;
static volatile uint8_t running;
#ifdef USB_TRACK_DIAGNOSTIC
volatile uint32_t usb_track_worker_ticks;
#endif

void usb_track_worker_rx_wake(void)
{
    /* Once active, ACKs are serviced with the next source event or UI watchdog.
     * Waking a second high-priority task pass for every ACK wastes render
     * margin under dense modulation. Handshake replies still wake immediately.
     * The raw RX queue owns the bytes before this callback runs. */
    if (!running || ((volatile struct sc_device_link *)&device)->state!=SC_DEVICE_ACTIVE)
        usb_track_control_wake();
}

static int send_bytes(const uint8_t *p, uint32_t n)
{
    return usbmidi_device_send(p,n)==0;
}
static void retire(uint8_t reason)
{
    uint8_t out[SC_CONTROL_WIRE_SIZE];
    usb_track_gate=0;
    usb_track_control_close();
    if (running && sc_dev_invalidate(&device,reason,out) && usbmidi_device_link())
        send_bytes(out,sizeof out);
    running=0;
}
static void receive(void *ctx, const uint8_t *p, uint32_t n)
{
    struct sc_control c;
    struct sc_device_update u;
    (void)ctx;
    if (!running) return;
    if (!sc_decode_control(&c,p,n)) { retire(SC_REASON_MALFORMED); return; }
    u=sc_dev_receive_control(&device,&c,usb_track_clock);
    if (u.invalidated) { retire(u.reason); return; }
    if (device.state==SC_DEVICE_ACTIVE) {
        usb_track_ack_clock=usb_track_clock;
        __asm__ volatile ("" ::: "memory");
        usb_track_gate=usb_track_route && !usb_track_control_failed();
    }
}
void usb_track_tick(void *ctrl)
{
#ifdef USB_TRACK_DIAGNOSTIC
    ++usb_track_worker_ticks;
#endif
    struct ut_record r;
    uint8_t out[SC_WIRE_SIZE];
    unsigned budget=usb_track_control_has_wakeup() ? 4 : 63, k;
    uint32_t now=usb_track_clock;
    if (!initialized) {
        usbmidi_device_init(receive,0);
        usbmidi_device_open(); initialized=1;
    }
    usbmidi_device_open(); /* retry if the native RX slot was not ready yet */
    usbmidi_tick(ctrl);
    now=usb_track_clock; /* receive callbacks may have advanced ACK time */
    if (usbmidi_device_failed()) {
        retire(SC_REASON_TRANSPORT);
        usbmidi_device_close(); usbmidi_device_init(receive,0); usbmidi_device_open();
        return;
    }
    if (!usbmidi_device_link() || !usb_track_route) {
        if (running) retire(SC_REASON_TRANSPORT);
        return;
    }
    if (running && (usb_track_control_failed() || usb_track_control_invalidated()))
        retire(SC_REASON_OVERFLOW);
    if (running && sc_dev_tick(&device,now,USB_TRACK_LEASE,out)) {
        usb_track_gate=0; send_bytes(out,SC_CONTROL_WIRE_SIZE);
        usb_track_control_close(); running=0;
    }
    if (!running) {
        /* Host rejects recently retired IDs. Seed from the free-running
         * render clock and advance across every retry; never reuse an ID in
         * this boot. A rejected post-reboot ID times out and advances. */
        if (!epoch) epoch=now ? now : 1;
        if (++epoch==0 || epoch==UINT32_MAX) epoch=1;
        usb_track_control_close();
        if (!usb_track_control_open(epoch)) return;
        if (!sc_dev_open(&device,epoch,now,out) || !send_bytes(out,SC_CONTROL_WIRE_SIZE)) {
            usb_track_control_close(); return;
        }
        running=first=1; voice=0; keepalive_at=now;
    }
    if (device.state==SC_DEVICE_WAIT_HELLO_ACK) {
        usbmidi_tick(ctrl); /* HELLO must leave in this pass, not a later UI tick */
        return;
    }
    while (budget-- && usb_track_control_peek(&r)) {
        if (device.state==SC_DEVICE_WAIT_ACTIVATE && device.count) break;
        struct sc_frame f;
        if (r.session!=device.session) { retire(SC_REASON_SESSION); return; }
        f.session=r.session; f.sequence=1; f.block=r.block;
        /* Resynchronize the existing rendered voice once per session. This
         * first START is recovery, subsequent STARTs are the native bit. */
        if (first || (r.flags & UT_START)) ++voice;
        f.voice=voice; f.flags=(first || (r.flags & UT_START)) ? SC_START : 0;
        f.note=r.note; f.velocity=r.velocity;
        for(k=0;k<8;++k) f.params[k]=r.params[k];
        if (!sc_dev_send_start(&device,&f,f.block,out) || !send_bytes(out,SC_WIRE_SIZE)) {
            retire(SC_REASON_OVERFLOW); return;
        }
        first=0;
        if (!usb_track_control_pop(&r)) { retire(SC_REASON_SEQUENCE); return; }
    }
    if (device.state==SC_DEVICE_ACTIVE && now-keepalive_at>=64) {
        if (sc_dev_keepalive(&device,now,out) && !send_bytes(out,SC_CONTROL_WIRE_SIZE)) {
            retire(SC_REASON_TRANSPORT); return;
        }
        keepalive_at=now;
    }
    usbmidi_tick(ctrl);
    /* Continue a bounded batch only when records can progress. Never spin on
     * WAIT_ACTIVATE: the USB reply or the UI watchdog will wake us. */
#ifndef V01_NO_SELF_WAKE
    if (device.state==SC_DEVICE_ACTIVE && usb_track_control_peek(&r))
        usb_track_control_wake();
#endif
}
