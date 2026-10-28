/* SPDX-License-Identifier: GPL-2.0-or-later */
/* OS 1.54 companion adapter; native ABI/addresses stay here for eventual
 * core integration. No caller may enter this adapter from render. */
#include "digitakt-mk1/core3.h"
#include "usb_midi.h"
#include "../../control/dt1_transport.h"

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
     * input bytes. Reserve the complete encoding, conservatively below FFB. */
    if (USB_QUEUED > 0xffbu || encoded > 0xffbu - USB_QUEUED)
        return 0;
    if (n > 0x2fu && USB_INDEX > 0xfbfu &&
        USB_QUEUED >= USB_INDEX - 0x3fu)
        return 0;
    return 1;
}

static uint32_t fw_send(void *ctx, const uint8_t *bytes, uint32_t n)
{
    uint16_t sr = irq_save();
    uint32_t before, accepted = 0, encoded = 4u * ((n + 2u) / 3u);
    /* One single-core critical section closes the preflight/send race with
     * stock MIDI writers. No task switches, completions or counter changes
     * can intervene. Native descriptor submission has a bounded 1000-poll
     * retry; its two unbounded capacity/wrap waits are excluded above. */
    if (fw_can_send(ctx, n)) {
        before = USB_QUEUED;
        USB_SEND(n, bytes); /* native function is void: d0 is not a byte count */
        if (USB_QUEUED - before == encoded) accepted = n;
        else failed = 1; /* descriptor exhaustion/partial submission: no retry */
    }
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
