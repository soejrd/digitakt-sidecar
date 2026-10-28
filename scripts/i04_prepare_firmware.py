#!/usr/bin/env python3
"""Verify/stage private I04Q firmware, previous H02F and stock recovery. No flash."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(root/'deps/elekloader'))
from elekloader.syx import Syx,verify
from elekloader.devices.digitakt_mk1 import DEVICE
sha=lambda data:hashlib.sha256(data).hexdigest()
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--candidate',default='private/i04-effective')
ap.add_argument('--label',choices=['I04Q','I04R'],default='I04Q')
args=ap.parse_args()
label=args.label
folder=root/args.candidate
meta=json.loads((folder/'fixture.json').read_text())
for name,expected in meta['source_hashes'].items():
    assert sha((root/name).read_bytes())==expected, 'stale candidate: '+name
raw=(folder/'artifacts/I02.syx').read_bytes()
assert sha(raw)==meta['syx_sha256'] and meta['firmware_label']==label
stock=Syx((root/'private/i02-fixture/Digitakt_OS1.54.syx').read_bytes())
new=Syx(raw)
facts=verify(raw,stock,new.section(3),DEVICE,version=label)
old=(root/'private/h02/trial/Digitakt-H02F-mode6.syx').read_bytes()
assert sha(old)==json.loads((root/'evidence/h02/mode6-fix.json').read_text())['firmware']['sha256']
recovery=(root/'private/h02/trial/Digitakt-OS1.54-stock-recovery.syx').read_bytes()
assert sha(recovery)==stock.sha256
manifest={'label':label,'scope':'private, verified candidate only; not installed',
          'candidate_sha256':sha(raw),'previous_H02F_sha256':sha(old),
          'stock_recovery_sha256':sha(recovery),'verification':facts,
          'source_hashes':meta['source_hashes'],
          'checks':['compiled boot and assignment','all eight native SRC encoder edits','compiled native transport/ACK/loss/recovery'],
          'human_gate':'Before installation, confirm saved/backed-up projects/sounds, recovery access and transfer connection. Speakers lowered, Pi voice muted.'}
files={('Digitakt-I04Q-effective-controls.syx' if label=='I04Q' else 'Digitakt-I04R-event-worker.syx'):raw,'Digitakt-H02F-previous.syx':old,
       'Digitakt-OS1.54-stock-recovery.syx':recovery,
       'manifest.json':(json.dumps(manifest,indent=2)+'\n').encode()}
if label=='I04R':
    ui=json.loads((root/'evidence/i04/event-worker-ui.json').read_text())['ui_emulator']
    runtime=json.loads((root/'evidence/i04/event-worker-runtime.json').read_text())
    exchange=json.loads((root/'evidence/i04/event-worker-exchange.json').read_text())
    assert ui['candidate_sha256']==sha(raw) and len(ui['panel_knobs_changed'])==8
    assert runtime['candidate_sha256']==sha(raw) and runtime['status']=='verified'
    assert exchange['candidate']['syx_sha256']==sha(raw) and exchange['status']=='verified'
    previous=(root/'private/i04/firmware-I04Q/Digitakt-I04Q-effective-controls.syx').read_bytes()
    expected=json.loads((root/'evidence/i04/firmware-trial-approval.json').read_text())['candidate_sha256']
    assert sha(previous)==expected
    verify(previous,stock,Syx(previous).section(3),DEVICE,version='I04Q')
    files['Digitakt-I04Q-previous.syx']=previous
    manifest['previous_I04Q_sha256']=expected
    manifest['checks']+=['native RTOS genuine sleep/wake and stack guards','dense every-block native exchange']
    files['manifest.json']=(json.dumps(manifest,indent=2)+'\n').encode()
out=root/('private/i04/firmware-'+label)
for name,data in files.items():
    p=out/name
    assert not p.exists() or p.read_bytes()==data,'existing different artifact: '+name
for name,data in files.items():
    p=out/name; p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
    assert p.read_bytes()==data
print(label+' firmware verified; stock protected sections/layout, source pins and previous/stock recovery staged. No hardware flash.')
