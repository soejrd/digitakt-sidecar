/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "usb_track.h"
#include "core_candidate_rtos.h"
#include "../usb-midi/usb_midi.h"
#define GUARD 0x574f524bu
static struct core_rtos_task worker_task __attribute__((aligned(16)));
static struct core_rtos_sem worker_sem __attribute__((aligned(16)));
static struct {
    uint32_t low[8],words[2048],high[8];
} worker_stack __attribute__((aligned(16)));
static uint8_t started;
volatile uint32_t usb_track_worker_runs,usb_track_worker_guard_faults;
static void wake(void)
{
    /* Only this bounded native post may run in render/RX context. All USB and
     * protocol servicing stays in entry(), never in either producer. */
    fw_rtos_sem_signal(&worker_sem);
}
static void entry(void)
{
    for (;;) {
        fw_rtos_sem_wait(&worker_sem); /* blocking is confined to this task */
        for (unsigned i=0;i<8;++i) {
            if (worker_stack.low[i]!=GUARD || worker_stack.high[i]!=GUARD) {
                ++usb_track_worker_guard_faults;
                usb_track_control_fail();
                break;
            }
        }
        if (usb_track_worker_guard_faults) continue;
        usb_track_tick(0);
        ++usb_track_worker_runs;
    }
}
void usb_track_ui_tick(void *ctrl)
{
    (void)ctrl;
    if (!started) {
        for (unsigned i=0;i<8;++i) worker_stack.low[i]=worker_stack.high[i]=GUARD;
        for (unsigned i=0;i<2048;++i) worker_stack.words[i]=0xa5a5a5a5u;
        fw_rtos_sem_init(&worker_sem,0);
        /* Priority14 below render IRQs; service blocks again after each batch.
         * Stack/TCB belong to linker BSS; no allocation or OS timer changes. */
        if (!fw_rtos_task_create(&worker_task,entry,14,worker_stack.words,sizeof worker_stack.words)) {
            ++usb_track_worker_guard_faults; return;
        }
        usb_track_control_set_wakeup(wake);
        usbmidi_device_set_wakeup(usb_track_worker_rx_wake);
        started=1;
        fw_rtos_task_start(&worker_task);
    }
    wake(); /* low-rate link-loss/reconnect fallback only */
}
