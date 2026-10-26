# R02 — physical USB descriptor capture

26 October 2026. Human-reported target: Digitakt mk1 OS 1.54. Measured VID/PID: `1935:0B2C`. [Raw capture](../evidence/r02-usb-descriptors-2026-10-26.txt); [decoded endpoints](../evidence/r02-endpoints-2026-10-26.json).

## Access correction

The prior report misidentified `0xE00002BE` as exclusive access. The installed Apple SDK's `IOKit/IOReturn.h` defines it as `kIOReturnNoResources`; exclusive access is `0xE00002C5`. Failure occurred at `IOCreatePlugInInterfaceForService`, before descriptor retrieval. Driver ownership was not established as its cause.

Compiled the existing inspector with `cc -O2 -Wall -framework IOKit -framework CoreFoundation tools/dt_usb_inspect.c -o /tmp/dt_usb_inspect_r02`. Running the same read-only binary outside the execution sandbox succeeded, with the Overbridge extension left unchanged. This isolates execution access as sufficient to release this blocker; it does not identify the precise internal reason the sandboxed API returned NoResources.

No interface was opened/claimed, no alternate setting selected and no audio streamed. No administrator driver removal or extension toggle was needed.

## Observed configuration

One configuration, 263 descriptor bytes, six interfaces. Interface 1 is the vendor audio IN path, interface 2 the OUT path. Device Speed is 2, defined as High Speed by the installed SDK's `IOKit/usb/USB.h` (`kUSBDeviceSpeedHigh`). `bcdUSB=0x0200` separately records the USB specification version.

| Interface | Alternate | Endpoint | Direction | Type | Maximum packet bytes | bInterval |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 1 | 0x83 | Digitakt → host | interrupt | 32 | 1 |
| 1 | 2 | 0x83 | Digitakt → host | interrupt | 88 | 1 |
| 1 | 3 | 0x83 | Digitakt → host | interrupt | 368 | 1 |
| 2 | 1 | 0x03 | host → Digitakt | interrupt | 32 | 1 |
| 2 | 2 | 0x03 | host → Digitakt | interrupt | 88 | 1 |
| 2 | 3 | 0x03 | host → Digitakt | interrupt | 88 | 1 |

Both interfaces also expose alternate 0 with no endpoints. At High Speed, interrupt bInterval 1 corresponds to a 125 µs service interval; it does not establish the actual packet or audio rate. Interface 3 has bulk OUT 0x02 (512 bytes) and interrupt IN 0x82 (64 bytes). Interface 4 is class 1/subclass 1; interface 5 is class 1/subclass 3 and exposes bulk OUT 0x01 / IN 0x81 at 512 bytes.

## Inferences and remaining limits

Under the source-described Overbridge v2 framing (32-byte header, seven frames, four bytes per sample), 368 bytes computes to 12 channels and 88 bytes to two. Physical packet capacities therefore agree with the Overwitch table. Audio contents, semantic channel names, sample width/order, sample rate and sustained transfer behavior have not been measured by this descriptor-only operation. The vendor descriptors do not establish these by themselves.

The host must handle independent input/output sizes. This capture does not establish MAIN-to-track routing or more than two independent conventional audio returns. R02 is complete; R03 and the unresolved receive-buffer handoff still gate the integrated audio architecture decision.

Validation: parsed every descriptor by its declared length, checked bounds and total 263-byte configuration length, and asserted alternate-3 endpoint capacities of 368 IN / 88 OUT against the decoded artifact.
