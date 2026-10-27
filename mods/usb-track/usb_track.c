/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "digitakt-mk1/core3.h"
#include "src_queue.h"

/* OS 1.54 only. Sources: evidence/r07/handoff-facts.json and D01.
 * These USB globals are not exported by the pinned core SDK. Keep this
 * experimental adapter separate from device-neutral Sidecar host code. */
#ifndef USB_TRACK_IO_OVERRIDES
#define USB_SELECTOR (*(volatile uint32_t *)0x4199f134u)
#define USB_MODE     (*(volatile uint32_t *)0x406104f4u)
#define USB_SYNC     (*(volatile uint32_t *)0x4025c294u)
#define USB_HALVES   ((volatile int32_t *)0x80001f60u)
#endif
#define MACHINE_ID 121

static uint32_t armed;
static struct ut_session control;
static uint8_t control_machine;
static uint8_t control_have_machine;

static void disarm_control(void)
{
    armed = 0;
    control_have_machine = 0;
}

/* The handshake/transport worker owns session creation and draining. These
 * wrappers keep its shared control/link API separate from the render queue. */
int usb_track_control_open(uint32_t session)
{
    disarm_control();
    if (!ut_session_open(&control, session))
        return 0;
    return 1;
}

void usb_track_control_close(void)
{
    ut_session_close(&control);
    disarm_control();
}

void usb_track_control_fail(void)
{
    ut_session_fail(&control);
    disarm_control();
}

int usb_track_control_failed(void)
{
    return ut_session_failed(&control);
}

int usb_track_control_invalidated(void)
{
    return ut_session_invalidated(&control);
}

int usb_track_control_peek(struct ut_record *record)
{
    return ut_session_peek(&control, record);
}

int usb_track_control_pop(struct ut_record *record)
{
    return ut_session_pop(&control, record);
}

unsigned usb_track_control_drain(struct ut_fake_transport *transport, unsigned budget,
                                 int (*sink)(void *, const struct ut_record *),
                                 void *ctx)
{
    return ut_fake_transport_drain(&control, transport, budget, sink, ctx);
}

static int capture_control(int32_t track)
{
    int16_t params[8];
    uint32_t i;
    uint8_t machine;
    int result;
    if (track != 0)
        return 0;
    if (!ut_session_ready(&control))
        return ut_session_failed(&control);
    machine = fw_render_machine[0];
    if (control_have_machine && machine != control_machine) {
        ut_session_invalidate(&control);
        disarm_control();
        return 0;
    }
    if (machine != MACHINE_ID)
        return 0;
    if (!ut_session_room(&control))
        return ut_session_failed(&control);
    for (i = 0; i < 8; ++i)
        params[i] = (int16_t)cm_src_value(0, (int32_t)i);
    result = ut_session_capture(&control, machine, params,
                                fw_voice_note[0], fw_voice_vel[0],
                                (fw_voice_start & 1u) != 0);
    if (result == UT_CAPTURE_OK) {
        control_machine = machine;
        control_have_machine = 1;
    }
    return result == UT_CAPTURE_FAILED;
}

void usb_track_render(int32_t *blocks)
{
    uint32_t i, s = USB_SELECTOR;
    volatile int32_t *pair;
    if (capture_control(0)) {
        for (i = 0; i < 32; ++i)
            blocks[i] = 0;
        for (i = 0; i < 128; ++i)
            USB_HALVES[i] = 0;
        armed = 0;
        return;
    }
    if (fw_render_machine[0] != MACHINE_ID) {
        armed = 0;
        return;
    }
    /* Invalid/transition state is silence; consume both stale halves once
     * before arming. No calls or USB ownership changes from the renderer. */
    if (s > 1 || USB_MODE != 2 || USB_SYNC != 1 || !armed) {
        for (i = 0; i < 32; ++i)
            blocks[i] = 0;
        for (i = 0; i < 128; ++i)
            USB_HALVES[i] = 0;
        armed = (s <= 1 && USB_MODE == 2 && USB_SYNC == 1);
        return;
    }
    pair = USB_HALVES + 64*s;
    for (i = 0; i < 32; ++i) {
        blocks[i] = pair[2*i];
        pair[2*i] = 0;
        pair[2*i+1] = 0;
    }
}

static int32_t control_text(char *buf, int32_t value, int32_t ctx, int32_t machine)
{
    uint32_t n = value < 0 ? 0 : value > (127 << 8) ? 127 : (uint32_t)value >> 8;
    uint32_t pos = 0;
    (void)ctx; (void)machine;
    if (n >= 100) buf[pos++] = '0' + n / 100;
    if (n >= 10) buf[pos++] = '0' + (n / 10) % 10;
    buf[pos++] = '0' + n % 10;
    buf[pos] = 0;
    return 1;
}

/* Extended ABI applies defaults through native sound updates; legacy v3
 * only overrides the displayed range. No extra controls are shown. */
static const struct cm_src_page trx_page = {
    .title = "TRX-B2", .parameter = {0, 1, 2, 3, 4, 5, 6, 7}
};
#define UNUSED_EXTRA {.look = 0x86, .flags = CM_RANGE | CM_DEFAULT, \
                      .max = 127 << 8, .def = 64 << 8}
static const struct cm_ui32 usb_page = {
    .base = {.abi = cm_ui_v32, .page_from = 3, .group = "TRX-B2",
    .knob = {
        [0] = {.name = "PTCH", .lname = "PTCH",
               .look = 0x86, .fmt = control_text,
               .flags = CM_RANGE | CM_DEFAULT | CM_NOT_SAMPLE,
               .min = 0, .max = 127 << 8, .def = 60 << 8},
        [1] = {.name = "DEC", .lname = "DEC",
               .look = 0x86, .fmt = control_text,
               .flags = CM_RANGE | CM_DEFAULT | CM_NOT_SAMPLE,
               .min = 0, .max = 127 << 8, .def = 65 << 8},
        [2] = {.name = "RAMP", .lname = "RAMP",
               .look = 0x86, .fmt = control_text,
               .flags = CM_RANGE | CM_DEFAULT | CM_NOT_SAMPLE,
               .min = 0, .max = 127 << 8, .def = 41 << 8},
        [3] = {.name = "HOLD", .lname = "HOLD",
               .look = 0x86, .fmt = control_text,
               .flags = CM_RANGE | CM_DEFAULT | CM_NOT_SAMPLE,
               .min = 0, .max = 127 << 8, .def = 62 << 8},
        [4] = {.name = "TICK", .lname = "TICK",
               .look = 0x86, .fmt = control_text,
               .flags = CM_RANGE | CM_DEFAULT | CM_NOT_SAMPLE,
               .min = 0, .max = 127 << 8, .def = 91 << 8},
        [5] = {.name = "NOIS", .lname = "NOIS",
               .look = 0x86, .fmt = control_text,
               .flags = CM_RANGE | CM_DEFAULT | CM_NOT_SAMPLE,
               .min = 0, .max = 127 << 8, .def = 44 << 8},
        [6] = {.name = "DIRT", .lname = "DIRT",
               .look = 0x86, .fmt = control_text,
               .flags = CM_RANGE | CM_DEFAULT | CM_NOT_SAMPLE,
               .min = 0, .max = 127 << 8, .def = 56 << 8},
        [7] = {.name = "DIST", .lname = "DIST",
               .look = 0x86, .fmt = control_text,
               .flags = CM_RANGE | CM_DEFAULT | CM_NOT_SAMPLE,
               .min = 0, .max = 127 << 8, .def = 58 << 8},
    }},
    .extra = {UNUSED_EXTRA, UNUSED_EXTRA, UNUSED_EXTRA, UNUSED_EXTRA,
              UNUSED_EXTRA, UNUSED_EXTRA, UNUSED_EXTRA},
    .page_count = 1, .pages = &trx_page
};
const struct cm_machine usb_track_machine = {
    121, "USB1", "USB1", 0, 3, 121, CM_UI_TAG, &usb_page.base
};
