# Reference triage and R03 next step

26 October 2026. Scope: brief first-party README review of the newly supplied MCP repository, Elektroid and the current elk-herd GitHub landing page, plus the local R03 report and pinned emulator documentation. Other listed projects have not been implementation-audited here.

## Applicability

| Resource | Useful role | Does it release R03? |
| --- | --- | --- |
| [jamiew/digitakt-digitone-mcp](https://github.com/jamiew/digitakt-digitone-mcp) | Original-Digitakt MIDI triggering and parameter-control reference; potentially an optional agent-facing hardware test harness. Verify its CC/NRPN maps against our OS/manual before use. | No. Its README describes MIDI operation, not emulator runtime repair or pre-filter PCM injection. |
| mido / python-rtmidi | Potential transport libraries for a small MIDI test harness. The MCP repository lists these as its I/O dependencies. | No. Installing a MIDI library does not establish a sample assignment, guest execution or audio buffer routing. |
| Official manual and pencilresearch/midi | Parameter-map references for later filter/amp test automation. Prefer the manual, use the database as a cross-check and verify revision/attribution before copying data. | Indirect support only; a map is not evidence that a command was applied or audio was rendered. |
| [dagargo/elektroid](https://github.com/dagargo/elektroid) | Its README lists Digitakt support, sample upload/download, project/sound transfer and SysEx operations. Useful later for hardware fixtures or protocol research. | Not the immediate fix. The emulator already supplies a WAV/sample-loading path; an external transfer stack would add unnecessary variables. |
| [mzero/elk-herd](https://github.com/mzero/elk-herd) | The supplied MCP credits cite it for pattern/project layout. Its current GitHub README is a migration notice, rather than the implementation; this investigation did not follow or audit the migrated source. | No demonstrated solution from the reviewed material. |
| Other MCP/OSC projects in the supplied credits | Possible architecture/API references for future automation, based on the listed descriptions. Not independently audited. | No evidence of a solution to USB→ColdFire handoff or pre-filter injection. |

MCP is an agent interface around operations the underlying instrument/library already exposes. It does not by itself add realtime audio channels, guest firmware hooks or access to internal audio buffers. No new MCP dependency is needed for the next R03 test.

## Current R03 state

The latest [R03 report](R03-emulator-insertion.md) supersedes the initial runtime-blocker entry. A host-only workaround passes compatibility checks and boots 1.53. It is temporary and not a reproducible tracked runtime solution. Prefer validating the pinned patched runtime outside the sandbox before relying on that workaround. Do not treat an arbitrary fixed cache-line binary patch as an accepted dependency.

The remaining experiment has not reached nonzero stock audio: the test track has no active sample/source. A zero-output memory poke cannot establish whether the insertion point is valid.

## Exact next assignment for the R03 owner

1. Establish a reproducible runtime: run `emu.unicorn_compat` using the intended patched Unicorn outside the restrictive sandbox where permitted; retain logs and exact library provenance. If it still fails, record that separately from the guest-audio experiment.
2. Generate a small known WAV locally. Use the pinned emulator's existing `emu/samples.py` / LOAD SAMPLES flow to place it on a private test +Drive, then rebuild/reboot as documented so the firmware refreshes its file index. Merely copying a file or writing memory does not assign a sample to a track.
3. Through stock UI or a reproducible guest-control script, load the sample into the project, assign it to track 1, ensure level/amp/mutes permit playback, and trigger it. Prove nonzero captured PCM with sample count, peak/RMS and a repeatable run.
4. Only once stock audio is established, hook the observed post-source/pre-filter execution point and inject known PCM each block. Demonstrate its signal in output, sweep filter/amp and retain a second stock track as a regression control.
5. Label 1.53 emulator evidence separately from the 1.54 target. Finish R03 only when its existing acceptance criteria pass; otherwise report the narrowest remaining failure.

Relevant local references: [sample loading instructions](../deps/digiemu/README.md#loading-samples), [sample loader](../deps/digiemu/emu/samples.py), [R03 evidence](R03-emulator-insertion.md). This is a continuation for the existing task owner, not a reassignment.

In parallel, H01 can establish physical stock USB streaming. It does not replace R03 or resolve the still-untraced USB→ColdFire handoff. D01 needs to account for both the insertion evidence and that unresolved boundary.
