// Test shim: Circle's real CUSBMIDIDevice API, declarations only.
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef uint8_t u8;
typedef int boolean;
#define FALSE 0
#define TRUE 1
typedef void TMIDIPacketHandler(unsigned nCable, u8 *pPacket,
                                unsigned nLength);
class CUSBMIDIDevice {
 public:
  static const size_t EventPacketSize = 4;
  void RegisterPacketHandler(TMIDIPacketHandler *handler);
  boolean SendPlainMIDI(unsigned cable, const u8 *data, unsigned size,
                        unsigned chunk = 0);
};
