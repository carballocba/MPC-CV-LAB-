#!/usr/bin/env python3
import csv,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'build'
rows=list(csv.DictReader((root/'eight-outputs.csv').open()))
data=(root/'capture.bin').read_bytes()
assert len(rows)==1379 and int(rows[-1]['sample'])==176399
assert len(data)==(len(rows)+1)*48
for i in range(len(rows)+1):
    for j in range(8):
        status,msb_cc,msb,status2,lsb_cc,lsb=data[i*48+j*6:i*48+j*6+6]
        assert status==status2==0xbf and msb_cc==j*2 and lsb_cc==j*2+1
        assert msb<128 and lsb<128
        value=msb*128+lsb
        assert value==(int(rows[i][f'cv{j+1}']) if i<len(rows) else 0)
print('PASS: 1379 eight-output snapshots and final safe-zero packet; all CC bytes match CSV')
