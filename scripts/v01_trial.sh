#!/usr/bin/env bash
# Guided physical reproduction; never sends USB/network commands or flashes.
# Based on diagnosing-bugs/scripts/hitl-loop.template.sh. Captures go private.
set -euo pipefail
cd "$(dirname "$0")/.."
label=${1:?Usage: bash scripts/v01_trial.sh V01B-or-V01C 32-or-64}
level=${2:?Supply 32 or 64 (SPD and DEP)}
case "$label" in V01B|V01C) ;; *) exit 2 ;; esac
case "$level" in 32|64) ;; *) exit 2 ;; esac
package="private/v01/firmware-$label"
python3 - "$package" "$label" <<'PY'
import hashlib,json,sys
from pathlib import Path
p=Path(sys.argv[1]); label=sys.argv[2]
m=json.loads((p/'manifest.json').read_text())
assert m['label']==label
for name,want in {f'Digitakt-{label}-diagnostic.syx':m['candidate_sha256'],**m['recovery']}.items():
 assert hashlib.sha256((p/name).read_bytes()).hexdigest()==want,name
print('Verified candidate and recovery:',p)
print('Candidate SHA256:',m['candidate_sha256'])
PY
step() { printf '\n>>> %s\n' "$1"; read -r -p '    [Enter when done] ' _; }
capture() { local answer; printf '\n>>> %s\n' "$2"; read -r -p '    > ' answer; printf -v "$1" '%s' "$answer"; }
capture ready 'Pi/Digitakt available, monitors lowered, known routing, disposable project backed up, and recovery ready? Type yes to proceed.'
[[ "$ready" == yes ]] || exit 1
step "If needed, use Transfer to install $package/Digitakt-$label-diagnostic.syx. Human flash only; do not change bootloader/wiring. Keep Pi voice muted and trxtrace off."
capture installed "Confirm the installed firmware label ($label)."
[[ "$installed" == "$label" ]] || exit 1
step 'Freshly reboot the Pi for clean counter baselines. Leave the voice muted. Read trxstatus/status without dumping a trace.'
capture pi_before 'Paste starting FIFO underrun, playout fault and USB gap/error counters, plus Pi build/uptime.'
step 'On disposable track 1, select a stock machine, wait a second, then select USB1 and wait a second. This starts a new measurement window. Do not turn diagnostic-page knobs.'
step "Set PTCH LFO SPD $level / DEP $level, MULT BPM16, triangle, free mode, SPH 0. Keep FADE and the pattern identical between candidates, C5/velocity 100. Stop immediately if setting/saving is sluggish or a Pi fault appears."
capture settings 'Record FADE, tempo/pattern density, any setting/save delay and whether a stop condition already occurred.'
step 'Open V01 DIAG. Read SECS; use an independent stopwatch for 10 seconds, then read SECS again. Stop here if that does not advance about 10 seconds. Read status before any trace dump.'
capture clock 'Record stopwatch elapsed seconds and SECS before/after.'
capture before 'Record SECS/UI/WORK/WGAP/BATCH/UGAP/FAULT before Play.'
step 'Only if no stop condition occurred: Play for up to 10 seconds and Stop. Stop at the first visible lag, new Pi fault or underrun. Keep voice muted for this isolation trial.'
capture after 'Record final SECS/UI/WORK/WGAP/BATCH/UGAP/FAULT and observed Play/Stop delays.'
capture pi_after 'Paste ending FIFO underrun, playout fault, USB gap/error counters and any retained fault.'
capture verdict 'Outcome: reproduced lag, clean short trial, or inconclusive? Include recovery needed. A muted trial is not an audible pass.'
export V01_LABEL="$label" V01_LEVEL="$level" V01_READY="$ready" V01_PI_BEFORE="$pi_before"
export V01_SETTINGS="$settings" V01_CLOCK="$clock" V01_BEFORE="$before" V01_AFTER="$after"
export V01_PI_AFTER="$pi_after" V01_VERDICT="$verdict"
python3 - <<'PY'
import datetime,json,os
from pathlib import Path
now=datetime.datetime.now(datetime.timezone.utc)
record={'task':'V01','scope':'human-operated muted hardware isolation; raw observations, not an automatic pass'}
for key in ('label','level','ready','pi_before','settings','clock','before','after','pi_after','verdict'):
 record[key]=os.environ['V01_'+key.upper()]
p=Path('private/v01')/('trial-'+record['label']+'-'+record['level']+'-'+now.strftime('%Y%m%dT%H%M%SZ')+'.json')
record['date_utc']=now.isoformat();p.write_text(json.dumps(record,indent=2)+'\n')
print('Captured:',p)
PY
