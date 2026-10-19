#!/usr/bin/env python3
"""Validate authored asset contracts, IDs, Q-Link references and profile semantics."""
import json,math,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def finite(x):return isinstance(x,(int,float)) and not isinstance(x,bool) and math.isfinite(x)
profile=json.loads((root/'profiles/generic-unipolar.json').read_text())
assert profile['schema_version']==1
for key in ['id','name','verification']:assert isinstance(profile[key],str) and profile[key]
assert 1<=len(profile['inputs'])<=32
ids=set()
for item in profile['inputs']:
    assert item['id'] not in ids;ids.add(item['id'])
    assert item['label'] and item['signal_type'] in ['continuous','pitch','gate','trigger']
    assert item['polarity'] in ['unipolar','bipolar']
    low,high=item['minimum_volts'],item['maximum_volts']
    assert finite(low) and finite(high) and low<high
    threshold=item['threshold_volts'];assert threshold is None or finite(threshold) and low<=threshold<=high
    assert finite(item['scale']) and item['scale']!=0 and finite(item['offset_volts'])
    output=item['default_logical_output'];assert output is None or type(output) is int and 1<=output<=8
    source=item['default_virtual_source'];assert source is None or isinstance(source,str) and source
preset=json.loads((root/'presets/sine.engine.json').read_text())
assert preset['kind']=='engine_preset' and preset['schema_version']==1
assert preset['engine']['type']=='sine' and 0<preset['engine']['cycles_per_beat']<=64
params=json.loads((root/'ui/native/params.json').read_text())['params']
keys=[p['key'] for p in params];assert len(keys)==44 and len(set(keys))==44
layout=(root/'ui/native/layout.conf').read_text()
assert re.findall(r'\[tab (.*?)\]',layout)==['OVERVIEW','ENGINES','ROUTING','DEVICES','SCENES','SETTINGS']
for key in re.findall(r'\bkey=([^\s]+)',layout):assert key in keys
for line in layout.splitlines():
    if line.startswith('qlinks '):
        entries=line.split('=',1)[1].strip().split(',');assert len(entries)<=16
        for k in entries:assert k=='-' or k in keys
print('PASS: generic profile, preset fixture, 44 parameters, six pages and Q-Link bindings')
