#!/usr/bin/env python3
"""Check the actual VST->producer queue->worker->capture trace; no device IO."""
import csv,sys
from pathlib import Path
path=Path(sys.argv[1]) if len(sys.argv)>1 else Path('build/vst-capture.csv')
rows=list(csv.DictReader(path.open()))
assert rows
sequence=0;last_event_sample=-1;outputs=set();gate=[];final={};prime={}
for row in rows:
    seq,sample,role,out,code=(int(row[key]) for key in ['sequence','sample','role','output','code'])
    assert seq==sequence or seq==sequence+1
    sequence=seq;assert 1<=out<=8 and 0<=code<=16383
    outputs.add(out)
    if role==0:prime[out]=code
    elif role==2:final[out]=code
    else:
        assert role==1 and sample>=last_event_sample
        last_event_sample=sample
        if out==3 and sample<176400:gate.append((sample,code))
assert outputs==set(range(1,9))
assert prime==final==dict.fromkeys(range(1,9),0)
assert len(gate)==64,len(gate)
for index,(sample,code) in enumerate(gate):
    assert code==(16383 if index%2==0 else 0)
    assert abs(sample-index*44100/16)<=2,(index,sample)
print(f'PASS: actual VST trace, {sequence} contiguous packets, eight outputs, 64 timed gate edges and final zeros')
