#!/usr/bin/env python3
"""Reproduce the captured late frame from public timing-only metadata."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    sources = ['tools/v01_replay_timing.c', 'audio/playout.c', 'audio/trx_voice.c',
               'audio/src_44148.c', 'deps/mdkit/src/trx/b2/mdkit_trx_b2.c']
    binary = ROOT/'build/v01/timing-replay'
    binary.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run(['cc','-std=c99','-O1','-g','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-DSC_PLAYOUT_DELAY=128u','-Icontrol','-Iaudio',
        '-Ideps/mdkit/include','-Ideps/mdkit/src/trx/b2',*sources,'-lm','-o',str(binary)],
        cwd=ROOT,check=True)
    fixture = ROOT/'evidence/v01/reload-clock-replay.txt'
    result = subprocess.run([str(binary),str(fixture)],capture_output=True,text=True)
    assert result.returncode==1,result.stdout+result.stderr
    observed=json.loads(result.stdout)
    expected={'rejected_sequence':107,'accepted':106,'code':3,'clock':222204,
              'due':217216,'delta_blocks':64}
    assert observed==expected,observed
    report={'scope':'actual playout/engine replay of hardware-derived timing metadata; ASan/UBSan desktop; no live hardware',
        'command':'python3 scripts/test_v01_timing_replay.py',
        'observed':observed,'reproduced_hardware_late_fault':True,
        'source_sha256':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
                        for name in sources+['scripts/test_v01_timing_replay.py',
                                             'evidence/v01/reload-clock-replay.txt']},
        'limits':['Controls/note/velocity replaced by safe defaults; only sequence/block/install-clock timing retained.',
                  'Confirms the playback deadline guard reacts to this timing pattern; not a fix.',
                  'Does not distinguish native clock pause from delayed control delivery.'],
        'replay_pass':True,'qualification_pass':False}
    (ROOT/'evidence/v01/reload-timing-replay.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Hardware timing replay: exact SC_PF_LATE at sequence107 reproduced; deadline guard preserved.')


if __name__=='__main__':main()
