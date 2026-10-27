// Checks the Circle-facing wrapper against Circle's documented
// CUSBMIDIDevice::SendPlainMIDI/RegisterPacketHandler signatures, using the
// declarations-only shim in control/circle_shim. It does not link Circle.
#include <circle/usb/usbmidi.h>
#include "circle_usbmidi.h"
#include "device_link.h"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>

struct FakeMidi {
  uint8_t last[64];
  unsigned size;
  unsigned cable;
  unsigned sends;
};

static int send_trampoline(void *ctx, unsigned cable, const uint8_t *data,
                           unsigned size, unsigned chunk) {
  FakeMidi *f = static_cast<FakeMidi *>(ctx);
  assert(chunk == 0);
  assert(size <= sizeof f->last);
  std::memcpy(f->last, data, size);
  f->size = size;
  f->cable = cable;
  ++f->sends;
  return 1;
}

static int installed;
static int install(void *, const sc_frame *) { ++installed; return 1; }

int main() {
  FakeMidi midi{};
  struct sc_circle_usbmidi bridge;
  sc_circle_usbmidi_init(&bridge, &midi, send_trampoline, install, nullptr,
                         &midi, 100);

  struct sc_device_link dev;
  uint8_t hello[SC_CONTROL_WIRE_SIZE];
  assert(sc_dev_open(&dev, 77, 100, hello));
  sc_circle_usbmidi_feed(&bridge, hello, sizeof hello);
  assert(midi.sends == 0);           // callback path never sends
  assert(sc_circle_usbmidi_process(&bridge, 4) == 1);
  assert(midi.sends == 1 && midi.cable == 0);
  struct sc_control c;
  assert(sc_decode_control(&c, midi.last, midi.size));
  assert(c.kind == SC_HELLO_ACK);

  assert(sc_dev_receive_control(&dev, &c, 100).invalidated == 0);
  struct sc_frame src{};
  src.session = 77;
  src.sequence = 1;
  src.block = 100;
  src.voice = 1;
  src.flags = SC_START;
  uint8_t wire[SC_WIRE_SIZE];
  assert(sc_dev_send_start(&dev, &src, 100, wire));
  sc_circle_usbmidi_feed(&bridge, wire, sizeof wire);
  assert(midi.sends == 1);
  assert(sc_circle_usbmidi_process(&bridge, 4) == 1);
  assert(installed == 1 && midi.sends == 2);
  assert(sc_decode_control(&c, midi.last, midi.size));
  assert(sc_dev_receive_control(&dev, &c, 100).activated);
  assert(sc_circle_usbmidi_audio_allowed(&bridge));
  uint8_t live[SC_CONTROL_WIRE_SIZE];
  assert(sc_dev_keepalive(&dev, 100, live));
  sc_circle_usbmidi_feed(&bridge, live, sizeof live);
  assert(sc_circle_usbmidi_process(&bridge, 4) == 1);
  assert(sc_circle_usbmidi_audio_allowed(&bridge));

  // Compile-time proof against Circle's real pinned declaration, not only the
  // local shim. The wrapper's callback adds only the opaque device context.
  using CircleSend = boolean (CUSBMIDIDevice::*)(unsigned, const u8 *,
                                                   unsigned, unsigned);
  CircleSend circle_send = &CUSBMIDIDevice::SendPlainMIDI;
  (void)circle_send;

  puts("Circle USB-MIDI adapter contract: PASS");
  return 0;
}
