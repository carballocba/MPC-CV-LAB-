#!/usr/bin/env python3
"""Build/run ARM Linux tests in a provisioned Linux environment; never access MPC."""
import argparse,hashlib,json,os,platform,shutil,subprocess,time
from pathlib import Path
root=Path(__file__).resolve().parents[1]
os.chdir(root)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--existing-dir',type=Path)
parser.add_argument('--compiler-label',default='Zig 0.15.2')
args=parser.parse_args()
out=(args.existing_dir or root/'build'/'armv7-qemu').resolve();out.mkdir(parents=True,exist_ok=True)
cxx=os.environ.get('ARM_CXX','arm-linux-gnueabihf-g++')
qemu=os.environ.get('QEMU_ARM','qemu-arm')
sysroot=Path(os.environ.get('ARM_SYSROOT','/usr/arm-linux-gnueabihf'))
for command in ([qemu] if args.existing_dir else [cxx,qemu]):
    if not shutil.which(command):raise SystemExit(f'Missing {command}; provision Linux tools separately')
if not (sysroot/'lib/ld-linux-armhf.so.3').is_file():raise SystemExit('Missing ARM hard-float dynamic loader in ARM_SYSROOT')
def run(args,timeout=120):
    print('+ '+' '.join(map(str,args)),flush=True)
    subprocess.run(list(map(str,args)),check=True,timeout=timeout)
flags=['-std=c++17','-O2','-UNDEBUG','-Wall','-Wextra','-Wpedantic','-Werror','-Iinclude','-pthread','-march=armv7-a','-mfpu=vfpv3-d16','-mfloat-abi=hard']
if not args.existing_dir:
    run([cxx,*flags,'-shared','-fPIC','-fvisibility=hidden','src/vst/plugin.cpp','src/capture.cpp','-o',out/'mpc_cv.so'])
    for name in ['core_test','backend_test','graph_test','event_pipeline_test','capture_test','vst_host_test','vst_capture_test','rt_test','scene_io_test','vst_capture_demo']:
        sources=[f'tests/{name}.cpp'] if name!='vst_capture_demo' else ['src/vst_capture_demo.cpp']
        if name=='rt_test':sources.extend(['src/vst/plugin.cpp','src/capture.cpp'])
        if name=='capture_test':sources.append('src/capture.cpp')
        run([cxx,*flags,*sources,'-ldl','-o',out/name])
run(['python3','tools/inspect_elf.py',out/'mpc_cv.so'])
run(['python3','tools/scene_tool.py','compile','scenes/generic-performance.json',out/'fixture.mcvs'])
results=[]
for name in ['core_test','backend_test','graph_test','event_pipeline_test','capture_test','vst_host_test','vst_capture_test','rt_test','scene_io_test','vst_capture_demo']:
    cmd=[qemu,'-cpu','cortex-a9','-L',str(sysroot),str(out/name)]
    if name in ['vst_host_test','vst_capture_test']:cmd.append(str(out/'mpc_cv.so'))
    if name=='scene_io_test':cmd.extend([str(out/'fixture.mcvs'),str(out/'roundtrip.mcvs')])
    if name=='vst_capture_demo':cmd.extend([str(out/'mpc_cv.so'),str(out/'vst-capture.csv')])
    start=time.monotonic();run(cmd)
    if name=='vst_capture_demo':run(['python3','tools/validate_vst_capture.py',out/'vst-capture.csv'])
    if name=='scene_io_test':assert (out/'fixture.mcvs').read_bytes()==(out/'roundtrip.mcvs').read_bytes()
    results.append({'test':name,'result':'PASS','seconds':round(time.monotonic()-start,3)})
report={'host':platform.platform(),'compiler':args.compiler_label if args.existing_dir else subprocess.check_output([cxx,'--version'],text=True).splitlines()[0],
        'qemu':subprocess.check_output([qemu,'--version'],text=True).splitlines()[0],
        'sysroot':str(sysroot),'cpu':'cortex-a9','tests':results,
        'plugin_sha256':hashlib.sha256((out/'mpc_cv.so').read_bytes()).hexdigest(),
        'limits':'QEMU user-mode and Ubuntu cross sysroot only; no Hakai, MPC, electrical or real-time deadline validation'}
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2),flush=True)
