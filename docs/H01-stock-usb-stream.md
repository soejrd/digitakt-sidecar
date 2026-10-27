# H01 — Physical stock USB streaming

**H01 DONE:** Pi Linux transport/analog loopback and the patched Circle host
both pass four-minute physical tests. The spare FAT32 SD was read-back verified
and booted successfully. No Digitakt firmware was changed. Target: Digitakt
mk1 OS 1.54, Pi 4B Rev 1.5 (4 GB).

## Measured on Pi Linux

[Four-minute results](../evidence/h01/linux-stability.json),
[left](../evidence/h01/left-analysis.json) and
[right](../evidence/h01/right-analysis.json) loopback analysis.

- 240.000119 seconds, 1,645,692 IN packets and 1,645,695 OUT completions.
  Zero IN counter gaps/lost packets, short packets, bad headers, USB errors
  or output-credit overflows. All submitted transfers were drained on exit.
- Descriptor-validated alt 3, interrupt EP `0x83` IN / `0x03` OUT: 368 / 88
  bytes, seven frames per packet, 32-byte header and big-endian int32 words.
  The measured packet cadence is 47,999.485 frames/s, consistent with 48 kHz.
  This is a host timestamp measurement, not a calibrated physical clock.
- Host return channel 0 reaches MAIN OUT L; channel 1 reaches MAIN OUT R.
  Each was physically looped into INPUT L, observed at IN channel 10
  (zero-based). Both captures show the 440 Hz test tone; captured input words
  have eight zero low bits, consistent with 24-bit ADC data in 32-bit words.
  Other track/input channel meanings have not all been measured.
- A -36 dBFS pulse stream (200 ms initial fade, 2 ms pulse edges, -12 dBFS
  hard clamp) returned all 478 pulses. Host-observed round trip:
  minimum 5.344 ms, median 5.724 ms, p95 5.928 ms, maximum 6.029 ms.
  The 0.685 ms range includes Linux scheduling, USB queues, analog output/input
  and threshold detection. It excludes semantic-event transport and synthesis;
  it is not the eventual machine's trigger-to-sound latency or Circle latency.

USB MAIN did not contain the returned test tone in these captures. An analog
MAIN→INPUT loop was therefore used. The input was captured only, never forwarded
back to the host output. Analog volume/input gain were not calibrated. Packet
counters and returned pulses do not expose the device's internal underrun
counter; Circle's host FIFO counter is a separate qualification below.

## Measured on Circle

[Circle stability results](../evidence/h01/circle-stability.json): 240.060 seconds
of network observation, with 242 seconds between the selected device log
boundaries. IN/OUT advance by 1,659,392 / 1,659,393 packets over those boundaries.
All USB error/gap/credit-drop/submit-failure counters are zero at both boundaries.
Engine FIFO underruns remain exactly zero; all 49 status reads report streaming,
USB link and MAIN mode. No network read fails; maximum observed temperature is
34 °C. The stock USB MIDI interface is also enumerated.

This Circle run sends silence with notes/drone off and FX off. Its full engine
binary, directional serializer and real FIFO are active, but synthesis load,
nonzero Circle output calibration and Circle round-trip timing are not qualified
by this run. The measured quiet-tone format/channel/timing proof above uses the
separate Linux diagnostic on the same Pi and stock Digitakt. H02 must measure
the actual integrated track/control path; R05 must measure TRX-B2 CPU/resampling.

The Pi is left running Circle on the spare SD with the network console at
`http://sidecar.local/`. Its normal Linux card is kept separately; Linux boot
configuration/kernel were never replaced. Earlier diagnostic/staging files in
that Linux user's home directory remain disposable test files.

## Host and build work

[Diagnostic](../tools/h01_usb_stream.cpp),
[analyzer](../tools/h01_analyze.py),
[Circle monitor](../tools/h01_watch_circle.py),
[build evidence](../evidence/h01/circle-build.json).

The diagnostic uses fixed transfer pools, descriptor validation and independent
368/88-byte lengths. It sends silence by default and cancels/drains transfers
before restoring alternate settings. IN arrivals grant bounded OUT credits;
credits survive batched completions. An initial host implementation dropped six
OUT opportunities when all slots were busy; that run failed. The credit fix
passed the subsequent channel and four-minute tests.

The Mac claim failed with `LIBUSB_ERROR_ACCESS`; libusb explicitly reported an
exclusive opener while Elektron's DriverKit extension was enabled. Read-only
R02 success did not establish that streaming interfaces could be claimed.
The Pi test used a temporary ACL on the one connected USB node. No Mac driver
was unloaded, and the Pi's Linux installation was not replaced.

The full Pi 4 Circle build found an I01 compile error: static `CaptureMarkMIDI`
used instance member `m_nInChannels` without an object. The
[follow-up patch](../patches/sidecar-firmware/0002-static-capture-marker.patch)
uses the active instance with a null guard. Apply it after patch 0001. The full
kernel now builds; the existing I01 host regression suite passes. Upstream setup
also required a temporary source alias without spaces and fetching pinned hostap
before the first engine-tree copy; the helper records those workarounds.

## Wi-Fi SD dependencies

The Circle kernel also needs the Pi 4 WLAN files under `/firmware/` on the
SD card. Merely enabling `wifi=1` and saving credentials does not start the
chip when those files are absent. This omission was found after the first
Wi-Fi-only boot attempt; all three files were missing from the H01 SD package.
The returned card was repaired without changing its kernel or credentials.
Association/DHCP still needs a new physical boot; the wired H01 stream proof
does not qualify Wi-Fi operation.

`scripts/h01_build_circle.sh` now includes those dependencies and their licence.
For an existing mounted card, run `python3 scripts/prepare_pi_wifi.py
--card-root /Volumes/SIDECAR`; append `--check` for an offline check. The helper
pins the exact RPi-Distro revision used by Circle's WLAN Makefile, verifies
sizes and Git blob IDs, and refuses to overwrite differing files. Downloads
remain on the SD/package, outside Git. It never reads network credentials.

## Reproduce

Keep captures, generated firmware and build logs in ignored `private/h01/`.
Run USB tests only after lowering output levels and confirming the audio cable
connects a MAIN output to an INPUT, with input monitoring off. The test sends
only a fixed quiet source; it does not forward captured audio.

```sh
# Apply both dependency patches on the pinned clean Sidecar checkout first.
H01_PI_IP=<unused-address-on-your-LAN> scripts/h01_build_circle.sh

# Linux diagnostic: compile on the Pi with g++ and libusb development headers,
# or in the project's aarch64 Docker toolchain image with libusb installed.
g++ -std=c++17 -O2 -Wall -Wextra -Werror \
  -Ideps/sidecar-firmware/teensy/libraries/RytmHost \
  tools/h01_usb_stream.cpp -lusb-1.0 -o usb-stream
./usb-stream --seconds 5 --json silence.json
./usb-stream --seconds 240 --tone 1 --pulse \
  --json stability.json --capture stability.raw
python3 tools/h01_analyze.py --metrics stability.json \
  --raw stability.raw --output evidence/h01/linux-stability.json

# After a Circle boot, warm-up and confirmed streaming:
python3 tools/h01_watch_circle.py --url http://sidecar.local --seconds 240 \
  --output evidence/h01/circle-stability.json --log private/h01/circle.log
```

The SD helper prepares a folder only. The physical spare card was explicitly
authorized for overwrite, identified by its Secure Digital/removable metadata,
formatted FAT32/MBR, copied, hash-verified and ejected. The card boots straight
into the kernel, with no serial bootloader needed. It starts with silence:
`out=main notes=off gadget=0 wifi=0`, plus wired IP/netmask/web options.
The Circle network console is described in the pinned Sidecar
`docs/pi-engine.md` (Web page section). Keep the normal Linux card safe for
recovery; shutting down and swapping back restores that installation.
