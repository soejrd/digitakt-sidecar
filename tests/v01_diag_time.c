/* Exercise the actual diagnostic formatters and UI/worker counter producers.
 * RTOS calls are stubbed; this checks units, not native scheduling latency. */
#define USB_TRACK_DIAGNOSTIC
#define main integration_main
#include "i03_integrated.c"
#undef main
#include "../mods/usb-track/task_worker.c"

int fw_rtos_task_create(struct core_rtos_task *t, void (*fn)(void), uint32_t p,
                        void *s, uint32_t n)
{ (void)t; (void)fn; (void)p; (void)s; (void)n; return 1; }
void fw_rtos_task_start(struct core_rtos_task *t) { (void)t; }
void fw_rtos_sem_init(struct core_rtos_sem *s, uint32_t n) { (void)s; (void)n; }
void fw_rtos_sem_wait(struct core_rtos_sem *s) { (void)s; }
void fw_rtos_sem_signal(struct core_rtos_sem *s) { (void)s; }

int main(void)
{
    char text[16];
    /* 48 kHz / 32 frames = 1500 render blocks per second. */
    printf("1500 render blocks: diagnostic reports %u ms; expected 1000 ms\n",
           diag_ms(1500)); fflush(stdout);
    assert(diag_ms(1500) == 1000);
    assert(diag_ms(1) == 1 && diag_ms(3) == 2);
    assert(diag_ms(15000) == 9999 && diag_ms(UINT32_MAX) == 9999);
    started = 1; /* Skip task creation; exercise the real UI sampling path. */
    usb_track_clock = 100;
    usb_track_ui_tick(0);
    usb_track_clock += 1500;
    usb_track_ui_tick(0);
    assert(usb_track_diag_ui_gap_max == 1000);
    diag_ugap(text, 0, 0, 0); assert(strcmp(text, "1000") == 0);
    usb_track_clock = UINT32_MAX - 9;
    usb_track_ui_tick(0);
    usb_track_diag_ui_gap_max = 0;
    usb_track_clock = 20; /* 30 blocks across unsigned clock wrap. */
    usb_track_ui_tick(0);
    assert(usb_track_diag_ui_gap_max == 20);
    usb_track_clock = 100;
    diag_stamp_entry();
    usb_track_clock += 3;
    diag_stamp_exit();
    diag_batch(text, 0, 0, 0); assert(strcmp(text, "2") == 0);
    usb_track_clock = 1600;
    diag_stamp_entry();
    diag_wgap(text, 0, 0, 0); assert(strcmp(text, "1000") == 0);
    puts("V01 diagnostic units, saturation, UI/worker producers and wrap: PASS");
    /* A real machine-selection callback must discard the previous trial,
     * without clearing a sticky safety fault or changing the control session. */
    assert(usb_page.base.on_switch);
    usb_track_clock = 5000;
    usb_track_worker_guard_faults = 2;
    usb_page.base.on_switch(0, 0, 0);
    assert(usb_track_diag_ui_gap_max == 0 && usb_track_diag_ui_ticks == 0);
    diag_worker(text, 0, 0, 0); assert(strcmp(text, "0") == 0);
    diag_wgap(text, 0, 0, 0); assert(strcmp(text, "0") == 0);
    diag_batch(text, 0, 0, 0); assert(strcmp(text, "0") == 0);
    usb_track_ui_tick(0);
    diag_stamp_entry();
    assert(usb_track_diag_worker_entries == 1 && usb_track_diag_worker_gap_max == 0);
    assert(usb_track_diag_batch_max == 0 && usb_track_worker_guard_faults == 2);
    usb_track_clock += 1500;
    usb_page.extra[0].fmt(text, 0, 0, 0); assert(strcmp(text, "1") == 0);
    usb_track_ui_tick(0);
    assert(usb_track_diag_ui_gap_max == 1000);
    /* Keep an old nonzero sample, then reset during a later batch. */
    diag_stamp_exit();
    assert(usb_track_diag_batch_max == 1500);
    diag_stamp_entry();
    /* A reset between entry and exit must not republish an old batch. */
    usb_page.base.on_switch(0, 0, 0);
    usb_track_clock += 3000;
    diag_stamp_exit();
    assert(usb_track_diag_batch_max == 1500); /* Old storage left untouched. */
    diag_batch(text, 0, 0, 0); assert(strcmp(text, "0") == 0);
    diag_stamp_entry();
    assert(usb_track_diag_worker_entries == 1 && usb_track_diag_worker_gap_max == 0);
    usb_track_clock += 3;
    diag_stamp_exit();
    assert(usb_track_diag_batch_max == 3);
    puts("V01 trial reset, elapsed seconds and sticky-fault preservation: PASS");
    return 0;
}
