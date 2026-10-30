# Upstream v0.3.0 migration

Status: desktop source port complete; Pi cross-build, reviewed candidate and
physical requalification remain open. The active dependency stays pinned to
v0.2.0 (`sources.json` `revision`); v0.3.0 is a separate `upgrade` target.
No installed Digitakt firmware, Pi kernel or SD changed. Existing private
recoveries remain valid.

The new-base series is [patches/sidecar-firmware-v0.3.0](../patches/sidecar-firmware-v0.3.0/README.md).
`python3 scripts/prepare_v030_source.py --output <new-directory>` stages clean
v0.3.0 source and replays 0001-0012 plus the portable overlay. `--x01` adds
the independent two-lane adapter. `python3 scripts/test_v030_replay.py` and
`python3 scripts/test_x01_host.py --v030` pass on desktop; [replay
evidence](../evidence/upgrades/v0.3.0-replay.json) and [X01
evidence](../evidence/upgrades/v0.3.0-x01.json) record their scope. The Pi 4
kernel cross-build was not run in this session because the Docker daemon was
unavailable; no build or hardware result is claimed.

The cross-build and private candidate helpers are ready:

```sh
python3 scripts/v030_build_pi.py --output /tmp/dtsc-v030-pi-build
python3 scripts/v030_prepare_candidate.py
```

The first command requires `rytm-circle` and Docker access (it writes only a
throwaway tree and `evidence/upgrades/v0.3.0-pi-build.json`). The second stages
`private/u01/v030-default/` with the built kernel, installed I04R-era v0.2
kernel as rollback, and hashes; it never writes an SD card. A human must review
the manifest and perform any card install.

Verified targets:

| Repository | v0.3.0 commit | Role here |
| --- | --- | --- |
| sidecar-firmware | 047873e630cade798f6b605ca0a46b5f86df5fb3 | Pi host integration must be ported |
| sidecar-mods | 71efeae5c091ce43659602c0919832e74c7f44e2 | Rytm reference; its release module targets Rytm MKII OS1.73, not Digitakt |

[Reproducible audit](../evidence/upgrades/v0.3.0-audit.json):
`python3 scripts/audit_upstream_upgrade.py` stages fetched release source in a
throwaway directory and tries our root patch stack in order. The first patch,
`0001-directional-overbridge-layout.patch`, fails at pi/engine/ob_usb.cpp.
The original v0.2 stack was ported into a separate v0.3.0 series; the firmware
change spans 863 files, including Makefile, build.sh, cores.cpp, ob_usb.cpp and
Output. This is a source migration, not replacing VERSION or installing the
generic upstream SD image.

Upstream Output now carries stereo planes, mixer/route state, a different Push/
Pull interface, and separate playback/restart underflow diagnostics. The latter
is useful for V01 interpretation, but it does not establish a fix for the
captured104ms late frame or dense physical freeze. Preserve both unresolved
issues and their current v0.2-based evidence; do not combine their baselines
with a new build's counters.

## Migration acceptance

1. Stage a new source tree at the target commit, preserving the dirty v0.2
   dependency and installed recoveries. Read applicable upstream instructions.
2. Port directional Digitakt USB framing and native return channel mapping.
   Rebase the DTSC adapter, explicit arm/mute, session epochs, timing trace,
   actual TRX-B2 scheduling/resampling and bounded queues onto the new render/
   Output lifecycle. Keep existing mdkit/elekloader pins unless separately
   reviewed; importing upstream vendored engines is not required by this bump.
3. Maintain replayable patches at the new base. Avoid restoring entire old
   Output/cores files over the new mixer. Port X01's independent two-mono-lane
   adapter separately, including fade, arm and stale-FIFO rejection checks.
4. Derive active base revisions from sources.json in build/replay scripts;
   preserve historical evidence pins. Replay from clean source, run native ABI/
   protocol/engine/session/Output tests, then cross-build the Pi4 kernel and
   inspect memory/stack costs. Do not update active pins based on a successful
   archive or standalone upstream build alone.
5. Prepare a reviewed candidate and exact recovery with hashes. Human handles
   any SD/firmware installation. Repeat physical baseline and V01 tests with
   low monitors and a disposable project before claiming compatibility.

Upstream references: [Pi release](https://github.com/trigcondition/sidecar-firmware/releases/tag/v0.3.0),
[Rytm modules](https://github.com/trigcondition/sidecar-mods/releases/tag/v0.3.0).
The Rytm .elemod/.syx must not be applied to the Digitakt. This upgrade does not
change our target of Digitakt mk1 OS1.54.
