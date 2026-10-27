#!/usr/bin/env python3
"""Match OS 1.54 mode-table descriptors to the captured physical endpoints."""
import argparse
import json
from pathlib import Path
import struct

from r07_handoff_map import BASE, MAIN_SHA, load_main


def facts(sections, captured):
    image = load_main(sections)
    physical = json.loads(Path(captured).read_text())
    rows = []
    for mode in (2, 4, 5, 6):
        table = 0x401f5a00 + 40*mode
        fields = struct.unpack_from('>10I', image, table-BASE)
        device = image[fields[2]-BASE:fields[2]-BASE+18]
        vid, pid = struct.unpack_from('<HH', device, 8)
        config = image[fields[3]-BASE:fields[3]-BASE+fields[4]]
        endpoints = []
        pos = 0
        interface = alternate = None
        while pos < len(config):
            length, kind = config[pos:pos+2]
            assert length >= 2 and pos+length <= len(config)
            if kind == 4:
                interface, alternate = config[pos+2:pos+4]
            elif kind == 5:
                ep, attr = config[pos+2:pos+4]
                mps = struct.unpack_from('<H', config, pos+4)[0]
                endpoints.append({'interface':interface, 'alternate':alternate,
                                  'endpoint':f'0x{ep:02x}', 'type':{2:'bulk',3:'interrupt',1:'isochronous'}[attr & 3],
                                  'wMaxPacketSize':mps,'bInterval':config[pos+6]})
            pos += length
        rows.append({'mode':mode,'table_address':hex(table),'initialize':hex(fields[1]),
                     'vendor_id':hex(vid),'product_id':hex(pid),
                     'configuration_bytes':fields[4],'endpoints':endpoints})
    keys = ('interface','alternate','endpoint','type','wMaxPacketSize','bInterval')
    signature = lambda endpoints: sorted(tuple(e[k] for k in keys) for e in endpoints)
    matches = [r['mode'] for r in rows if signature(r['endpoints']) == signature(physical['endpoints'])]
    assert matches == [6], matches
    assert next(r for r in rows if r['mode']==6)['product_id'] == '0xb2c'
    return {'task':'H02','pass':True,'scope':'static pinned OS descriptors matched to earlier physical R02 capture; no live RAM read',
            'main_sha256':MAIN_SHA,'physical_capture':str(captured),'matching_mode':6,
            'old_guard_mode':2,'mode_rows':[{k:v for k,v in r.items() if k!='endpoints'} for r in rows],
            'hardware_diagnostic':{'RCLK':'increments, wraps in about 7 seconds','TICK':'about 30/s',
                                   'STATE':20,'SYNC':1,'MIDI':125,'TX':0,'RX':'increases by about 13 every 6 seconds'},
            'conclusion':'STATE 20 rejects mode==2; mode 6 uniquely matches physical endpoint descriptors. The route guard/model used the wrong USB mode.'}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sections', type=Path, required=True)
    p.add_argument('--captured', type=Path, default=Path('evidence/r02-endpoints-2026-10-26.json'))
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    result = facts(a.sections, a.captured)
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(result, indent=2)+'\n')
    print('OS 1.54 mode 6 uniquely matches physical Digitakt endpoints: PASS')
