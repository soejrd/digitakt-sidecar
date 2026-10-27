# H02 — Real track pipeline

Status: **IN PROGRESS — physical firmware boot/UI pass; control activation fails**
(2026-10-27). Human confirmed hardware availability, backups and ability to
lower output levels. The Circle test kernel is installed on the existing
Sidecar SD and physically booted. The human reached the FUNC-startup OS receiver and reports sending I03P,
pressing YES and completing the upgrade. Normal boot and USB1 SRC engine parameters are human-verified; no test tone played.

## Current setup and next step

Use the **one existing Sidecar/Circle SD card**. The new Circle build
(`Oct 27 2026 20:54:01`) boots; its [15-second silence check](../evidence/h02/new-kernel-boot.json)
passes with advancing USB packets and zero gaps/errors/underruns. I03P boots;
USB1 and its SRC engine parameters on audio track 1 are human-verified.
[Activation fails](../evidence/h02/first-activation.json): the Pi rejects tone-on
because the control session is inactive. Port settings are MIDI+USB in/out,
TRIG KEY DEST INT. [CLOCK SEND test](../evidence/h02/midi-clock-check.json)
proves ordinary MIDI reaches Pi (~48 B/s without loss) while USB1 remains
inactive. The human played an existing beat, not an empty pattern. Tone is
off. Read native worker/render diagnostics before inferring a cause.

**Next candidate: H02D**, staged as
`private/h02/trial/Digitakt-H02D-diagnostic.syx`, SHA-256
`61dba3090f82893ee894133810de62aa23dae7dd8d6a465281ba06fd43ebd1e1`.
It adds a read-only second SRC page under `--diagnostic`; the normal build
retains one SRC page. [Compiled page/counter check](../evidence/h02/diagnostic-page.json)
and [activation/loss/reconnect regression](../evidence/h02/diagnostic-exchange.json)
pass in the emulator. No hardware fix or successful activation is claimed.

After human install, select/trigger USB1 on audio track 1, connect to Pi and
press SRC again. Read RCLK, TICK, STATE, SYNC, MIDI, TX and RX. Counters wrap
at 10000; flip SRC pages to refresh. RCLK/TICK show render/worker activity;
STATE combines route/gate/machine/mode/half validity; SYNC is the raw USB
sync flag; MIDI combines native ready/callback/link flags; TX/RX are submitted
messages/received bytes. Their bit definitions are in the diagnostic source.
Reproduce with a fresh root and the existing private runtime:

```sh
"$PY" tools/i02_probe.py candidate --root private/h02-diagnostic \
  --stock private/i02-fixture/Digitakt_OS1.54.syx --fixture private/i02-fixture --diagnostic
"$PY" tools/h02_diag_probe.py --candidate private/h02-diagnostic \
  --output evidence/h02/diagnostic-page.json
```

Restore CLOCK SEND to its pre-test setting. Keep the gate and original stock
recovery file; no bootloader changes are included.

[SD deployment evidence](../evidence/h02/sd-deployment.json): the installed
2,456,232-byte image matches the tested build, SHA-256
`1514f996a9c63e0d82bad5091cc3075c3dac52827273d701ae21494c89d07721`.
The old H01 kernel is verified in `kernel8-rpi4.before-h02.img` on the card
and `private/h02/sd-backup/kernel8-rpi4.img` locally. Other existing files,
including boot configuration and Wi-Fi credentials, are unchanged. WLAN
firmware check passes; `SHA256SUMS` is refreshed and its original retained.

Rollback with the Pi off and card mounted: copy
`kernel8-rpi4.before-h02.img` over `kernel8-rpi4.img`, and
`SHA256SUMS.before-h02` over `SHA256SUMS`, then eject and boot.

## Candidate and diagnostic host

Pair the Circle host with the existing source-pinned **I03P** firmware
candidate (`private/i03-release3/artifacts/I02.syx`), SHA-256
`8ab9d4728889fd0dbb0d3f6511e849c99d8841590cf26be58156a3ed0c868309`.
Artifact and firmware-source hashes were rechecked against fixture metadata.
Agree the concrete candidate and the
[I02 human trial/recovery procedure](I02-one-track-routing.md#human-hardware-trial-and-recovery-gate)
before the human installs it. Original stock 1.54 recovery SHA-256:
`f78ba80fa7b1da5fb0e1ff61ad61e9e71aafe79f4364fc49679f3651353e3cf6`.

[Host tests/build](../evidence/h02/test-host-build.json) pass. Apply Sidecar
patches 0001–0004 on the pinned base, then reproduce:

```sh
bash scripts/i03_prepare_circle.sh
bash scripts/test_i03_integration.sh
bash deps/sidecar-firmware/pi/engine/build.sh kernel
```

`h02tone=on|off` defaults off. On is rejected before the USB1 handshake;
activation alone never enables it. Explicit enable produces 3 kHz at
<= −42 dBFS on MAIN with a 200 ms fade and the existing limiter. Loss,
reconnect or lease expiry disarms it; a new explicit enable is required.
Actual Circle glue/Output passes ASan/UBSan; patches replay and match tested
files. Cross-build/host tests do not establish physical audio behavior.

## Physical evidence and remaining acceptance

The earlier H01 build's status said streaming while packet counters stayed
frozen: [30-second preflight](../evidence/h02/preflight-circle.json).
A [Digitakt restart](../evidence/h02/after-digitakt-restart.json) did not
recover; a [Pi restart](../evidence/h02/after-pi-restart.json) restored traffic
with zero gaps/errors/underruns over 15 seconds. Wi-Fi access works. Root
cause and automatic reconnect remain unqualified.

Start with headphone volume down, speakers off, analog loopback removed and
input monitoring off. Send silence first; enable the quiet tone only after
USB1 render activation on track 1. Verify filter, AMP, effects sends, no dry
bypass and a neighboring internal track. Reconnect the analog loopback only
for timing capture, with input monitoring off. Measure onset latency/jitter,
render headroom and reconnect freshness; agree the musical timing limit.
Complete a ten-minute zero-gap/underrun run before marking H02 DONE.
