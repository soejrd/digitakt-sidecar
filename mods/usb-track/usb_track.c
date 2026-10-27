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

static const struct cm_ui usb_page = {
    .abi = cm_ui_v3, .page_from = 3,
    .knob = {
        [0] = {.flags = CM_HIDDEN}, [1] = {.flags = CM_HIDDEN},
        [2] = {.flags = CM_HIDDEN}, [3] = {.flags = CM_HIDDEN | CM_NOT_SAMPLE},
        [4] = {.flags = CM_HIDDEN}, [5] = {.flags = CM_HIDDEN},
        [6] = {.flags = CM_HIDDEN}, [7] = {.flags = CM_HIDDEN},
    }
};
const struct cm_machine usb_track_machine = {
    121, "USB1", "USB1", 0, 3, 121, CM_UI_TAG, &usb_page
};
