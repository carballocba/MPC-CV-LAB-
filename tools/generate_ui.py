#!/usr/bin/env python3
"""Generate MPC skin input definitions. No device access, registration or installer."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'ui'/'native'
root.mkdir(parents=True,exist_ok=True)
params=[{'key':'sim_arm','name':'Simulation arm','min':0,'max':1,'default':0},
        {'key':'output_mask','name':'Output bitmask 0-255','min':0,'max':1,'default':15/255}]
options=['Off','Sine','Triangle','Gate','Sequence','Random hold','Mix','Slew','Quantizer']
for i in range(8):
    params.extend([{'key':f'n{i+1}_rate','name':f'Node {i+1} rate','min':0,'max':1,'default':((4 if i==2 else 1)-0.001)/63.999},
                   {'key':f'n{i+1}_scale','name':f'Node {i+1} scale','min':0,'max':1,'default':((1 if i in (4,5) else 5)+16)/32},
                   {'key':f'n{i+1}_type','name':f'Node {i+1} type','options':options,'default':[1,2,3,5,4,7,1,2][i]}])
for i in range(8):params.append({'key':f'cv{i+1}','name':f'CV {i+1} simulated volts','min':0,'max':1,'default':0})
params.extend([{'key':'playing','name':'Transport playing','min':0,'max':1,'default':0},
               {'key':'fault','name':'Backend fault','min':0,'max':1,'default':0}])
for i in range(8):params.append({'key':f'route{i+1}','name':f'Output {i+1} source','options':['Off']+[f'Node {n}' for n in range(1,17)],'default':i+1})
for i in range(8):params.append({'key':f'enable{i+1}','name':f'Output {i+1} enabled','min':0,'max':1,'default':1 if i<4 else 0})
(root/'params.json').write_text(json.dumps({'name':'MPC-CV LAB','params':params},indent=2)+'\n')
(root/'vst.json').write_text(json.dumps({'name':'MPC-CV LAB','vendor':'carballocba','uid':'McCv','version':100,'so':'mpc_cv.so','params':'params.json','layout':'layout.conf'},indent=2)+'\n')
lines=['theme_bg=151b24','theme_panel=151b24','theme_ink=edf4fa','theme_ink_dim=a2b3c4','theme_accent=62d9b5','qlink_bounds=column','[tab OVERVIEW]',
       'text cx=640 cy=120 label="MPC-CV / SIMULATION ONLY" size=2',
       'toggle cx=160 cy=600 label="SIM ARM" key=sim_arm',
       'readout cx=460 cy=600 w=180 h=40 label="TRANSPORT" key=playing',
       'readout cx=760 cy=600 w=180 h=40 label="FAULT" key=fault']
for i in range(8):lines.append(f'readout cx={160+(i%4)*310} cy={255+(i//4)*180} w=210 h=60 label="CV {i+1} / SIM V" key=cv{i+1}')
for i in range(8):lines.append(f'toggle cx={160+(i%4)*310} cy={325+(i//4)*180} bw=180 label="OUT {i+1} ON" key=enable{i+1}')
lines.append('qlinks "OVERVIEW" = '+','.join(f'enable{i+1}' for i in range(8))+',sim_arm,-,-,-')
lines+=['[tab ENGINES]','text cx=640 cy=120 label="VIRTUAL NODES / RATE - SCALE - TYPE" size=1.5']
for i in range(8):
    x=160+(i%4)*310;y=255+(i//4)*260
    lines.append(f'text cx={x} cy={y-65} label="NODE {i+1}" size=1.5')
    lines.append(f'knob cx={x-70} cy={y} r=28 bw=110 label="RATE" key=n{i+1}_rate')
    lines.append(f'knob cx={x+70} cy={y} r=28 bw=110 label="SCALE" key=n{i+1}_scale')
    lines.append(f'slider_h cx={x} cy={y+105} w=240 h=30 label="TYPE" key=n{i+1}_type')
for bank in range(2):lines.append(f'qlinks "NODES {bank*4+1}-{bank*4+4}" = '+','.join(f'n{i+1}_{key}' for i in range(bank*4,bank*4+4) for key in ['rate','scale','type','empty']).replace(f'n{bank*4+1}_empty','-').replace(f'n{bank*4+2}_empty','-').replace(f'n{bank*4+3}_empty','-').replace(f'n{bank*4+4}_empty','-'))
lines+=['[tab ROUTING]','text cx=640 cy=120 label="VIRTUAL SOURCE TO LOGICAL OUTPUT" size=1.5']
for i in range(8):lines.append(f'slider_h cx={160+(i%4)*310} cy={290+(i//4)*210} w=240 h=35 label="OUT {i+1} SOURCE" key=route{i+1}')
lines+=['qlinks "ROUTING" = '+','.join(f'route{i+1}' for i in range(8)),
        '[tab DEVICES]','text cx=640 cy=230 label="GENERIC UNIPOLAR / 0 TO 5 V" size=2',
        'text cx=640 cy=340 label="MODULE PROFILES: JSON TEMPLATE / NOT DEVICE CALIBRATION" size=1.3',
        'text cx=640 cy=450 label="NO PHYSICAL BACKEND" size=2',
        '[tab SCENES]','text cx=640 cy=230 label="SCENES USE HOST VST STATE CHUNKS" size=2',
        'text cx=640 cy=340 label="LOAD VALIDATES GRAPH / DISARMS SIMULATION" size=1.5',
        'text cx=640 cy=450 label="TOUCH SCENE BROWSER AND MORPH: PENDING" size=1.4',
        '[tab SETTINGS]','text cx=640 cy=160 label="OFFLINE BUILD / PHYSICAL OUTPUT ABSENT" size=1.8',
        'text cx=340 cy=360 label="OUTPUT SWITCHES: OVERVIEW" size=1.4',
        'readout cx=900 cy=360 w=250 h=60 label="FAULT" key=fault',
        'text cx=640 cy=520 label="FOUR OUTPUTS ENABLED BY DEFAULT" size=1.5',
        'qlinks "SETTINGS" = -,-,-,-']
(root/'layout.conf').write_text('\n'.join(lines)+'\n')
