/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Opt-in actual render/worker + existing native ABI adapter + Pi task peer.
 * MMIO, USB completion and host scheduling are modelled; engines are real. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define DT1_NATIVE_TEST
#include "../mods/usb-midi/usb_midi.c"
#define USB_TRACK_IO_OVERRIDES
#include "usb_track_queue_io.h"
#include "../mods/usb-track/x01_render.c"
#include "../mods/usb-track/x01_worker.c"
#include "track_host.h"
uint32_t fw_io_selector,fw_io_mode,fw_io_sync;
volatile int32_t fw_io_halves[128];
volatile uint8_t fw_render_machine[8];
volatile int16_t fw_voice_params[53*8];
volatile int32_t fw_voice_note[8];
volatile int16_t fw_voice_vel[8];
volatile uint32_t fw_voice_start;
const char cm_ui_v32[1];
int32_t cm_src_value(int32_t t,int32_t k) { assert(t>=0 && t<8 && k>=0 && k<8); return fw_voice_params[53*t+k]; }
static struct sc_track_host peer;
static unsigned drop_acks,refuse_acks,worker_pending,wakes,frames_seen,foreign_bytes;
static uint8_t packets[512][DT1_MSG_MAX];
static unsigned sizes[512],written,read_at;
static int16_t previous_pcm[32][8];
static int32_t track_blocks[256];
static uint64_t heard[8];
static unsigned worker_passes;
static void wake(void) { worker_pending=1; ++wakes; }
static void stock(const uint8_t *p,uint32_t n) { (void)p; foreign_bytes+=n; }
static void collect(const uint8_t *p,uint32_t n)
{
    assert(written-read_at<512 && n<=DT1_MSG_MAX);
    memcpy(packets[written%512],p,n); sizes[written++%512]=n;
}
static int ack(void *ctx,const uint8_t *p,unsigned n)
{
    (void)ctx; assert(!irq_depth);
    if (refuse_acks) return 0;
    if (!drop_acks) ((void (*)(const uint8_t *,uint32_t))test_rx_slot)(p,n);
    return 1;
}
static void process_packets(void)
{
    while (read_at<written) {
        unsigned n=sizes[read_at%512];
        const uint8_t *bytes=packets[read_at++%512];
        if (n==SC_TRACK_WIRE_SIZE) ++frames_seen;
        (void)sc_track_host_receive(&peer,bytes,n,usb_track_clock);
    }
}
static void step(unsigned count)
{
    for (unsigned iter=0;iter<count;++iter) {
        fw_io_selector=usb_track_clock&1u;
        for (unsigned i=0;i<128;++i) fw_io_halves[i]=0;
        for (unsigned i=0;i<32;++i) for (unsigned lane=0;lane<2;++lane)
            fw_io_halves[fw_io_selector*64u+2u*i+lane]=(int32_t)previous_pcm[i][lane]*65536;
        for (unsigned i=0;i<256;++i) track_blocks[i]=7654321;
        usb_track_render(track_blocks);
        for (unsigned t=0;t<8;++t) for (unsigned i=0;i<32;++i) {
            if (fw_render_machine[t]!=121) assert(track_blocks[t*32+i]==7654321);
            else {
                if (!usb_track_gate) assert(!track_blocks[t*32+i]);
                if (track_blocks[t*32+i]) ++heard[t];
            }
        }
        if (usb_track_clock%50u==0) worker_pending=1;
        unsigned budget=0;
        while (worker_pending) {
            assert(++budget<20); worker_pending=0; ++worker_passes;
            usb_track_tick(0); process_packets();
        }
        sc_track_host_tick(&peer,usb_track_clock);
        (void)sc_track_host_render(&peer,previous_pcm,32);
        assert(!irq_depth);
    }
}
static void starts(void)
{
    const uint64_t a=peer.bank.voice[0].consumed_starts,b=peer.bank.voice[3].consumed_starts;
    fw_voice_start=(1u<<0)|(1u<<3); step(1); fw_voice_start=0; step(15);
    if (peer.bank.voice[0].consumed_starts!=a+1 || peer.bank.voice[3].consumed_starts!=b+1)
        fprintf(stderr,"START mismatch clock=%u session=%u gate=%u consumed=%llu/%llu prior=%llu/%llu host_reason=%u bank_fault=%u pending=%u/%u\n",
                usb_track_clock,x01_device.session,(unsigned)usb_track_gate,
                (unsigned long long)peer.bank.voice[0].consumed_starts,
                (unsigned long long)peer.bank.voice[3].consumed_starts,
                (unsigned long long)a,(unsigned long long)b,peer.reason,peer.bank.failure.code,
                x01_device.count,worker_pending);
    assert(peer.bank.voice[0].consumed_starts==a+1);
    assert(peer.bank.voice[3].consumed_starts==b+1);
}
int main(int argc,char **argv)
{
    sc_track_host_init(&peer,ack,0,128,USB_TRACK_LEASE);
    test_ready=test_isr=test_auto_complete=1; test_rx_slot=(uintptr_t)stock;
    test_tx_observer=collect;
    fw_io_mode=6; fw_io_sync=1; fw_render_machine[0]=fw_render_machine[3]=121;
    const int defaults[]={60,65,41,62,91,44,56,58};
    for (unsigned t=0;t<8;++t) {
        fw_voice_note[t]=60<<16; fw_voice_vel[t]=100<<8;
        for (unsigned k=0;k<8;++k) fw_voice_params[53*t+k]=(defaults[k]+(int)t)*256;
    }
    usb_track_control_set_wakeup(wake); usbmidi_device_set_wakeup(usb_track_worker_rx_wake);
    step(200);
    assert(usb_track_gate && sc_track_host_allowed(&peer) && sc_track_dev_allowed(&x01_device));
    assert(x01_device.lanes[0]==0 && x01_device.lanes[1]==3 && peer.active_mask==9);
    assert(heard[0] && heard[3]);
    uint32_t session=x01_device.session;
    for (unsigned i=0;i<100;++i) starts();
    for (unsigned i=0;i<1000;++i) {
        fw_voice_params[0]=(i%127)*256;
        fw_voice_params[53*3+1]=((i+31)%127)*256;
        step(1);
        assert(usb_track_gate && x01_device.session==session && !peer.bank.failure.code);
    }
    step(100);
    assert(peer.bank.voice[0].starts==101 && peer.bank.voice[3].starts==101);
    /* ACKs may be lost; a later cumulative ACK retires the exact earlier tuple. */
    drop_acks=1; starts(); step(100); drop_acks=0; step(150);
    assert(usb_track_gate && x01_device.session==session && !x01_device.failed);
    /* A retained frame replay is byte-identical and must not reinstall START. */
    drop_acks=1; starts();
    uint64_t applied=peer.bank.applied,consumed=peer.bank.voice[0].consumed_starts;
    step(130);
    assert(peer.bank.voice[0].consumed_starts==consumed);
    assert(peer.bank.applied>=applied); /* periodic snapshots may still arrive */
    drop_acks=0; step(150);
    assert(usb_track_gate && x01_device.session==session);
    /* Lost ACKs cannot keep either side active indefinitely. */
    drop_acks=1; step(1000); assert(!usb_track_gate);
    drop_acks=0; step(300); assert(usb_track_gate && x01_device.session!=session);
    session=x01_device.session;
    fw_render_machine[3]=0; step(1); assert(!usb_track_gate);
    step(150); assert(usb_track_gate && x01_device.session!=session && x01_device.mask==1);
    fw_render_machine[5]=121; step(150);
    assert(usb_track_gate && x01_device.mask==33);
    fw_render_machine[7]=121; step(100); assert(!usb_track_gate && !usb_track_route);
    fw_render_machine[7]=0; step(200); assert(usb_track_gate);
    session=x01_device.session;
    test_ready=0; sc_track_host_stop(&peer,SC_REASON_TRANSPORT);
    step(1000); assert(!usb_track_gate && !sc_track_host_allowed(&peer));
    test_ready=1; step(200); assert(usb_track_gate && x01_device.session!=session);
    /* Native sender refusal is bounded, even though its adapter defers 0. */
    test_auto_complete=0; test_queued=0xffb-1; step(1000);
    assert(!usb_track_gate);
    test_queued=0; test_auto_complete=1; step(300); assert(usb_track_gate);
    refuse_acks=1; fw_voice_start=1; step(1); fw_voice_start=0;
    step(1000); assert(!usb_track_gate && !sc_track_host_allowed(&peer));
    refuse_acks=0; step(1000); assert(usb_track_gate);
    printf("X01 compiled native transport: simultaneous starts, per-track controls, lost/duplicate ACKs, map changes, excess voices, unplug, stalled TX, refused replies: PASS (%u frames, %u worker passes, %u wakes)\n",frames_seen,worker_passes,wakes);
    if (argc>1) {
        FILE *out=fopen(argv[1],"w"); assert(out);
        fprintf(out,"{\n  \"scope\": \"actual opt-in render/worker and existing native ABI adapter with MMIO/USB completion model, portable Pi peer, real engines; no hardware\",\n"
                    "  \"frames\": %u, \"worker_passes\": %u, \"wakes\": %u,\n"
                    "  \"checks\": [\"100 simultaneous native START pairs\", \"1000 per-track SRC changes\", \"lost/duplicate ACKs\", \"handshake retry\", \"stable map/new session on selection changes\", \"third external track rejected\", \"disconnect/reconnect\", \"bounded stalled TX\", \"refused host replies\"],\n"
                    "  \"pass\": true\n}\n",frames_seen,worker_passes,wakes);
        assert(!fclose(out));
    }
}
