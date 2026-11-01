#!/usr/bin/env python3
"""Build/run ARM Linux tests in a provisioned Linux environment; never access MPC."""
import hashlib,json,os,platform,shutil,subprocess,time
from pathlib import Path
root=Path(__file__).resolve().parents[1]
os.chdir(root)
out=root/'build'/'armv7-qemu';out.mkdir(parents=True,exist_ok=True)
cxx=os.environ.get('ARM_CXX','arm-linux-gnueabihf-g++')
qemu=os.environ.get('QEMU_ARM','qemu-arm')
sysroot=Path(os.environ.get('ARM_SYSROOT','/usr/arm-linux-gnueabihf'))
for command in [cxx,qemu]:
    if not shutil.which(command):raise SystemExit(f'Missing {command}; provision Linux tools separately')
if not (sysroot/'lib/ld-linux-armhf.so.3').is_file():raise SystemExit('Missing ARM hard-float dynamic loader in ARM_SYSROOT')
def run(args,timeout=120):
    print('+ '+' '.join(map(str,args)),flush=True)
    subprocess.run(list(map(str,args)),check=True,timeout=timeout)
flags=['-std=c++17','-O2','-UNDEBUG','-Wall','-Wextra','-Wpedantic','-Werror','-Iinclude','-pthread','-march=armv7-a','-mfpu=vfpv3-d16','-mfloat-abi=hard']
run([cxx,*flags,'-shared','-fPIC','src/vst/plugin.cpp','-o',out/'mpc_cv.so'])
for name in ['core_test','backend_test','graph_test','vst_host_test','rt_test']:
    sources=[f'tests/{name}.cpp']
    if name=='rt_test':sources.append('src/vst/plugin.cpp')
    run([cxx,*flags,*sources,'-ldl','-o',out/name])
run(['python3','tools/inspect_elf.py',out/'mpc_cv.so'])
results=[]
for name in ['core_test','backend_test','graph_test','vst_host_test','rt_test']:
    cmd=[qemu,'-cpu','cortex-a9','-L',str(sysroot),str(out/name)]
    if name=='vst_host_test':cmd.append(str(out/'mpc_cv.so'))
    start=time.monotonic();run(cmd)
    results.append({'test':name,'result':'PASS','seconds':round(time.monotonic()-start,3)})
report={'host':platform.platform(),'compiler':subprocess.check_output([cxx,'--version'],text=True).splitlines()[0],
        'qemu':subprocess.check_output([qemu,'--version'],text=True).splitlines()[0],
        'sysroot':str(sysroot),'cpu':'cortex-a9','tests':results,
        'plugin_sha256':hashlib.sha256((out/'mpc_cv.so').read_bytes()).hexdigest(),
        'limits':'QEMU user-mode and Ubuntu cross sysroot only; no Hakai, MPC, electrical or real-time deadline validation'}
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2),flush=True)
