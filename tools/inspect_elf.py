#!/usr/bin/env python3
"""Inspect ARM ELF32 build evidence using only the Python standard library."""
import json,struct,sys,hashlib
from pathlib import Path
p=Path(sys.argv[1]);b=p.read_bytes()
assert b[:6]==b'\x7fELF\x01\x01','expected ELF32 little-endian'
header=struct.unpack_from('<16sHHIIIIIHHHHHH',b)
_,kind,machine,_,_,_,shoff,flags,_,_,_,shentsize,shnum,shstrndx=header
assert kind==3 and machine==40 and flags&0x400,'expected ARM shared library with hard-float ABI'
sections=[struct.unpack_from('<IIIIIIIIII',b,shoff+i*shentsize) for i in range(shnum)]
def blob(s):return b[s[4]:s[4]+s[5]]
def string(data,offset):return data[offset:data.index(b'\0',offset)].decode()
names=blob(sections[shstrndx]);byname={string(names,s[0]):s for s in sections}
dyn=byname['.dynamic'];strings=blob(sections[dyn[6]])
needed=[]
for offset in range(dyn[4],dyn[4]+dyn[5],8):
    tag,value=struct.unpack_from('<II',b,offset)
    if tag==1:needed.append(string(strings,value))
sym=byname['.dynsym'];symstrings=blob(sections[sym[6]]);exports=[]
for offset in range(sym[4],sym[4]+sym[5],sym[9]):
    name,value,size,info,other,section=struct.unpack_from('<IIIBBH',b,offset)
    if name and section and info>>4 in (1,2):exports.append(string(symstrings,name))
assert 'VSTPluginMain' in exports
versions=[]
ver=byname.get('.gnu.version_r')
if ver:
    cursor=ver[4]
    while True:
        version,count,file,aux,next_record=struct.unpack_from('<HHIII',b,cursor)
        a=cursor+aux
        for _ in range(count):
            _,_,_,name,next_aux=struct.unpack_from('<IHHII',b,a)
            versions.append(string(strings,name));a+=next_aux
        if not next_record:break
        cursor+=next_record
print(json.dumps({'file':str(p),'sha256':hashlib.sha256(b).hexdigest(),'class':'ELF32','machine':'ARM','eabi':flags>>24,'hard_float':bool(flags&0x400),'needed':needed,'symbol_versions':sorted(set(versions)),'entry_exported':'VSTPluginMain' in exports,'telemetry_exported':'mpc_cv_read_telemetry' in exports},indent=2))
