# H02 — Real track pipeline

**DONE by explicit user acceptance (2026-10-28).** The user accepted the
approximately 8½-minute run in place of ten minutes, ended further testing,
and requested completion. This accepts the physical routing milestone;
latency, full render headroom and physical reconnect stress remain unmeasured.

## Physical result

H02F firmware and the corrected Circle kernel booted on Digitakt mk1 OS 1.54
and Pi 4B. USB1 appeared on audio track 1 with eight SRC engine parameters.
The live host accepted `h02tone=on` after the control handshake; the human
heard the quiet 3 kHz tone and verified:

- Filter cutoff, AMP volume/decay and overdrive affect the tone.
- Reverb tails and delay repeats work through the track sends.
- The neighboring internal track remains audible with USB1 AMP VOL at zero.

These are human listening observations, not calibrated attenuation or complete
output-identity measurements. [Routing evidence](../evidence/h02/physical-routing.json).

[Stability evidence](../evidence/h02/physical-stability.json): **506.758 s**,
101 successful reads, 3,476,639 IN and 3,476,642 OUT packets advanced. No new
USB gaps/errors or FIFO underruns; the FIFO counter remained 10 throughout.
Those accumulated underruns predate the observation and were not isolated.
The human then powered Pi off; subsequent HTTP timeouts are expected shutdown,
not proof of a transport failure. A full 600-second live run is not claimed.

The [timing attempt](../evidence/h02/timing.json) captured twelve source pulses,
but USB MAIN had no matching returned pulse train after startup. No valid
latency was extracted. Analog loopback was not performed. Firmware-derived
instruction counts and Pi CPU logs do not qualify full Digitakt render headroom.
These limits remain explicit despite user acceptance; I04 is now released.

## Corrections and reproducible checks

H02D showed STATE 20, SYNC 1, MIDI 125, TX 0, with render/worker clocks active.
[Mode-table/physical-descriptor comparison](../evidence/h02/usb-mode-154.json)
identified native Overbridge as mode **6**, PID `0x0b2c`, EP3 IN/OUT 368/88 B.
The old mode-2 guard rejected it; old emulator fixtures hid that error by
forcing mode 2. H02F fixes the guard. The production regression fails before
and passes after, rejecting other modes.

Circle patch 0005 separates the lease clock from per-second reset statistics,
advancing in 32-frame units from actual samples. Actual-glue tests cover
12+20-frame conversion and exact expiry. Host/ColdFire regression, compiled
boot/page/exchange, Pi build and five-patch replay pass. Artifact/source hashes
and commands: [fix evidence](../evidence/h02/mode6-fix.json),
[page](../evidence/h02/mode6-diagnostic-page.json),
[exchange](../evidence/h02/mode6-exchange.json).

```sh
bash scripts/test_i03_queue.sh
bash scripts/test_i03_integration.sh
python3 tools/h02_usb_mode.py --sections private/i02-fixture/sections \
  --output evidence/h02/usb-mode-154.json
python3 tools/h02_analyze_timing.py --folder private/h02/timing-wav \
  --capture private/h02/timing-capture.json --output evidence/h02/timing.json
```

The final command records the useful negative timing result; it does not
claim a successful latency measurement. Private firmware/captures stay ignored.

## Installed state and recovery

Pi is off, ending the runtime tone. One Sidecar/Circle SD card is used;
settings/credentials are unchanged. Installed firmware:
`private/h02/trial/Digitakt-H02F-mode6.syx`. Kernel deployment/read-back/boot:
[record](../evidence/h02/sd-clock-deployment.json).

Previous H02 backups: card `kernel8-rpi4.before-clock.img` and local
`private/h02/sd-backup-clock/kernel8-rpi4.img`. With Pi off and card mounted,
restore that image as `kernel8-rpi4.img` and `SHA256SUMS.before-clock` as
`SHA256SUMS`, then eject. Original H01 backups remain separately available.
Digitakt stock recovery is `private/h02/trial/Digitakt-OS1.54-stock-recovery.syx`;
use the [human recovery procedure](I02-one-track-routing.md#human-hardware-trial-and-recovery-gate).
No bootloader/internal wiring changes. Computer UI access is prohibited by user.

The tone defaults off on boot and requires explicit enable after handshake;
it is faded, limited and <= −42 dBFS on MAIN before Digitakt processing.
This fixed-tone proof is not TRX-B2 synthesis: that is I04. Startup underruns,
latency, render headroom and reconnect qualification remain follow-up limits.
