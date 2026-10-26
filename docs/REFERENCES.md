# Project reference index

These references are retained for the whole project. Consult them when the matching task needs them; listing a project does not make it a dependency or verify its implementation. Prefer official documentation and measured OS 1.54 behavior when references disagree.

## Integration and audio transport

| Reference | When to consult | Evidence status |
| --- | --- | --- |
| [Sidecar firmware](https://github.com/trigcondition/sidecar-firmware) | I01/H01: host USB streaming, buffering, external synthesis and Pi implementation | Pinned locally in sources.json; baseline and directional-layout evidence recorded |
| [Sidecar Rytm mods](https://github.com/trigcondition/sidecar-mods) | R01/R04/I02/I03: reference architecture for semantic events, remote pages and per-voice returns | Pinned reference; Rytm behavior does not establish Digitakt behavior |
| [Creator's Digitakt guide](DIGITAKT-GUIDE.md) | Host setup and stock-firmware transport experiments | Captured upstream snapshot; explicitly excludes our deeper firmware integration |
| [Overwitch](https://github.com/dagargo/overwitch) | USB framing, channel direction and device-layout research | Device table reviewed; R02 physically captured endpoint capacities; live stream semantics still need validation |

## Controls, automation and user interface

| Reference | Project-wide use | Evidence status |
| --- | --- | --- |
| [Digitakt manual OS 1.52A](https://www.elektron.se/wp-content/uploads/2025/07/Digitakt-User-Manual_ENG_OS1.52A_250708.pdf) | R04/I03/V01: stock parameter meanings, CC/NRPN, routing and sequencing behavior | Official reference reviewed; check differences against target OS 1.54 |
| [jamiew/digitakt-digitone-mcp](https://github.com/jamiew/digitakt-digitone-mcp) | Optional agent-driven hardware test harness: triggers, parameter sweeps and repeatable MIDI scenarios | README reviewed; code and timing guarantees not audited; not the realtime DSP/control-loop implementation |
| [mido](https://github.com/mido/mido), [python-rtmidi](https://github.com/SpotlightKid/python-rtmidi) | MIDI I/O candidates for small host test tools and internal-protocol experiments | Listed by supplied MCP project; not independently audited here |
| [pencilresearch/midi](https://github.com/pencilresearch/midi), [midi.guide Elektron](https://midi.guide/d/elektron/) | Cross-check parameter numbers and ranges when implementing or testing controls | Supplied references; maps not verified here; check attribution/licence before copying data |
| [feamster/digitakt-midi-mcp](https://github.com/feamster/digitakt-midi-mcp) | Prior art for Digitakt automation and test-tool design | Supplied reference; not audited |
| [roomi-fields/osc-bridge](https://github.com/roomi-fields/osc-bridge) | Consider declarative per-device descriptions if maintaining multiple host/device layouts | Supplied reference; not audited |
| [TheAndrewStaker/mcp-midi-control](https://github.com/TheAndrewStaker/mcp-midi-control) | Consider descriptor-driven agent APIs if we later expose project tools through MCP | Supplied reference; not audited |
| [elektron-mcp](https://github.com/ftupas/elektron-mcp) | Compare device parameter coverage if a multi-device automation harness becomes useful | Supplied credits call it zerubeus/elektron-mcp but link here; current identity and implementation not audited |
| [MCP specification](https://modelcontextprotocol.io/), [Python SDK](https://github.com/modelcontextprotocol/python-sdk) | Only when implementing an agent-facing tool server for testing or operation | Supplied references; no MCP server required for the sidecar milestone |

## Persistence, samples and protocol research

| Reference | Project-wide use | Evidence status |
| --- | --- | --- |
| [Elektroid](https://github.com/dagargo/elektroid) | V01 and hardware fixtures: sample/project/sound transfers, device identification and SysEx behavior | README reviewed; relevant implementation must be checked before reuse; GPLv3 |
| [elk-herd](https://github.com/mzero/elk-herd) | Potential reference for pattern/project layouts, backup and persistence tests | Supplied credits cite it; current GitHub landing page is a migration notice, not audited implementation |
| [Elektronauts](https://www.elektronauts.com/) | Find experimental leads, quirks and reproduction reports | Discovery source; follow claims to official docs, code or our own measurements before treating them as facts |

Project/sound transfer knowledge may support persistence testing, but does not prove where added machine state can safely be stored. Likewise external MIDI control does not prove that the Digitakt transmits the final sequencer locks/LFO values needed by the sidecar. Those require our own integration evidence.

## Engine and emulator sources

The pinned `mdkit`, `digiemu` and `elekloader` repositories in sources.json remain the primary local starting points for TRX-B2 behavior, emulator execution and Digitakt machine hooks. See the task-specific reports for verified symbols, runtime limitations and test results.

Before adopting external code, verify its revision, licence and compatibility with the destination repository. Record the dependency and replayable changes under the existing AGENTS workflow. For the immediate R03 experiment, see [reference triage](REFERENCE-TRIAGE.md).
