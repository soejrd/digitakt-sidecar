# Digitakt integrated sidecar plan

Status: proposed research and prototype plan, 26 October 2026.

Current execution status and dependencies are maintained in [TASKBOARD.md](TASKBOARD.md). See [docs/BASELINE.md](docs/BASELINE.md) for inherited evidence.

## Goal and first milestone

Run TRX-B2 on an external processor while using the Digitakt as the instrument: select the machine on an audio track, edit its eight synthesis controls on SRC, sequence and parameter-lock those controls, and process the returned voice through that track's normal filter, amp and effects sends.

The first milestone is **one external TRX-B2 voice on one existing audio track**. Adding more machine types is in scope; adding sequencer tracks is a separate project. MIDI-only control with a MAIN return may help diagnose USB, but does not satisfy this milestone.

Confirmed target: Digitakt mk1 OS 1.54; Raspberry Pi 4B available. Prefer Raspberry Pi for the initial external host, following the creator's development path, with the Mac as the first transport test environment. Keep Teensy 4.1 as a later target; make that choice after measuring the required link and DSP load. No hardware purchase is needed to write or begin the host-side experiments.

## Evidence and boundaries

- [Creator's Digitakt guide](docs/DIGITAKT-GUIDE.md): useful USB/Overbridge architecture and staged transport tests, explicitly untested on Digitakt. It covers stock firmware and excludes the deep integration sought here.
- [Sidecar firmware](https://github.com/trigcondition/sidecar-firmware): public 0.2.0 source; audit the implementation before assuming its Rytm behavior transfers to Digitakt. The exact upstream guide has been captured in docs/DIGITAKT-GUIDE.md; the earlier supplied copy had a stale source-availability statement.
- [Local synthesis hook](deps/elekloader/examples/sine-machine/sine.c) and [Digitakt API](deps/elekloader/elekloader/sdk/include/digitakt-mk1/core3.h): candidate machine UI and per-track insertion point before overdrive. Treat source descriptions and reported hardware results as leads to reproduce.
- [TRX-B2](deps/mdkit/src/trx/b2/mdkit_trx_b2.c): existing native tests passed with zero failures. Desktop synthesis is established; external-host integration is not.
- [Prior feasibility notes](docs/BASELINE.md): evidence inventory and limitations. This plan supersedes their MIDI-first implementation direction.

The creator's stock-only guide cannot itself provide a recipe for this goal. Keep any new Digitakt firmware integration in a separate project/workstream, subject to the relevant repository's instructions. Do not assume patches developed using firmware research can be contributed to the creator's clean-room project. No firmware flashing is part of writing this plan; emulator work and physical transport measurements precede a hardware firmware trial.

## 1. Establish the actual hardware and software baseline

Record Digitakt model, OS, available Pi/Teensy hardware, repository revisions, compiler versions and emulator configuration. Pin one supported OS for the prototype rather than supporting multiple versions immediately.

Obtain the Sidecar source locally; read its contribution/build instructions and run the portable tests relevant to the chosen host. Retain the existing TRX-B2 tests and a default-hit reference. Record which emulator runs this Digitakt OS and what it omits, especially the USB/interface controller.

**Done when:** a reproducible baseline exists, with versions, commands, results and known omissions.

## 2. Resolve the audio-return question first

This is the main feasibility gate. Before adding an engine or elaborate UI:

1. Read the physical Digitakt's USB descriptors in Overbridge mode. Record VID/PID, speed, interface/alternate settings, endpoint types and intervals, packet sizes, sample format and independent IN/OUT channel counts. Do not substitute Rytm constants.
2. Audit the Rytm implementation end to end: USB host output, Rytm firmware receive path, per-track routing and control protocol. Identify which parts are reusable and which depend on its hardware.
3. Trace the corresponding Digitakt receive path and how received audio becomes accessible to its main audio renderer. Determine whether MAIN return data can be redirected to one track and whether enough independent channels are available for eventual expansion. Multiple USB receive channels must not be assumed.
4. First exercise stock streaming with silence and a quiet tone. Separately demonstrate the candidate track insertion point in the emulator using known PCM, bypassing the external transport.

Keep a short table of measured facts, code-supported inferences and unresolved assumptions. Choose the smallest USB route that can support one voice. If the interface controller prevents this, document the blocker before considering an internal digital connection; that would be a separate hardware-design decision.

**Go:** a specific route exists from external PCM to the pre-filter track buffer, with identified controller/firmware changes and a plausible timing budget.

**Stop or reassess:** only a MAIN mix is accessible and no practical per-track redirection is found. Do not spend time building the complete synth interface around that limitation.

## 3. Prove one complete external audio path

Create a minimal host sender and Digitakt receiver for one test tone. Use sequence numbers, bounded queues and counters for missing, late and discarded blocks. The audio callback copies ready samples; it never waits for USB or allocates memory.

Establish clock ownership from measurements. Pace rendering from the device stream where possible, and preserve resampler state across packets. Define reconnect/reset behavior and silence or fade on underrun/disconnection so stale audio cannot replay.

In the emulator, verify that moving the track filter and amp controls changes injected audio, and that stock tracks still work. On hardware, repeat with the actual USB path. Begin with silence, then a quiet tone; turn speakers down and retain the creator's fade-in and -12 dBFS test ceiling.

**Done when:** an external tone passes through one Digitakt track's filter/amp/sends on the real unit, other tracks work, and a ten-minute run records zero gaps/underruns. Log latency and jitter; those measurements decide whether the approach is musically acceptable.

## 4. Integrate SRC controls and sequencer events

Add a minimal external-machine descriptor named TRX-B2. Map SRC A–H to PTCH, DEC, RAMP, HOLD, TICK, NOIS, DIRT and DIST, retaining their 0–127 engine ranges.

Use semantic machine and voice events rather than forwarding every button press. Let Digitakt handle its ordinary navigation, filter and amp pages locally. Send the external host machine selection, voice start/retrigger, note/velocity policy, and synthesis parameter values after relevant locks/LFO processing. Verify the exact hook order instead of assuming step-fire events contain the final audible values.

Define a versioned control protocol with track ID, ordered events, stream generation and device-clock timestamps. A USB MIDI/SysEx carrier is an implementation option; using it internally does not mean dedicating a Digitakt MIDI track to the synth. Measure its throughput before choosing it.

Plan causal timing explicitly: a remote voice cannot return audio before receiving its trigger. Measure the round trip, decide whether sequencer lookahead or a fixed compensated delay is available, and measure live pad latency separately. Apply each parameter update to its intended audio interval and avoid adding smoothing on top of Digitakt's and TRX-B2's existing behavior without a reason.

**Done when:** SRC edits, per-step locks, retriggers, conditions and relevant LFO changes affect the external voice predictably; filter/amp controls continue to process the return locally.

## 5. Substitute TRX-B2 for the tone

Wrap the existing init/set_control/trigger/render API without changing synthesis arithmetic. Maintain a persistent voice instance. TRX-B2's native rate is **44.1 kHz**; resample to the measured device rate. For a 48 kHz stream the ratio is 160/147. Do not simply run the unchanged engine at 48 kHz.

Specify note and velocity behavior rather than guessing: the exposed engine API has controls and a trigger, so decide how note maps to PTCH and where velocity gain is applied, avoiding double scaling by the Digitakt pipeline. Compare default hits, extreme settings, control changes, long tails and rapid retriggers with the native reference. Measure host CPU and queue margins.

**Done when:** a Digitakt audio track plays external TRX-B2 from its own SRC page and sequencer, through its own filter/amp/effects, with acceptable measured timing. This is the first milestone.

## 6. Make it persistent, then scale

Verify machine identity and parameter saving across pattern/project save-load, copy/paste, undo and restart. Verify mute behavior, sound locks, pattern changes and sidecar reconnection. Keep per-track host state isolated and restore it from the Digitakt's current state after reconnect.

Scale from one voice to two, then eight only if transport channels, CPU and timing allow. Repeat the ten-minute test under dense locks, retriggers, LFO modulation, effects and UI activity. Record worst-case latency, jitter, queue depth, missed blocks and CPU load. Set the acceptable timing limits from the one-voice measurements before scaling.

Once the Pi/Mac architecture works, benchmark the Teensy implementation. Choose it only if it supports the proven transport, resampling and required voices with measured headroom.

## Immediate next work package

Complete steps 1–2 and write a short **audio-return decision** with the descriptor dump, Rytm/Digitakt differences, proposed insertion path and remaining controller unknowns. This is a bounded investigation, not a commitment to implement a full sidecar. If it yields no credible route, review the evidence before broadening into interface-controller firmware or internal wiring.
