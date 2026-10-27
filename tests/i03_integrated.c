/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Execute production firmware C, native two-argument RX bridge and the real
 * portable Circle receiver together. Only MMIO/DMA and USB are modelled. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define DT1_NATIVE_TEST
#include "../mods/usb-midi/usb_midi.c"
#define USB_TRACK_IO_OVERRIDES
#include "usb_track_queue_io.h"
#include "../mods/usb-track/usb_track.c"
#include "../mods/usb-track/worker.c"
#include "../control/circle_link_adapter.h"

uint32_t fw_io_selector,fw_io_mode,fw_io_sync;
volatile int32_t fw_io_halves[128];
volatile uint8_t fw_render_machine[8];
volatile int16_t fw_voice_params[53*8];
volatile int32_t fw_voice_note[8];
volatile int16_t fw_voice_vel[8];
volatile uint32_t fw_voice_start;
const char cm_ui_v32[1];
int32_t cm_src_value(int32_t t,int32_t k) { return fw_voice_params[53*t+k]; }
static struct sc_circle_link host;
static struct sc_frame installed;
static unsigned starts,frames,drop_acks,blocked,foreign;
static unsigned tick_period=50;
static uint8_t packets[256][DT1_MSG_MAX];
static uint32_t lengths[256],write_at,read_at;
static int32_t blocks[256];

static void advance_during_worker(void) { ++usb_track_clock; }
static void stock(const uint8_t *p,uint32_t n) { (void)p; foreign+=n; }
static void collect(const uint8_t *p,uint32_t n)
{
    assert(write_at-read_at<256); memcpy(packets[write_at%256],p,n);
    lengths[write_at%256]=n; ++write_at;
}
static int install(void *ctx,const struct sc_frame *f)
{
    assert(ctx==&host); installed=*f; ++frames; if(f->flags&SC_START) ++starts; return 1;
}
static void clear(void *ctx) { assert(ctx==&host); memset(&installed,0,sizeof installed); }
static int reply(void *ctx,const uint8_t *p,uint32_t n)
{
    assert(ctx==&host && !irq_depth);
    if(!drop_acks) ((void (*)(const uint8_t *,uint32_t))test_rx_slot)(p,n);
    return 1;
}
static void pump(void)
{
    while(read_at!=write_at) {
        uint32_t n=lengths[read_at%256],i;
        for(i=0;i<n;++i) sc_circle_midi_byte(&host,packets[read_at%256][i]);
        ++read_at;
        sc_circle_advance_block(&host,usb_track_clock);
        sc_circle_process(&host,128);
    }
}
static void step(unsigned n)
{
    unsigned j,i;
    for(j=0;j<n;++j) {
        fw_io_selector=usb_track_clock&1;
        for(i=0;i<128;++i) fw_io_halves[i]=1234567;
        for(i=32;i<256;++i) blocks[i]=7654321;
        usb_track_render(blocks);
        for(i=32;i<256;++i) assert(blocks[i]==7654321);
        if(!usb_track_gate && fw_render_machine[0]==121) { assert(blocks[0]==0); ++blocked; }
        if(usb_track_clock%tick_period==0) { usb_track_tick(0); pump(); }
    }
}
int main(void)
{
    unsigned i,was_starts; uint32_t generation;
    sc_circle_init(&host,install,reply,clear,&host,USB_TRACK_LEASE);
    test_ready=test_isr=1; test_rx_slot=0;
    test_auto_complete=1; test_tx_observer=collect;
    fw_io_mode=2; fw_io_sync=1; fw_render_machine[0]=121;
    fw_voice_note[0]=60<<16; fw_voice_vel[0]=100<<8;
    for(i=0;i<8;++i) fw_voice_params[i]=(i+1)<<8;
    step(1); assert(!usb_track_gate && blocked==1);
    usb_track_tick(0); assert(!usbmidi_device_link());
    test_rx_slot=(uintptr_t)stock;
    step(499);
    assert(usb_track_gate && sc_circle_audio_allowed(&host));
    assert(frames>0 && installed.params[3]==4<<8);
    generation=device.session; was_starts=starts;
    fw_voice_start=1; step(2); fw_voice_start=0; step(198);
    assert(starts==was_starts+2); /* adjacent starts are retained */
    for(i=0;i<1000;++i) { fw_voice_params[0]=(i%127)<<8; step(1); }
    assert(usb_track_gate && !usb_track_control_failed());
    /* Slow cadence from the prior failure: 300 render blocks between UI ticks.
     * Unchanged snapshots must not grow backlog over sustained service. */
    tick_period=300;
    generation=device.session;
    step(12000);
    assert(usb_track_gate && device.session==generation && !usb_track_control_failed());
    was_starts=starts;
    fw_voice_start=1; step(2); fw_voice_start=0; step(898);
    assert(starts==was_starts+2 && usb_track_gate);
    tick_period=50;
    /* Bad/missing replies cannot hold either gate open indefinitely. */
    drop_acks=1; step(1500); assert(!usb_track_gate);
    drop_acks=0; step(2000); assert(usb_track_gate && device.session!=generation);
    generation=device.session;
    fw_render_machine[0]=0; step(100);
    assert(!usb_track_gate && fw_io_halves[0]==0 && fw_io_halves[127]==0);
    fw_render_machine[0]=121; step(500); assert(usb_track_gate && device.session!=generation);
    /* Link loss discards sessions; firmware-ready returning is not activation. */
    test_ready=0; step(100); assert(!usb_track_gate);
    test_ready=1; step(1); assert(!usb_track_gate);
    step(499); assert(usb_track_gate);
    test_auto_complete=0; test_queued=0xffb-1; step(800);
    assert(!usb_track_gate); /* bounded congestion fails closed */
    test_queued=0; test_auto_complete=1; step(2000); assert(usb_track_gate);
    test_restore_observer=advance_during_worker; step(1000); assert(usb_track_gate);
    test_restore_observer=0;
    assert(!irq_depth);
    puts("production render + worker + native ABI bridge + Circle: activation, controls, adjacent starts, modulation, ACK loss, reconnect, congestion: PASS");
}
