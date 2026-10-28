/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "digitakt-mk1/core3.h"
#include "usb_track.h"
#ifdef USB_TRACK_DIAGNOSTIC
#include "../usb-midi/usb_midi.h"
#endif

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
/* OS 1.54 mode table: row 6 matches physical PID 0x0b2c / EP3 368/88.
 * Row 2 is MIDI-only PID 0x000d. See evidence/h02/usb-mode-154.json. */
#define USB_NATIVE_OVERBRIDGE_MODE 6u

volatile uint32_t usb_track_clock, usb_track_route, usb_track_gate, usb_track_ack_clock;
static uint32_t armed;
static void (*volatile control_wakeup)(void);
#ifdef USB_TRACK_DIAGNOSTIC
volatile uint32_t usb_track_diag_ui_ticks, usb_track_diag_render_wakes;
#endif
void usb_track_control_set_wakeup(void (*fn)(void)) { control_wakeup=fn; }
int usb_track_control_has_wakeup(void) { return control_wakeup!=0; }
void usb_track_control_wake(void) { if (control_wakeup) control_wakeup(); }
static struct ut_session control;
static uint8_t control_machine;
static uint8_t control_have_machine;
/* Producer-owned comparison copy: never rewrite a published queue record. */
static int16_t captured_params[8], captured_velocity;
static int32_t captured_note;
static uint32_t captured_at;

static void disarm_control(void)
{
    armed = 0;
    usb_track_gate = 0;
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
    int changed;
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
    changed = !control_have_machine || (fw_voice_start & 1u) ||
              captured_note != fw_voice_note[0] || captured_velocity != fw_voice_vel[0];
    for (i = 0; i < 8; ++i) {
        /* TRX-B2's public API takes integer 0..127 controls. Normalize before
         * comparison/queueing exactly as the host does; fractional smoothing
         * changes cannot change engine output and must not flood the worker. */
        int32_t value = (int16_t)cm_src_value(0, (int32_t)i) / 256;
        if (value < 0) value = 0;
        if (value > 127) value = 127;
        params[i] = (int16_t)(value * 256);
        changed |= params[i] != captured_params[i];
    }
    /* Suppress only identical non-START snapshots; retain a periodic sample
     * so the existing block lease remains live. Every effective integer change and adjacent
     * START keeps its original block and FIFO position. */
    if (!changed && usb_track_clock - captured_at < 64u)
        return 0;
    if (!ut_session_room(&control))
        return ut_session_failed(&control);
    control.block = usb_track_clock; /* absolute device block, including reset gaps */
    result = ut_session_capture(&control, machine, params,
                                fw_voice_note[0], fw_voice_vel[0],
                                (fw_voice_start & 1u) != 0);
    if (result == UT_CAPTURE_OK) {
        control_machine = machine;
        for (i = 0; i < 8; ++i) captured_params[i] = params[i];
        captured_note = fw_voice_note[0];
        captured_velocity = fw_voice_vel[0];
        captured_at = usb_track_clock;
        control_have_machine = 1;
    }
    if (result == UT_CAPTURE_OK || result == UT_CAPTURE_FAILED) usb_track_control_wake();
    return result == UT_CAPTURE_FAILED;
}

void usb_track_render(int32_t *blocks)
{
    uint32_t i, s = USB_SELECTOR;
    volatile int32_t *pair;
    ++usb_track_clock;
#ifdef USB_TRACK_DIAGNOSTIC
    ++usb_track_diag_render_wakes;
#endif
    usb_track_route = fw_render_machine[0] == MACHINE_ID &&
                      s <= 1 && USB_MODE == USB_NATIVE_OVERBRIDGE_MODE && USB_SYNC == 1;
    if (!usb_track_route || usb_track_clock - usb_track_ack_clock >= USB_TRACK_LEASE)
        usb_track_gate = 0;
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
        /* This one-voice prototype reserves both USB return channels. Never
         * let a late sidecar block escape through dry MAIN after deselection. */
        for (i = 0; i < 128; ++i) USB_HALVES[i] = 0;
        return;
    }
    /* Invalid/transition state is silence; consume both stale halves once
     * before arming. No calls or USB ownership changes from the renderer. */
    if (!usb_track_gate || usb_track_clock - usb_track_ack_clock >= USB_TRACK_LEASE ||
        s > 1 || USB_MODE != USB_NATIVE_OVERBRIDGE_MODE || USB_SYNC != 1 || !armed) {
        for (i = 0; i < 32; ++i)
            blocks[i] = 0;
        for (i = 0; i < 128; ++i)
            USB_HALVES[i] = 0;
        armed = (usb_track_gate && usb_track_clock - usb_track_ack_clock < USB_TRACK_LEASE &&
                 s <= 1 && USB_MODE == USB_NATIVE_OVERBRIDGE_MODE && USB_SYNC == 1);
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
static const struct cm_src_page trx_pages[] = {
    {.title = "TRX-B2", .parameter = {0, 1, 2, 3, 4, 5, 6, 7}},
#ifdef USB_TRACK_DIAGNOSTIC
    {.title = "V01 DIAG", .parameter = {8, 9, 10, 11, 12, 13, 14, 0}},
#endif
};
#ifdef USB_TRACK_DIAGNOSTIC
static int32_t diag_text(char *buf, uint32_t n)
{
    uint32_t divisor = 1000, pos = 0;
    n %= 10000;
    while (divisor > 1 && n < divisor) divisor /= 10;
    do { buf[pos++] = '0' + n / divisor; n %= divisor; divisor /= 10; } while (divisor);
    buf[pos] = 0;
    return 1;
}
static uint32_t diag_ms(uint32_t blocks)
{
    uint32_t ms=(blocks*1000u+15u)/32u;
    return ms>9999u ? 9999u : ms; /* UI text is capped; do not wrap a long stall */
}
#define DIAG_FORMAT(name, expression) \
static int32_t name(char *b, int32_t v, int32_t c, int32_t m) \
{ (void)v; (void)c; (void)m; return diag_text(b, (expression)); }
DIAG_FORMAT(diag_clock, usb_track_clock)
DIAG_FORMAT(diag_ui, usb_track_diag_ui_ticks)
DIAG_FORMAT(diag_worker, usb_track_diag_worker_entries)
DIAG_FORMAT(diag_wgap, diag_ms(usb_track_diag_worker_gap_max))
DIAG_FORMAT(diag_batch, diag_ms(usb_track_diag_batch_max))
DIAG_FORMAT(diag_ugap, usb_track_diag_ui_gap_max)
DIAG_FORMAT(diag_fault, usb_track_worker_guard_faults)
#define DIAG_KNOB(label, formatter) {.name = label, .lname = label, .look = 0x86, \
    .flags = CM_RANGE | CM_DEFAULT | CM_NOT_SAMPLE, .max = 127 << 8, .fmt = formatter}
#endif
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
#ifdef USB_TRACK_DIAGNOSTIC
    .extra = {DIAG_KNOB("RCLK", diag_clock), DIAG_KNOB("UI", diag_ui),
              DIAG_KNOB("WORK", diag_worker), DIAG_KNOB("WGAP", diag_wgap),
              DIAG_KNOB("BATCH", diag_batch), DIAG_KNOB("UGAP", diag_ugap),
              DIAG_KNOB("FAULT", diag_fault)},
    .page_count = 2, .pages = trx_pages
#else
    .extra = {UNUSED_EXTRA, UNUSED_EXTRA, UNUSED_EXTRA, UNUSED_EXTRA,
              UNUSED_EXTRA, UNUSED_EXTRA, UNUSED_EXTRA},
    .page_count = 1, .pages = trx_pages
#endif
};
const struct cm_machine usb_track_machine = {
    121, "USB1", "USB1", 0, 3, 121, CM_UI_TAG, &usb_page.base
};
