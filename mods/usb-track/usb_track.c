/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "digitakt-mk1/core3.h"

/* OS 1.54 only. Sources: evidence/r07/handoff-facts.json and D01.
 * These USB globals are not exported by the pinned core SDK. Keep this
 * experimental adapter separate from device-neutral Sidecar host code. */
#define USB_SELECTOR (*(volatile uint32_t *)0x4199f134u)
#define USB_MODE     (*(volatile uint32_t *)0x406104f4u)
#define USB_SYNC     (*(volatile uint32_t *)0x4025c294u)
#define USB_HALVES   ((volatile int32_t *)0x80001f60u)
#define MACHINE_ID 121

static uint32_t armed;

void usb_track_render(int32_t *blocks)
{
    uint32_t i, s = USB_SELECTOR;
    volatile int32_t *pair;
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
