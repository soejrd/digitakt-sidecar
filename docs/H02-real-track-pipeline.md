# H02 — Real track pipeline

Status: **IN PROGRESS — route guard diagnosed; tested fixes awaiting install**
(2026-10-27). H02D firmware boots on hardware and USB1 appears on audio
track 1 with engine SRC parameters. No test tone has played. Use the one
existing Sidecar/Circle SD card; computer UI access is prohibited by the user.

## Diagnosis and prepared updates

Human diagnostic readout: **STATE 20, SYNC 1, MIDI 125, TX 0**; RCLK
advances at ~1500/s, TICK ~30/s, RX rises ~13 bytes every 6 s. USB1 and half
selection are valid, but the route rejects the USB mode. Port settings are
MIDI+USB in/out and TRIG KEY DEST INT. The [clock check](../evidence/h02/midi-clock-check.json)
proves ordinary USB MIDI reaches Pi while USB1 activation remains closed.

[OS mode-table/physical-descriptor comparison](../evidence/h02/usb-mode-154.json)
identifies native Overbridge as **mode 6**, PID `0x0b2c`, with EP3 IN/OUT
368/88-byte packets. The old guard required mode 2 (MIDI-only PID `0x000d`),
and earlier emulator fixtures forced that wrong mode. [Fix evidence](../evidence/h02/mode6-fix.json)
records the production render regression failing before and passing after
the correction, with modes 0–5 and 7 rejected.

Install the tested pair; neither new artifact is installed yet:

- `private/h02/trial/Digitakt-H02F-mode6.syx`: mode-6 route guard plus the
  diagnostic SRC page. [Compiled page](../evidence/h02/mode6-diagnostic-page.json)
  and [activation/loss/reconnect exchange](../evidence/h02/mode6-exchange.json) pass.
- `private/h02/trial/kernel8-rpi4-clock.img`: Circle patch 0005 advances a
  separate 32-frame lease clock from rendered sample counts. Previously it
  reused the statistics counter, which resets every second. Actual-glue
  tests check 12+20-frame conversion and exact lease expiry. Pi cross-build
  and all five dependency patches replay/match. This is a separate source
  finding, not a physically observed activation failure.

Artifact/source hashes are in the fix evidence. The human performs firmware
installation using the [verified recovery procedure](I02-one-track-routing.md#human-hardware-trial-and-recovery-gate).
Stock recovery: `private/h02/trial/Digitakt-OS1.54-stock-recovery.syx`, SHA-256
`f78ba80fa7b1da5fb0e1ff61ad61e9e71aafe79f4364fc49679f3651353e3cf6`.
Back up the mounted card's current kernel before replacement; preserve
configuration/credentials and verify read-back before ejecting.

## Reproduction and current card recovery

Use the existing private runtime and a fresh candidate root:

```sh
python3 tools/h02_usb_mode.py --sections private/i02-fixture/sections \
  --output evidence/h02/usb-mode-154.json
bash scripts/test_i03_queue.sh
"$PY" tools/i02_probe.py candidate --root private/h02-mode6 \
  --stock private/i02-fixture/Digitakt_OS1.54.syx --fixture private/i02-fixture --diagnostic
"$PY" tools/h02_diag_probe.py --candidate private/h02-mode6 \
  --output evidence/h02/mode6-diagnostic-page.json
"$PY" tools/i03_integrated_probe.py --candidate private/h02-mode6 \
  --output evidence/h02/mode6-exchange.json
# Sidecar patches 0001–0005 applied on the pinned base:
bash scripts/i03_prepare_circle.sh
bash scripts/test_i03_integration.sh
bash deps/sidecar-firmware/pi/engine/build.sh kernel
```

The [first SD update](../evidence/h02/sd-deployment.json) is physically booted;
its [15-second silence check](../evidence/h02/new-kernel-boot.json) passed.
Original H01 kernel backups: card `kernel8-rpi4.before-h02.img` and local
`private/h02/sd-backup/kernel8-rpi4.img`. With Pi off, restore that backup as
`kernel8-rpi4.img` and `SHA256SUMS.before-h02` as `SHA256SUMS`, then eject.
Wi-Fi configuration/credentials and other existing files are preserved.
A separate earlier [USB stall](../evidence/h02/preflight-circle.json) recovered
by [Pi restart](../evidence/h02/after-pi-restart.json), not Digitakt restart;
its cause and automatic reconnect remain unqualified.

## Physical trial and acceptance

After both updates, reconnect Digitakt USB and boot Pi with Digitakt on.
Select/trigger USB1 on audio track 1, and recheck activation before tone.
SRC switches between engine and diagnostic pages; RCLK/TICK wrap at 10000.
Expected pre-gate STATE 29, active STATE 31; native MIDI ready is 125.
Flip SRC pages to refresh. Restore CLOCK SEND to its pre-test setting.

`h02tone=on|off` defaults off and rejects enable before the handshake.
Explicit enable produces 3 kHz at <= −42 dBFS on MAIN with a 200 ms fade and
limiter. Loss/expiry disarms it; reconnect requires a fresh explicit enable.
Keep headphones down, speakers off, analog loopback removed and monitoring
off for listening. Verify filter, AMP, sends, no dry bypass and a neighboring
internal track. Use the analog loopback only for timing capture, monitoring
off. Measure latency/jitter, render headroom and reconnect freshness; agree
the musical timing limit. Complete a ten-minute zero-gap/underrun run before
marking H02 DONE. Host/emulator checks do not replace this physical evidence.
