/* SPDX-License-Identifier: GPL-2.0-or-later */
/* OS 1.54 companion adapter; native ABI/addresses stay here for eventual
 * core integration. No caller may enter this adapter from render. */
#include "digitakt-mk1/core3.h"
#include "usb_midi.h"
#include "../../control/dt1_transport.h"
#ifdef V01_REMOTE_DIAGNOSTIC
#include "../usb-track/core_candidate_clock.h"
volatile uint32_t v01_send_calls,v01_send_ticks,v01_send_max;
#endif

#ifndef DT1_NATIVE_TEST
#define USB_READY      (*(volatile uint32_t *)0x40610474u)
#define USB_ISR_ON     (*(volatile uint32_t *)0xFC0B01C4u)
#define USB_INDEX      (*(volatile uint32_t *)0x4061047Cu)
#define USB_QUEUED     (*(volatile uint32_t *)0x40610478u)
#define USB_SYSEX      (*(volatile uint32_t *)0x40610464u)
#define USB_RX_SLOT    (*(volatile uintptr_t *)0x4061046Cu)
#define USB_SEND(n,p)  ((void (*)(uint32_t,const uint8_t *))0x40004990u)(n,p)
static uint16_t irq_save(void)
{
    uint16_t sr;
    __asm__ volatile ("move.w %%sr,%0\n\tmove.w #0x2700,%%sr" : "=d"(sr) :: "memory");
    return sr;
}
static void irq_restore(uint16_t sr)
{
    __asm__ volatile ("move.w %0,%%sr" :: "d"(sr) : "memory");
}
#else
#include "dt1_native_io.h"
#endif

/* 0x400049FE waits while queued + written > 0xFFB, so the whole encoding must
 * fit below it. That wait spins on a counter only the completion interrupt
 * lowers; with interrupts masked it would never end. */
#define USB_TRACK_QUEUE_LIMIT 0xffbu

static struct dt1_transport transport;
static void (*rx_handler)(void *, const uint8_t *, uint32_t);
static void *rx_context;
static void (*stock_rx)(const uint8_t *, uint32_t);
static uint8_t opened, failed;
static void (*volatile device_wakeup)(void);
void usbmidi_device_set_wakeup(void (*fn)(void)) { device_wakeup=fn; }

/* Native calls exactly (pointer, length), with no context argument. */
static void native_rx(const uint8_t *bytes, uint32_t n)
{
    if (rx_handler) rx_handler(rx_context, bytes, n);
    if (device_wakeup) device_wakeup();
}

static int fw_link_up(void *ctx)
{
    (void)ctx;
    return USB_READY == 1 && USB_ISR_ON != 0;
}

static int fw_can_send(void *ctx, uint32_t n)
{
    uint32_t encoded = 4u * ((n + 2u) / 3u);
    (void)ctx;
    if (!n || n > DT1_MSG_MAX || !fw_link_up(0) || USB_INDEX > 0xfffu || USB_SYSEX != 0)
        return 0; /* never interleave with a stock sender's partial SysEx */
    /* Native capacity test is queued + locally staged USB bytes, not MIDI
     * input bytes. Reserve the complete encoding below FFB. */
    if (USB_QUEUED > USB_TRACK_QUEUE_LIMIT ||
        encoded > USB_TRACK_QUEUE_LIMIT - USB_QUEUED)
        return 0;
    /* The sender decides the wrap itself at 0x400049C2: for messages over
     * 0x2F bytes it wraps only when the index is past 0xFBF and the queue has
     * drained; otherwise it simply writes on and submits a descriptor at
     * 0x40004C22. Every message this adapter sends is at most DT1_MSG_MAX, so
     * a single message can cross 0xFFF only when the index is already past
     * 0xFBF, i.e. exactly when the native code wraps first. Refusing early
     * here would be wrong: only our own sends advance the index, so a refusal
     * in that band stalls traffic permanently (observed in the emulator).
     * The descriptor submission's semaphore retry is bounded (1000 polls at
     * 0x40005DFC) and needs no interrupt to make progress. */
    if (n > 0x2fu && USB_INDEX > 0xfbfu &&
        USB_QUEUED >= USB_INDEX - 0x3fu)
        return 0; /* the sender would spin in the wrap wait */
    return 1;
}

static uint32_t fw_send(void *ctx, const uint8_t *bytes, uint32_t n)
{
    uint16_t sr = irq_save();
#ifdef V01_REMOTE_DIAGNOSTIC
    uint32_t started_at=v01_clock();
#endif
    uint32_t before, accepted = 0, encoded = 4u * ((n + 2u) / 3u);
    /* One single-core critical section closes the preflight/send race with
     * stock MIDI writers. No task switches, completions or counter changes
     * can intervene. The preflight must therefore run *inside* this section:
     * dt1_transport's unlocked can_send() is only an early filter, and the
     * stock stack can raise USB_QUEUED between that check and the mask.
     * Entering 0x40004990 past either limit spins on values that only the
     * completion interrupt lowers (0x400049D0, 0x400049FE) and the device
     * hangs. Refuse here instead: returning 0 is a deferral, not a fault.
     * The descriptor submission's own retry (0x40005DBA..0x40005E02, 1000
     * polls of a controller bit) needs no interrupt, so it is not a hazard. */
    if (fw_can_send(ctx, n)) {
        before = USB_QUEUED;
        USB_SEND(n, bytes); /* native function is void: d0 is not a byte count */
        if (USB_QUEUED - before == encoded) accepted = n;
        else failed = 1; /* descriptor exhaustion/partial submission: no retry */
    }
#ifdef V01_REMOTE_DIAGNOSTIC
    uint32_t elapsed=v01_clock()-started_at;
    ++v01_send_calls; v01_send_ticks+=elapsed;
    if(elapsed>v01_send_max) v01_send_max=elapsed;
#endif
    irq_restore(sr);
    return accepted;
}

static void *fw_set_rx(void *ctx, void *handler, void *handler_ctx, void **prev_ctx)
{
    uintptr_t previous;
    uint16_t sr = irq_save();
    (void)ctx;
    previous = USB_RX_SLOT;
    if (handler_ctx) {
        stock_rx = (void (*)(const uint8_t *, uint32_t))previous;
        rx_handler = (void (*)(void *, const uint8_t *, uint32_t))handler;
        rx_context = handler_ctx;
        USB_RX_SLOT = (uintptr_t)native_rx;
    } else {
        USB_RX_SLOT = (uintptr_t)handler;
        rx_handler = 0;
        rx_context = 0;
    }
    if (prev_ctx) *prev_ctx = 0;
    irq_restore(sr);
    return (void *)previous;
}

static const struct dt1_fw_ops fw_ops = {fw_link_up, fw_can_send, fw_send, fw_set_rx};

static void chain_stock(void *ctx, const uint8_t *p, uint32_t n)
{
    (void)ctx;
    if (stock_rx) stock_rx(p, n);
}

static int accept_dtsc(void *ctx, const uint8_t *f, uint32_t n)
{
    (void)ctx;
    return n >= 6 && f[0] == 0xf0 && f[1] == 0x7d && f[2] == 'D' &&
           f[3] == 'T' && f[4] == 'S' && f[5] == 'C';
}

int usbmidi_device_send(const uint8_t *bytes, uint32_t n)
{
    if (!opened || failed || !transport.link) return -1;
    /* Success means copied to our owned queue. A deferred submission must
     * never be reported as a refusal and then enqueued again by the caller. */
    return dt1_tx_stage(&transport, bytes, n) == DT1_TX_OK ? 0 : -2;
}

int usbmidi_device_link(void) { return opened && fw_link_up(0); }
uint32_t usbmidi_device_diag(unsigned field)
{
    if (field == 1) return transport.submitted;
    if (field == 2) return transport.rx_bytes;
    if (field == 3) return transport.staged;
    if (field == 4) return transport.tx_head-transport.tx_tail;
    return (opened ? 1u : 0u) | (failed ? 2u : 0u) |
           (USB_READY == 1 ? 4u : 0u) | (USB_ISR_ON ? 8u : 0u) |
           (USB_RX_SLOT ? 16u : 0u) |
           (USB_RX_SLOT == (uintptr_t)native_rx ? 32u : 0u) |
           (transport.link ? 64u : 0u) | (USB_SYSEX ? 128u : 0u);
}
int usbmidi_device_failed(void)
{
    return failed || transport.tx_overflow || transport.submit_failures || transport.rx.dropped;
}

void usbmidi_device_init(void (*on_sysex)(void *, const uint8_t *, uint32_t), void *ctx)
{
    uint16_t sr = irq_save();
    dt1_transport_init(&transport, &fw_ops, 0);
    transport.on_sysex = on_sysex;
    transport.accept = accept_dtsc;
    transport.chain = chain_stock;
    transport.cb_ctx = ctx;
    failed = opened = 0;
    irq_restore(sr);
}

void usbmidi_device_open(void)
{
    uint16_t sr = irq_save();
    if (!opened && USB_RX_SLOT) opened = dt1_transport_open(&transport);
    dt1_link_update(&transport, fw_link_up(0));
    irq_restore(sr);
}

void usbmidi_device_close(void)
{
    uint16_t sr = irq_save();
    if (opened && USB_RX_SLOT == (uintptr_t)native_rx) dt1_transport_close(&transport);
    opened = 0;
    dt1_link_update(&transport, 0);
    irq_restore(sr);
}

void usbmidi_tick(void *ctrl)
{
    uint16_t sr;
    (void)ctrl;
    if (!opened) return;
    /* USB reinitialization replaces the callback; capture the new stock
     * handler rather than restoring a stale pointer after a cable replug. */
    sr = irq_save();
    if (USB_RX_SLOT && USB_RX_SLOT != (uintptr_t)native_rx) {
        stock_rx = (void (*)(const uint8_t *, uint32_t))USB_RX_SLOT;
        transport.prev_rx = (void *)USB_RX_SLOT;
        USB_RX_SLOT = (uintptr_t)native_rx;
    }
    dt1_link_update(&transport, fw_link_up(0));
    irq_restore(sr);
    if (failed) return;
    dt1_rx_service(&transport, 4096u);
    dt1_tx_service(&transport, 64u);
}
