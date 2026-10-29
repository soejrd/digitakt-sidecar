/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "x01_track.h"
#include "../usb-midi/usb_midi.h"
static struct sc_track_device x01_device;
static uint32_t x01_epoch,x01_keepalive,x01_retry,x01_tx_progress,x01_tx_since;
static uint32_t x01_generation[2];
static uint8_t x01_initialized,x01_running;
void usb_track_worker_rx_wake(void) { usb_track_control_wake(); }
static int x01_send(const uint8_t *wire,unsigned n) { return usbmidi_device_send(wire,n)==0; }
static void x01_retire(unsigned reason)
{
    struct sc_track_control c; uint8_t wire[SC_TRACK_CONTROL_SIZE];
    usb_track_gate=0; x01_capture_close();
    if (x01_running) {
        c.state.session=x01_device.session; c.state.sequence=x01_device.ack_sequence ? x01_device.ack_sequence : 1;
        c.state.kind=SC_DEACTIVATE; c.state.value=reason;
        c.lanes[0]=x01_device.lanes[0]; c.lanes[1]=x01_device.lanes[1];
        sc_track_dev_close(&x01_device,reason);
        if (sc_track_control_encode(wire,&c) && usbmidi_device_link()) (void)x01_send(wire,sizeof wire);
    }
    x01_running=0;
}
static void x01_receive(void *ctx,const uint8_t *wire,uint32_t n)
{
    (void)ctx;
    if (!x01_running) return;
    if (!sc_track_dev_receive(&x01_device,wire,n,usb_track_clock)) {
        if (x01_device.failed) x01_retire(x01_device.failed);
        return;
    }
    if (sc_track_dev_allowed(&x01_device) && !x01_capture_invalid()) {
        usb_track_ack_clock=usb_track_clock;
        __asm__ volatile ("" ::: "memory");
        usb_track_gate=usb_track_route;
    }
}
void usb_track_tick(void *ctrl)
{
    uint8_t wire[SC_TRACK_WIRE_SIZE]; struct ut_record record;
    if (!x01_initialized) {
        usbmidi_device_init(x01_receive,0); usbmidi_device_open(); x01_initialized=1;
    }
    usbmidi_device_open(); usbmidi_tick(ctrl);
    uint32_t now=usb_track_clock,submitted=usbmidi_device_diag(1);
    if (submitted!=x01_tx_progress || !usbmidi_device_diag(4)) {
        x01_tx_progress=submitted; x01_tx_since=now;
    }
    /* Bound the existing adapter's zero-return deferral without modifying V01.
     * Even keepalives/duplicate ACKs cannot renew a stalled TX queue forever. */
    if (usbmidi_device_failed() || (usbmidi_device_diag(4) && now-x01_tx_since>=USB_TRACK_LEASE)) {
        x01_retire(SC_REASON_TRANSPORT); usbmidi_device_close();
        usbmidi_device_init(x01_receive,0); usbmidi_device_open(); return;
    }
    if (!usbmidi_device_link() || !usb_track_route) {
        if (x01_running) x01_retire(SC_REASON_TRANSPORT);
        return;
    }
    if (x01_running && (x01_capture_invalid() || sc_track_dev_tick(&x01_device,now,USB_TRACK_LEASE)))
        x01_retire(SC_REASON_TIMEOUT);
    if (!x01_running) {
        const uint32_t packed=x01_route_lanes;
        uint8_t lanes[2]={(uint8_t)packed,(uint8_t)(packed>>8)};
        if (!sc_track_mask(lanes)) return;
        if (!x01_epoch) x01_epoch=now ? now : 1;
        if (++x01_epoch==0 || x01_epoch==UINT32_MAX) x01_epoch=1;
        if (!sc_track_dev_open(&x01_device,x01_epoch,now,lanes,wire) ||
            !x01_capture_open(x01_epoch,lanes) || !x01_send(wire,SC_TRACK_CONTROL_SIZE)) return;
        x01_generation[0]=x01_generation[1]=0; x01_running=1;
        x01_keepalive=x01_retry=now;
    }
    unsigned budget=4,lane;
    while (x01_device.negotiated && x01_device.count<SC_TRACK_WINDOW && budget-- &&
           x01_capture_peek(&lane,&record)) {
        struct sc_track_frame f;
        f.track=x01_device.lanes[lane]; f.state.session=record.session;
        f.state.sequence=1; f.state.block=record.block;
        f.state.flags=record.flags&UT_START ? SC_START : 0;
        if (f.state.flags) ++x01_generation[lane];
        f.state.voice=x01_generation[lane]; f.state.note=record.note; f.state.velocity=record.velocity;
        for (unsigned k=0;k<8;++k) f.state.params[k]=record.params[k];
        if (record.session!=x01_device.session || !sc_track_dev_send(&x01_device,&f,now,wire) ||
            !x01_send(wire,SC_TRACK_WIRE_SIZE) || !x01_capture_pop(lane,&record)) {
            x01_retire(SC_REASON_OVERFLOW); return;
        }
    }
    if (now-x01_retry>=64) {
        unsigned size=0;
        if (!x01_device.negotiated && sc_track_dev_hello(&x01_device,wire)) size=SC_TRACK_CONTROL_SIZE;
        else if (sc_track_dev_replay(&x01_device,wire)) size=SC_TRACK_WIRE_SIZE;
        if (size && !x01_send(wire,size)) {
            x01_retire(SC_REASON_TRANSPORT); return;
        }
        x01_retry=now;
    }
    if (now-x01_keepalive>=64) {
        if (sc_track_dev_keepalive(&x01_device,wire) && !x01_send(wire,SC_TRACK_CONTROL_SIZE)) {
            x01_retire(SC_REASON_TRANSPORT); return;
        }
        x01_keepalive=now;
    }
    usbmidi_tick(ctrl);
    if (x01_device.negotiated && x01_device.count<SC_TRACK_WINDOW && x01_capture_peek(&lane,&record))
        usb_track_control_wake();
}
