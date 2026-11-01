#!/usr/bin/env python3
"""Control-plane JSON scene/preset authoring; no hardware and no RT dependencies."""
import argparse,copy,json,math,os,struct,tempfile
from pathlib import Path
KINDS=['off','sine','triangle','gate','sequence','random_hold','mix','slew','quantizer']
NODE_DEFAULT={'type':'off','input':None,'modulator':None,'cycles_per_beat':1.0,'scale':1.0,'offset_volts':0.0,
              'modulation_amount':0.0,'slew_volts_per_second':10.0,'quantum_volts':1/12,
              'steps_volts':[0,1,2,3,4,3,2,1],'seed':1}
ROUTE_DEFAULT={'source':None,'scale':1.0,'offset_volts':0.0,'minimum_volts':0.0,'maximum_volts':5.0}
def require(test,message):
    if not test:raise ValueError(message)
def fields(obj,allowed):
    require(type(obj) is dict,'expected object')
    require(not set(obj)-set(allowed),'unknown fields: '+str(set(obj)-set(allowed)))
def real(value,low,high):
    require(type(value) in (int,float) and math.isfinite(value) and low<=value<=high,'invalid numeric range')
    return float(value)
def integer(value,low,high):
    require(type(value) is int and low<=value<=high,'invalid integer range')
    return value
def ref(value,count):
    return None if value is None else integer(value,0,count-1)
def validate(scene):
    fields(scene,['schema_version','kind','enabled_mask','sync','controls','nodes','routes'])
    require(scene.get('schema_version')==1 and type(scene.get('schema_version')) is int,'unsupported scene version')
    require(scene.get('kind')=='performance_scene','expected performance_scene')
    nodes=scene.get('nodes',[]);require(type(nodes) is list and len(nodes)<=16,'at most 16 nodes')
    count=len(nodes);result={'schema_version':1,'kind':'performance_scene','enabled_mask':integer(scene.get('enabled_mask',15),0,255),
                           'sync':scene.get('sync',True),'controls':scene.get('controls',[None]*4),'nodes':[],'routes':[]}
    require(result['sync'] is True,'only host sync is implemented')
    require(type(result['controls']) is list and len(result['controls'])==4,'four control assignments required')
    result['controls']=[ref(v,count) for v in result['controls']]
    for raw in nodes:
        fields(raw,NODE_DEFAULT);n={**copy.deepcopy(NODE_DEFAULT),**raw}
        require(n['type'] in KINDS,'unknown engine type')
        n['input']=ref(n['input'],count);n['modulator']=ref(n['modulator'],count)
        for key,low,high in [('cycles_per_beat',.001,64),('scale',-16,16),('offset_volts',-20,20),
                             ('modulation_amount',-16,16),('slew_volts_per_second',.001,10000),('quantum_volts',.001,10)]:
            n[key]=real(n[key],low,high)
        n['seed']=integer(n['seed'],0,0xffffffff)
        require(type(n['steps_volts']) is list and len(n['steps_volts'])==8,'eight step values required')
        n['steps_volts']=[real(v,-20,20) for v in n['steps_volts']];result['nodes'].append(n)
    done=set()
    for _ in range(count):
        ready=[i for i,n in enumerate(result['nodes']) if i not in done and all(n[k] is None or n[k] in done for k in ['input','modulator'])]
        require(bool(ready),'modulation cycle');done.add(ready[0])
    routes=scene.get('routes',[{} for _ in range(8)])
    require(type(routes) is list and len(routes)==8,'eight output routes required')
    for raw in routes:
        fields(raw,ROUTE_DEFAULT);r={**ROUTE_DEFAULT,**raw};r['source']=ref(r['source'],count)
        for key,low,high in [('scale',-16,16),('offset_volts',-20,20),('minimum_volts',0,5),('maximum_volts',0,5)]:r[key]=real(r[key],low,high)
        require(r['minimum_volts']<=r['maximum_volts'],'reversed route range');result['routes'].append(r)
    return result
def fnv(data):
    value=2166136261
    for byte in data:value=((value^byte)*16777619)&0xffffffff
    return value
def encode(scene):
    s=validate(scene);p=bytearray(struct.pack('<IBBB',1,len(s['nodes']),s['enabled_mask'],1))
    p.extend(255 if v is None else v for v in s['controls'])
    for n in s['nodes']:
        p.extend(struct.pack('<BBBI',KINDS.index(n['type']),255 if n['input'] is None else n['input'],255 if n['modulator'] is None else n['modulator'],n['seed']))
        p.extend(struct.pack('<14d',*[n[k] for k in ['cycles_per_beat','scale','offset_volts','modulation_amount','slew_volts_per_second','quantum_volts']],*n['steps_volts']))
    for r in s['routes']:p.extend(struct.pack('<B4d',255 if r['source'] is None else r['source'],*[r[k] for k in ['scale','offset_volts','minimum_volts','maximum_volts']]))
    return b'MCVS'+struct.pack('<II',len(p),fnv(p))+p
def decode(data):
    require(23<=len(data)<=4096 and data[:4]==b'MCVS','invalid scene header/size')
    length,hash_value=struct.unpack_from('<II',data,4)
    require(length==len(data)-12 and hash_value==fnv(data[12:]),'scene length/checksum mismatch')
    cursor=12
    def take(fmt):
        nonlocal cursor
        size=struct.calcsize(fmt);require(cursor+size<=len(data),'truncated scene')
        values=struct.unpack_from(fmt,data,cursor);cursor+=size;return values
    version,count,enabled,sync=take('<IBBB');require(count<=16 and sync==1,'unsupported scene count/sync')
    s={'schema_version':version,'kind':'performance_scene','enabled_mask':enabled,'sync':True,
       'controls':[None if v==255 else v for v in take('<4B')],'nodes':[],'routes':[]}
    for _ in range(count):
        kind,source,mod,seed=take('<BBBI');require(kind<len(KINDS),'unknown engine kind')
        values=take('<14d');n=dict(zip(['cycles_per_beat','scale','offset_volts','modulation_amount','slew_volts_per_second','quantum_volts'],values[:6]))
        n.update(type=KINDS[kind],input=None if source==255 else source,modulator=None if mod==255 else mod,seed=seed,steps_volts=list(values[6:]));s['nodes'].append(n)
    for _ in range(8):
        source,scale,offset,low,high=take('<B4d');s['routes'].append({'source':None if source==255 else source,'scale':scale,'offset_volts':offset,'minimum_volts':low,'maximum_volts':high})
    require(cursor==len(data),'trailing scene bytes');return validate(s)
def apply_preset(scene,preset,node):
    s=validate(scene);fields(preset,['schema_version','kind','name','engine'])
    require(type(preset.get('schema_version')) is int and preset['schema_version']==1 and preset.get('kind')=='engine_preset','unsupported preset')
    integer(node,0,len(s['nodes'])-1)
    s['nodes'][node]=copy.deepcopy(preset.get('engine'))
    return validate(s)
def read_limited(path,limit):
    with Path(path).open('rb') as stream:data=stream.read(limit+1)
    require(len(data)<=limit,'file exceeds size limit');return data
def read_json(path):
    data=read_limited(path,65536)
    def unique(pairs):
        obj={}
        for k,v in pairs:
            require(k not in obj,'duplicate JSON key: '+k);obj[k]=v
        return obj
    def constant(value):raise ValueError('nonfinite JSON constant: '+value)
    return json.loads(data,object_pairs_hook=unique,parse_constant=constant)
def atomic_write(path,data):
    path=Path(path);fd,name=tempfile.mkstemp(prefix=path.name+'.',dir=path.parent)
    try:
        with os.fdopen(fd,'wb') as stream:stream.write(data);stream.flush();os.fsync(stream.fileno())
        os.replace(name,path)
    finally:
        if os.path.exists(name):os.unlink(name)
def main():
    parser=argparse.ArgumentParser(description=__doc__);commands=parser.add_subparsers(dest='command',required=True)
    for name in ['compile','decode']:
        cmd=commands.add_parser(name);cmd.add_argument('input');cmd.add_argument('output')
    cmd=commands.add_parser('preset');cmd.add_argument('scene');cmd.add_argument('preset');cmd.add_argument('node',type=int);cmd.add_argument('output')
    args=parser.parse_args()
    try:
        if args.command=='compile':data=encode(read_json(args.input))
        elif args.command=='decode':data=(json.dumps(decode(read_limited(args.input,4096)),indent=2,allow_nan=False)+'\n').encode()
        else:data=(json.dumps(apply_preset(read_json(args.scene),read_json(args.preset),args.node),indent=2,allow_nan=False)+'\n').encode()
        atomic_write(args.output,data)
    except (ValueError,OSError,TypeError,struct.error) as error:parser.exit(2,str(error)+'\n')
if __name__=='__main__':main()
