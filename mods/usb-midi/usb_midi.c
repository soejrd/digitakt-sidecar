/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Digitakt mk1 OS 1.54 USB-MIDI transport adapter: the firmware-facing half.
 *
 * All firmware knowledge is here, pinned to OS 1.54 and cited from
 * evidence/i03/transport-154.json; control/dt1_transport.c holds the
 * device-neutral rules and control/dt1_usbmidi_model.c models these same
 * paths for host tests.
 *
 * Ownership: the adapter is the single caller of the raw-MIDI sender
 * 0x40004990 and the single owner of its queue counters. Only the UI task
 * calls it. Render may only stage a message. */
#include "digitakt-mk1/core3.h"
#include "../../control/dt1_transport.h"

/* Raw-MIDI sender state, from the disassembly (see dt1_usbmidi_model.h). */
#define USB_READY      (*(volatile uint32_t *)0x40610474u)  /* 0x400049A2 */
#define USB_ISR_ON     (*(volatile uint32_t *)0xFC0B01C4u)  /* 0x400049AC */
#define USB_INDEX      (*(volatile uint32_t *)0x4061047Cu)  /* 0x400049E0 */
#define USB_QUEUED     (*(volatile uint32_t *)0x40610478u)  /* 0x400049F0 */
#define USB_RX_HANDLER 0x4061046Cu                            /* 0x400045AA */
#define USB_QUEUE_LIMIT 0xFFBu                                /* 0x40004A06 */
#define USB_WRAP_LIMIT  0xFBFu                                /* 0x40004A06 site */
#define USB_WRAP_KEEP   0x3Fu                                 /* 0x400049CA */
#define USB_MSG_WRAP    0x2Fu                                 /* 0x400049B6 */

typedef uint32_t (*usb_send_fn)(uint32_t count, const uint8_t *bytes);
typedef void (*usb_rx_fn)(void *ctx, const uint8_t *bytes, uint32_t count);

static struct dt1_transport transport;
static uint8_t sysex_out[DT1_RX_FRAME];

static int fw_link_up(void *ctx)
{
    (void)ctx;
    return USB_READY == 1 && USB_ISR_ON != 0;
}

static int fw_can_send(void *ctx, uint32_t n)
{
    (void)ctx;
    if (USB_READY != 1 || !USB_ISR_ON)
        return 0;
    /* 0x400049FE..0x40004A0C: the per-byte capacity wait. */
    if (USB_QUEUED >= USB_QUEUE_LIMIT)
        return 0;
    /* 0x400049B6..0x400049D8: longer messages wrap when index > 0xFBF and
     * busy-wait until queued < index - 0x3F. Never enter it. */
    if (n > USB_MSG_WRAP && USB_INDEX > USB_WRAP_LIMIT &&
        USB_QUEUED >= USB_INDEX - USB_WRAP_KEEP)
        return 0;
    return 1;
}

static uint32_t fw_send(void *ctx, const uint8_t *msg, uint32_t n)
{
    (void)ctx;
    return ((usb_send_fn)0x40004990u)(n, msg);
}

static void *fw_set_rx(void *ctx, void *handler, void *handler_ctx,
                       void **prev_ctx)
{
    volatile uint32_t *slot = (volatile uint32_t *)USB_RX_HANDLER;
    uint32_t prev = *slot;
    (void)ctx;
    if (prev_ctx)
        *prev_ctx = 0;      /* the stock handler's own context is a constant */
    *slot = (uint32_t)(uintptr_t)handler;
    (void)handler_ctx;
    return (void *)(uintptr_t)prev;
}

static const struct dt1_fw_ops fw_ops = {
    fw_link_up, fw_can_send, fw_send, fw_set_rx
};

/* Our frames only: the DTSC signature and a known control version. START
 * (version 1) and the session control frames (version 2) are both ours. */
static int accept_dtsc(void *ctx, const uint8_t *f, uint32_t n)
{
    (void)ctx;
    return n >= 8 && f[0] == 0xf0 && f[1] == 0x7d && f[2] == 'D' &&
           f[3] == 'T' && f[4] == 'S' && f[5] == 'C' &&
           (f[6] == 1 || f[6] == 2) && f[n - 1] == 0xf7;
}

/* Provided for the handshake module (ev_tick context). Nonblocking: it
 * submits only when the firmware will not spin, otherwise reports busy. */
int usbmidi_device_send(const uint8_t *bytes, uint32_t n)
{
    if (!dt1_link_update(&transport, fw_link_up(0)))
        return -1;
    if (dt1_tx_stage(&transport, bytes, n) != DT1_TX_OK)
        return -2;
    return dt1_tx_service(&transport, 1u) ? 0 : -3;
}

int usbmidi_device_link(void)
{
    return dt1_link_update(&transport, fw_link_up(0));
}

/* Set by the control module before the link is opened. */
void usbmidi_device_init(void (*on_sysex)(void *, const uint8_t *, uint32_t),
                         void (*chain)(void *, const uint8_t *, uint32_t),
                         void *ctx)
{
    dt1_transport_init(&transport, &fw_ops, 0);
    transport.on_sysex = on_sysex;
    transport.accept = accept_dtsc;
    transport.chain = chain;
    transport.cb_ctx = ctx;
}

void usbmidi_device_open(void)
{
    if (dt1_transport_open(&transport))
        dt1_link_update(&transport, fw_link_up(0));
}

void usbmidi_device_close(void)
{
    dt1_transport_close(&transport);
    dt1_link_update(&transport, 0);
}

/* ev_tick, order 30 (core 4.0): the only place firmware transport runs. */
void usbmidi_tick(void *ctrl)
{
    (void)ctrl;
    dt1_link_update(&transport, fw_link_up(0));
    dt1_tx_service(&transport, 8u);
    dt1_rx_service(&transport, 512u);
}

void usbmidi_device_stats(uint32_t *submitted, uint32_t *dropped,
                          uint32_t *deferrals, uint32_t *frames)
{
    if (submitted) *submitted = transport.submitted;
    if (dropped) *dropped = transport.dropped;
    if (deferrals) *deferrals = transport.deferrals;
    if (frames) *frames = transport.rx_frames;
}

/* Unused storage keeps the linker from folding the receive buffer away. */
void *usbmidi_sysex_buffer(void)
{
    return sysex_out;
}
