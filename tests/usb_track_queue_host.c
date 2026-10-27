/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Host execution of the compiled usb_track render hook. The firmware-global
 * addresses are remapped to host arrays by -DUSB_TRACK_IO_OVERRIDES; this
 * proves queue/silence behavior of the hook, not hardware timing or DMA. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "src_queue.h"

uint32_t fw_io_selector, fw_io_mode, fw_io_sync;
volatile int32_t fw_io_halves[128];
volatile uint8_t fw_render_machine[8];
volatile int16_t fw_voice_params[53 * 8];
volatile int32_t fw_voice_note[8];
volatile int16_t fw_voice_vel[8];
volatile uint32_t fw_voice_start;
int32_t fw_track_blocks[32 * 8];
const char cm_ui_v32[1];

/* Only the two SDK calls used by the hook are intercepted. */
int32_t cm_src_value(int32_t track, int32_t parameter)
{
    assert(track == 0 && parameter >= 0 && parameter < 8);
    return fw_voice_params[53 * track + parameter];
}

void usb_track_render(int32_t *blocks);
int usb_track_control_open(uint32_t session);
void usb_track_control_close(void);
int usb_track_control_failed(void);
int usb_track_control_peek(struct ut_record *record);
int usb_track_control_pop(struct ut_record *record);

static void tick(void)
{
    uint32_t i;
    fw_io_selector = 0;
    fw_io_mode = 2;
    fw_io_sync = 1;
    for (i = 0; i < 128; ++i)
        fw_io_halves[i] = 0;
    usb_track_render(fw_track_blocks);
}

int main(void)
{
    struct ut_record r;
    uint32_t i;
    fw_io_selector = 0;
    fw_io_mode = 2;
    fw_io_sync = 1;
    fw_render_machine[0] = 121;
    fw_voice_note[0] = 60 << 16;
    fw_voice_vel[0] = 100 << 8;
    for (i = 0; i < 8; ++i)
        fw_voice_params[i] = (int16_t)((i + 1) << 8);

    /* No session: audio routing is unchanged (and remains silent until armed). */
    tick();
    assert(!usb_track_control_failed());

    assert(usb_track_control_open(9001));
    for (i = 0; i < 4; ++i) {
        fw_voice_start = (i == 1 || i == 2) ? 1u : 0u;
        fw_voice_note[0] = (int32_t)(60 + i) << 16;
        fw_voice_vel[0] = (int16_t)((90 + i) << 8);
        fw_voice_params[0] = (int16_t)((10 + i) << 8);
        tick();
    }
    assert(usb_track_control_pop(&r));
    assert(r.session == 9001 && r.sequence == 1 && r.block == 2);
    assert(r.note == 60 << 16 && r.velocity == 90 << 8 && r.flags == 0);
    assert(r.params[0] == 10 << 8);
    assert(usb_track_control_pop(&r));
    assert(r.sequence == 2 && r.flags == UT_START);
    assert(usb_track_control_pop(&r));
    assert(r.sequence == 3 && r.flags == UT_START);
    assert(usb_track_control_pop(&r));
    assert(r.sequence == 4 && r.flags == 0);
    assert(!usb_track_control_pop(&r));

    /* Machine change invalidates without muting the next machine. */
    fw_render_machine[0] = 120;
    tick();
    assert(!usb_track_control_failed());
    assert(!usb_track_control_peek(&r));
    usb_track_control_close();
    assert(usb_track_control_open(9002));
    fw_render_machine[0] = 121;
    tick();
    assert(usb_track_control_pop(&r) && r.session == 9002 && r.sequence == 1);

    /* Fill the bounded ring exactly. Note: capture_control reads A-H before
     * checking queue capacity, so the hook already captured the final values;
     * crossing the bound on the next block must zero current and stale PCM. */
    for (i = 0; i < UT_QUEUE_CAPACITY; ++i)
        tick();
    assert(!usb_track_control_failed());
    tick();
    assert(usb_track_control_failed());
    assert(fw_track_blocks[0] == 0);
    for (i = 0; i < 128; ++i)
        assert(fw_io_halves[i] == 0);
    puts("compiled render hook: session, starts, machine change, overflow silence: PASS");
    return 0;
}
