/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "usb_track.h"
#include "core_candidate_rtos.h"
#include "../usb-midi/usb_midi.h"
#ifdef V01_REMOTE_DIAGNOSTIC
#include "core_candidate_clock.h"
#include "../../control/v01_diag.h"
extern volatile uint32_t v01_send_calls,v01_send_ticks,v01_send_max;
static uint32_t v01_ui_last,v01_batch_started,v01_last_report,v01_sequence;
static uint8_t v01_ui_seen;
static uint32_t v01_ui_max,v01_batch_max;
static void diag_emit(void)
{
    uint32_t v[V01_FIELDS]; uint8_t wire[V01_WIRE];
    /* Best effort, at most once/render-second, only after normal TX drains.
     * No telemetry backlog, reply, session change or safety-fault reset. */
    if(usb_track_clock-v01_last_report<1500u || !usbmidi_device_link() ||
       usbmidi_device_failed() || usbmidi_device_diag(4)!=0) return;
    v01_last_report=usb_track_clock;
    v[V01_SEQ]=++v01_sequence; v[V01_BLOCK]=usb_track_clock;
    v[V01_TIMER]=v01_clock(); v[V01_TIMER_MODE]=v01_clock_mode();
    v[V01_UI]=usb_track_diag_ui_ticks; v[V01_WORK]=usb_track_diag_worker_entries;
    v[V01_UGAP]=usb_track_diag_ui_gap_max;
    v[V01_WGAP]=usb_track_diag_ms(usb_track_diag_worker_gap_max);
    v[V01_BATCH]=usb_track_diag_ms(usb_track_diag_batch_max);
    v[V01_SENDS]=v01_send_calls; v[V01_SEND_TICKS]=v01_send_ticks;
    v[V01_SEND_MAX]=v01_send_max; v[V01_UI_TICKS_MAX]=v01_ui_max;
    v[V01_BATCH_TICKS_MAX]=v01_batch_max; v[V01_GUARDS]=usb_track_worker_guard_faults;
    v01_encode(wire,v);
    (void)usbmidi_device_send(wire,sizeof wire);
}
#endif
#define GUARD 0x574f524bu
static struct core_rtos_task worker_task __attribute__((aligned(16)));
static struct core_rtos_sem worker_sem __attribute__((aligned(16)));
static struct {
    uint32_t low[8],words[2048],high[8];
} worker_stack __attribute__((aligned(16)));
static uint8_t started;
volatile uint32_t usb_track_worker_runs,usb_track_worker_guard_faults;
#ifdef USB_TRACK_DIAGNOSTIC
volatile uint32_t usb_track_diag_worker_entries,usb_track_diag_worker_gap_max;
volatile uint32_t usb_track_diag_batch_max,usb_track_diag_ui_gap_max;
static uint32_t diag_worker_entered,diag_ui_last;
volatile uint32_t usb_track_diag_worker_window;
static uint32_t diag_batch_window;
static uint8_t diag_worker_valid,diag_ui_valid;
volatile uint32_t usb_track_diag_window,usb_track_diag_started_at;
void usb_track_diag_reset(void)
{
    /* UI owns UI counters. The higher-priority worker owns its counters and
     * acknowledges the new window at its next entry; no shared bulk clear. */
#ifdef V01_REMOTE_DIAGNOSTIC
    v01_ui_max=0; v01_ui_last=v01_clock(); v01_ui_seen=1;
#endif
    usb_track_diag_started_at=usb_track_clock;
    usb_track_diag_ui_ticks=usb_track_diag_ui_gap_max=0;
    diag_ui_last=usb_track_clock;
    diag_ui_valid=1;
    __asm__ volatile ("" ::: "memory");
    ++usb_track_diag_window;
}
static void diag_stamp_entry(void)
{
    uint32_t now=usb_track_clock;
    if (usb_track_diag_worker_window!=usb_track_diag_window) {
        usb_track_diag_worker_window=usb_track_diag_window;
        usb_track_diag_worker_entries=usb_track_diag_worker_gap_max=0;
        usb_track_diag_batch_max=0;
        diag_worker_valid=0;
#ifdef V01_REMOTE_DIAGNOSTIC
        v01_batch_max=0; v01_send_max=0;
#endif
    }
    diag_batch_window=usb_track_diag_worker_window;
    ++usb_track_diag_worker_entries;
    if (diag_worker_valid && now-diag_worker_entered>usb_track_diag_worker_gap_max)
        usb_track_diag_worker_gap_max=now-diag_worker_entered;
    diag_worker_entered=now;
    diag_worker_valid=1;
#ifdef V01_REMOTE_DIAGNOSTIC
    v01_batch_started=v01_clock();
#endif
}
static void diag_stamp_exit(void)
{
    uint32_t elapsed=usb_track_clock-diag_worker_entered;
    if (diag_batch_window!=usb_track_diag_window) return;
    if (elapsed>usb_track_diag_batch_max) usb_track_diag_batch_max=elapsed;
#ifdef V01_REMOTE_DIAGNOSTIC
    uint32_t ticks=v01_clock()-v01_batch_started;
    if(ticks>v01_batch_max) v01_batch_max=ticks;
#endif
}
#endif
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
#ifdef USB_TRACK_DIAGNOSTIC
        diag_stamp_entry();
#endif
        usb_track_tick(0);
#ifdef USB_TRACK_DIAGNOSTIC
        diag_stamp_exit();
#ifdef V01_REMOTE_DIAGNOSTIC
        diag_emit();
#endif
#endif
        ++usb_track_worker_runs;
    }
}
void usb_track_ui_tick(void *ctrl)
{
    (void)ctrl;
#ifdef USB_TRACK_DIAGNOSTIC
#ifdef V01_REMOTE_DIAGNOSTIC
    uint32_t ticks=v01_clock();
    if(v01_ui_seen && ticks-v01_ui_last>v01_ui_max) v01_ui_max=ticks-v01_ui_last;
    v01_ui_last=ticks; v01_ui_seen=1;
#endif
    ++usb_track_diag_ui_ticks;
    if (diag_ui_valid) {
        uint32_t gap=usb_track_clock-diag_ui_last;
        uint32_t ms=usb_track_diag_ms(gap);
        if (ms>usb_track_diag_ui_gap_max) usb_track_diag_ui_gap_max=ms;
    }
    diag_ui_last=usb_track_clock;
    diag_ui_valid=1;
#endif
    if (!started) {
        for (unsigned i=0;i<8;++i) worker_stack.low[i]=worker_stack.high[i]=GUARD;
        for (unsigned i=0;i<2048;++i) worker_stack.words[i]=0xa5a5a5a5u;
        fw_rtos_sem_init(&worker_sem,0);
        /* Priority14 below render IRQs; service blocks again after each batch.
         * Stack/TCB belong to linker BSS; no allocation or OS timer changes. */
        if (!fw_rtos_task_create(&worker_task,entry,
#ifdef V01_WORKER_PRIORITY
            V01_WORKER_PRIORITY,
#else
            14,
#endif
            worker_stack.words,sizeof worker_stack.words)) {
            ++usb_track_worker_guard_faults; return;
        }
        usb_track_control_set_wakeup(wake);
        usbmidi_device_set_wakeup(usb_track_worker_rx_wake);
        started=1;
        fw_rtos_task_start(&worker_task);
    }
    wake(); /* low-rate link-loss/reconnect fallback only */
}
