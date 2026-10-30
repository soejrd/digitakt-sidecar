# V01 Gates B and C — runbook

Run on **I04R**, the accepted prototype
(`private/v01/firmware-V01K/Digitakt-I04R-previous.syx`, SHA256
`6a04ae9d…f7fa2b`). V01K froze and must not be used for these gates.

Deliberately scoped: B and C need a *stable device*, not dense modulation. The
freeze stays parked as V01-A. Keep every LFO slow/shallow, at or below what I04
already accepted ("slow/shallow PTCH LFO plus brief PTCH/DEC edits").

## Non-negotiables

- Disposable project only. Never a retained personal project.
- Mute the Pi voice except where the case is about audio output; then lower
  monitors and start with a quiet, limited tone.
- Never dump full traces during timing work; printing previously caused
  underruns. Read `/status` and `v01status` instead.
- Pi HTTP: `http://192.168.1.15`, GET `/status`, `/log`; POST `/cmd` with
  `c=trx=off`, `trxtrace=off`, `trxstatus`, `v01status`.
- Stop a case at the first freeze, new Pi fault or FIFO underrun growth.
- Reboot the Pi before a timing-sensitive set so counters start clean.
- Counter interpretation: `Underflows` counts empty requested output frames,
  including intentional FIFO flushes on control-epoch changes. Stop/review new
  growth as above, but do not equate it with CPU starvation or independent USB
  failures. [Offline proof](../evidence/v01/output-counter-semantics.json).
- For a targeted muted timing diagnosis, the existing128-record `trxtrace=on`
  capture freezes on the first playback fault. Keep actions separately marked;
  stop capture with `trxtrace=off` before requesting the `trxtrace` dump.
  Dumping is post-trial only and its counter effects are outside the trial.
  This is not permission to resume the parked V01-A dense workload.

## Gate B — persistence and edit semantics

**Emulator status (corrected2026-10-30):** The eight TRX controls use native sound
slots17-24; diagnostic/unused extras are slots46-52. Custom version-4 records
are160 bytes. A V01-specific full project round trip is not yet established
in the emulator. The physical displayed-state results are in TASKBOARD/report.
[Feasibility and failed attempts](../evidence/v01/gate-b-emulator-feasibility.json).
Resume by porting the call pattern from
`deps/elekloader/tests/probe_digitakt_src.py` (unpatched machine, `MAP_INIT` and
`SOUND_INIT` first, native `SOUND_SAVE=0x4007A5A0`, `SOUND_LOAD=0x4007A236`).
Physical qualification still requires hardware. Later X01 emulator probes cover
SRC-page copy and seeded RAM sound-lock selection/restoration, but do not
constitute complete V01-B cases or saved-project persistence.

For each case record: tiden values before, the action, the value after, the
restored state, output (muted or audible), and any unintended dirty flag.

Use distinguishable numbers per control (e.g. PTCH 20, DEC 33, RAMP 41, HOLD 52)
so a swapped or defaulted value is obvious.

| # | Case | What "pass" means |
| --- | --- | --- |
| B1 | Project save → reload | Same eight SRC values, note/velocity policy and machine selection return |
| B2 | Cold boot → load project | Same as B1, from power-off, no Pi caching involved |
| B3 | Sound save → load | Sound-level values return; unchanged values stay unchanged |
| B4 | Track/pattern copy-paste | Copied track carries USB1 selection and values, neighbours unchanged |
| B5 | Supported undo / reload | Undo restores previous values; no stale stream residue |
| B6 | Mute / unmute during play | No stuck gate, no stuck note; native tail behaves as with a stock machine |
| B7 | Sound lock on a step | Locked values apply, then base sound restores on the following step |
| B8 | Switch machine during play | No freeze, no stuck audio; returning to USB1 rebuilds state |
| B9 | Persistence without the Pi | Save, unplug the Pi, reload: the project alone determines state |
| B10 | Copy USB1 to another track | Either works, or is documented as safe-unsupported |

B9 is the one that cannot be faked by host caching; it must be run.

## Gate C — recovery and soak

Numbered profile to freeze first: machine selection, LFO destination and all
settings, tempo, notes/velocity/locks, stock track load. Write the numbers in
the evidence file, not "the usual pattern".

| # | Case | Repeat | Pass |
| --- | --- | --- | --- |
| C1 | USB pull/replug during PCM | 3 | Fresh session, START and matching ACTIVATE; no old PCM/ACK carried over |
| C2 | Pi reboot during PCM | 3 | Reconnect without stale state; no stuck silence |
| C3 | Digitakt restart during PCM | 3 | Pi resyncs; counters consistent |
| C4 | Combined soak | 1 × 10 min | Notes, locks, UI edits, mutes, state changes; zero new faults, underruns, dropped/inserted blocks |
| C5 | UI thresholds | — | Panel/Play/Stop within 100 ms; UI-service gaps ≤ 100 ms, measured, not eyeballed |
| C6 | I04 timing recheck | — | Recheck the accepted ~11.4 ms relative onset and disclose any tradeoff |

A shorter human-accepted soak is a limited result, not a pass.

## Evidence

One compact JSON per gate under `evidence/v01/`, with the image hash, the
numbered profile, per-case before/action/after and the human's listening/UI
opinion. Raw captures stay private. Update [the report](V01-persistence-stress.md)
and the board when a gate closes; do not leave trial narratives in either.

## Current muted timing workflow

Use `tools/v01_timing_capture.py` with a new private output directory and the
existing I04R/Pi pairing; it never enables audio or changes firmware. Establish
an untouched baseline, mark one action in the directory's action.json, then
perform only that action. Historical fault counts are baselined; a new fault
ends capture. A stop file ends a clean observation. The complete bounded dump
is collected only after capture is off, into standalone trace.txt. Analyze it
with `tools/i04_analyze_trace.py --delay 128`; never combine earlier dumps from
raw-log.txt. Run `python3 tests/test_v01_timing_capture.py` before tool changes.

Latest separated trials reproduce reload-only SC_PF_LATE; idle and slow PTCH-only
trials do not. [Evidence](../evidence/v01/i04r-separated-timing.json). Hardware
remains muted; this is diagnosis, not a Gate B/C pass or authorization for dense
V01-A trials. The pending TX-timeout source change is not installed and does
not explain the104ms control timing gap.
