# H02 — Real track pipeline

Status: **BLOCKED pending candidate/recovery agreement** (2026-10-27). Human confirmed
Digitakt/Pi availability, backups and ability to lower output levels.
No firmware flashed, host deployed or audio command sent during this preflight.

## Physical preflight

[Read-only Circle observation](../evidence/h02/preflight-circle.json), reproduced:

```sh
python3 tools/h01_watch_circle.py --url http://sidecar.local --seconds 30 \
  --output private/h02/preflight-circle.json --log private/h02/preflight-circle.log
```

The running build reports Circle 51.1.1, Sidecar 0.2.0 / `6cff67b` (dirty),
built Oct 27 2026 10:33:06. Wi-Fi reports connected. Notes/drone are off,
output is MAIN. This is the earlier H01 host, not the I03 integrated build.

Across seven successful reads, USB IN/OUT counters remain 949880/949878;
engine underruns remain 986. The device log reports IN/OUT 0/s while the
status endpoint continues to report streaming. The monitor correctly fails
`in_and_out_progress`. No new reported USB error or underrun does not prove
healthy transport when no packets advance. Cause is unconfirmed.
The cumulative underruns predate this observation; their origin is unknown.
Wi-Fi reachability is now physically observed, superseding the earlier
pending-association note; this does not qualify audio stability over Wi-Fi.

## Recovery observation

The human reports the Digitakt was on while the Pi rebooted, and a USB
interruption occurred during setup; the precise sequence is not established.
A subsequent Digitakt power cycle did not restore progress:
[15-second check](../evidence/h02/after-digitakt-restart.json).
Power-cycling only the Pi with the Digitakt left on and connected restored
traffic: [15-second check](../evidence/h02/after-pi-restart.json) passes,
IN/OUT advance 102854/102857 packets, with zero reported gaps/errors and
zero FIFO underruns. This demonstrates recovery by Pi reboot, not automatic
reconnect recovery or an identified root cause. No source fix was applied.

## Trial gates and acceptance

Packet progress is restored; recheck after setup changes before the trial. Retain the
[I02 candidate/recovery procedure](I02-one-track-routing.md#human-hardware-trial-and-recovery-gate).
The I02A artifact hash was rechecked against `evidence/i02/build.json`:
`17a59a621fe479df1c7ad2e0ed6016708778fa24999ea4b14b37a42d608bc090`.
The original stock 1.54 recovery file hash was rechecked:
`f78ba80fa7b1da5fb0e1ff61ad61e9e71aafe79f4364fc49679f3651353e3cf6`.

Proposed trial pairing is I02A plus the existing Linux diagnostic, whose
fixed tone is −36 dBFS with a 200 ms initial fade and −12 dBFS clamp.
Human candidate/recovery agreement and Linux-card setup remain pending.
Alternative pairing details: I02A needs observed render
activation before nonzero PCM; I03P adds the handshake but its current
integrated Circle mode deliberately emits silence. It needs a gated quiet
test source before routing can be qualified. Do not treat either pairing
as already hardware-qualified.

Start with headphone volume down, speakers off, analog loopback removed and
input monitoring off. Use silence first, followed by a limited/faded quiet
tone only after USB1 render activation on track 1. Check filter, AMP, effects
sends, no dry bypass and a neighboring internal track. Reconnect the analog
loopback only for timing capture, keeping input monitoring off. Record onset
latency/jitter, render headroom, startup/reconnect freshness and a ten-minute
zero-gap/underrun run. Agree the musical timing limit before qualification.
