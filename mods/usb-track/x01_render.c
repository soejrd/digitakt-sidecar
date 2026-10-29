/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Opt-in experimental replacement. The normal mod does not compile this. */
#include "digitakt-mk1/core3.h"
#include "x01_track.h"
#include "return_lanes.h"
#ifndef USB_TRACK_IO_OVERRIDES
#define USB_SELECTOR (*(volatile uint32_t *)0x4199f134u)
#define USB_MODE (*(volatile uint32_t *)0x406104f4u)
#define USB_SYNC (*(volatile uint32_t *)0x4025c294u)
#define USB_HALVES ((volatile int32_t *)0x80001f60u)
#endif
#define x01_barrier() __asm__ volatile ("" ::: "memory")
volatile uint32_t usb_track_clock,usb_track_route,usb_track_gate,usb_track_ack_clock;
volatile uint32_t x01_route_lanes=0x7f7fu;
static struct ut_session x01_queues[2];
static struct ut_record x01_previous[2];
static uint8_t x01_lanes[2],x01_have[2];
static volatile uint8_t x01_capturing;
static void (*volatile x01_wakeup)(void);
void usb_track_control_set_wakeup(void (*fn)(void)) { x01_wakeup=fn; }
int usb_track_control_has_wakeup(void) { return x01_wakeup!=0; }
void usb_track_control_wake(void) { if (x01_wakeup) x01_wakeup(); }
void x01_capture_close(void)
{
    x01_capturing=0; usb_track_gate=0; x01_barrier();
    for (unsigned i=0;i<2;++i) ut_session_close(&x01_queues[i]);
}
void usb_track_control_fail(void)
{
    usb_track_gate=0;
    for (unsigned i=0;i<2;++i) ut_session_fail(&x01_queues[i]);
}
int usb_track_control_failed(void)
{ return ut_session_failed(&x01_queues[0]) || ut_session_failed(&x01_queues[1]); }
int x01_capture_open(uint32_t session,const uint8_t lanes[2])
{
    x01_capture_close();
    if (!sc_track_mask(lanes)) return 0;
    for (unsigned i=0;i<2;++i) {
        x01_lanes[i]=lanes[i]; x01_have[i]=0;
        if (!ut_session_open(&x01_queues[i],session)) return 0;
    }
    x01_barrier(); x01_capturing=1; return 1;
}
int x01_capture_invalid(void)
{
    if (!x01_capturing || usb_track_control_failed()) return 1;
    return x01_route_lanes!=((uint32_t)x01_lanes[0]|((uint32_t)x01_lanes[1]<<8));
}
int x01_capture_peek(unsigned *lane,struct ut_record *record)
{
    struct ut_record left,right;
    int a=ut_session_peek(&x01_queues[0],&left),b=ut_session_peek(&x01_queues[1],&right);
    if (!a && !b) return 0;
    *lane=(!a || (b && left.block-right.block<0x80000000u && left.block!=right.block)) ? 1u : 0u;
    *record=*lane ? right : left; return 1;
}
int x01_capture_pop(unsigned lane,struct ut_record *record)
{ return lane<2 && ut_session_pop(&x01_queues[lane],record); }
void usb_track_render(int32_t *blocks)
{
    uint8_t lanes[2]={SC_TRACK_UNUSED,SC_TRACK_UNUSED};
    unsigned selected=0,mask=0;
    const uint32_t previous_lanes=x01_route_lanes,previous_route=usb_track_route;
    ++usb_track_clock;
    for (unsigned t=0;t<8;++t) if (fw_render_machine[t]==121) {
        if (selected<2) lanes[selected]=(uint8_t)t;
        ++selected; mask|=1u<<t;
    }
    x01_route_lanes=(uint32_t)lanes[0]|((uint32_t)lanes[1]<<8);
    usb_track_route=selected>0 && selected<=2 && USB_MODE==6 && USB_SYNC==1 && USB_SELECTOR<=1;
    const unsigned valid=x01_capturing && !x01_capture_invalid();
    if (!valid || !usb_track_route || usb_track_clock-usb_track_ack_clock>=USB_TRACK_LEASE)
        usb_track_gate=0;
    if (valid && usb_track_route) for (unsigned lane=0;lane<2;++lane) {
        unsigned t=x01_lanes[lane]; if (t>=8) continue;
        struct ut_record now={0};
        now.note=fw_voice_note[t]; now.velocity=fw_voice_vel[t];
        unsigned changed=!x01_have[lane] || (fw_voice_start&(1u<<t)) ||
            now.note!=x01_previous[lane].note || now.velocity!=x01_previous[lane].velocity;
        for (unsigned k=0;k<8;++k) {
            int value=(int16_t)cm_src_value((int32_t)t,(int32_t)k)/256;
            if (value<0) value=0;
            if (value>127) value=127;
            now.params[k]=(int16_t)(value*256);
            changed|=now.params[k]!=x01_previous[lane].params[k];
        }
        if (!changed && usb_track_clock-x01_previous[lane].block<64u) continue;
        x01_queues[lane].block=usb_track_clock;
        if (ut_session_capture(&x01_queues[lane],121,now.params,now.note,now.velocity,
                               !x01_have[lane] || (fw_voice_start&(1u<<t)))!=UT_CAPTURE_OK) {
            usb_track_control_fail(); break;
        }
        now.block=usb_track_clock; x01_previous[lane]=now; x01_have[lane]=1;
        usb_track_control_wake();
    }
    int8_t map[2]={(int8_t)(lanes[0]<8 ? lanes[0] : -1),
                   (int8_t)(lanes[1]<8 ? lanes[1] : -1)};
    if (selected<=2)
        sc_return_lanes(blocks,USB_HALVES,USB_SELECTOR,usb_track_gate && valid,map);
    else {
        for (unsigned i=0;i<128;++i) USB_HALVES[i]=0;
        for (unsigned t=0;t<8;++t) if (mask&(1u<<t))
            for (unsigned i=0;i<32;++i) blocks[t*32+i]=0;
    }
    /* A retired/empty route has no capture traffic to wake the worker. Post
     * once on a route edge so recovery does not depend on UI fallback ticks.
     * This is the existing bounded semaphore signal; no USB runs in render. */
    if (previous_lanes!=x01_route_lanes || previous_route!=usb_track_route)
        usb_track_control_wake();
}
static int32_t x01_control_text(char *buf,int32_t value,int32_t ctx,int32_t machine)
{
    uint32_t n=value<0 ? 0 : value>(127<<8) ? 127 : (uint32_t)value>>8,pos=0;
    (void)ctx; (void)machine;
    if (n>=100) buf[pos++]='0'+n/100;
    if (n>=10) buf[pos++]='0'+(n/10)%10;
    buf[pos++]='0'+n%10; buf[pos]=0; return 1;
}
static const struct cm_src_page x01_pages[]={
    {.title="TRX-B2",.parameter={0,1,2,3,4,5,6,7}}
};
#define X01_KNOB(label, initial) {.name=label,.lname=label,.look=0x86,.fmt=x01_control_text, \
    .flags=CM_RANGE|CM_DEFAULT|CM_NOT_SAMPLE,.min=0,.max=127<<8,.def=(initial)<<8}
#define X01_UNUSED {.look=0x86,.flags=CM_RANGE|CM_DEFAULT,.max=127<<8,.def=64<<8}
static const struct cm_ui32 x01_ui={
    .base={.abi=cm_ui_v32,.page_from=3,.group="TRX-B2",.knob={
        X01_KNOB("PTCH",60),X01_KNOB("DEC",65),X01_KNOB("RAMP",41),X01_KNOB("HOLD",62),
        X01_KNOB("TICK",91),X01_KNOB("NOIS",44),X01_KNOB("DIRT",56),X01_KNOB("DIST",58)}},
    .extra={X01_UNUSED,X01_UNUSED,X01_UNUSED,X01_UNUSED,X01_UNUSED,X01_UNUSED,X01_UNUSED},
    .page_count=1,.pages=x01_pages
};
const struct cm_machine usb_track_machine={121,"USB1","USB1",0,3,121,CM_UI_TAG,&x01_ui.base};
