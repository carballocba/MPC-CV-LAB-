#!/usr/bin/env python3
"""Fetch an official SHA-256-pinned Zig into ignored build/toolchains; no global install."""
import hashlib,os,platform,subprocess,sys,tarfile,tempfile
from pathlib import Path
VERSION='0.15.2'
DISTRIBUTIONS={
    ('Linux','x86_64'):('x86_64-linux','02aa270f183da276e5b5920b1dac44a63f1a49e55050ebde3aecc9eb82f93239'),
    ('Darwin','arm64'):('aarch64-macos','3cc2bab367e185cdfb27501c4b30b1b0653c28d9f73df8dc91488e66ece5fa6b')}
def main():
    key=(platform.system(),platform.machine())
    if key not in DISTRIBUTIONS:raise SystemExit('Unsupported build host; supply a verified ZIG path manually')
    host,digest=DISTRIBUTIONS[key];name=f'zig-{host}-{VERSION}'
    root=Path(__file__).resolve().parents[1]/'build/toolchains';root.mkdir(parents=True,exist_ok=True)
    archive=root/(name+'.tar.xz');binary=root/name/'zig'
    if not archive.is_file():
        fd,temp=tempfile.mkstemp(dir=root,suffix='.download');os.close(fd)
        try:
            subprocess.run(['curl','--fail','--silent','--show-error','--location','--max-time','120',
                            f'https://ziglang.org/download/{VERSION}/{name}.tar.xz','-o',temp],check=True)
            if hashlib.sha256(Path(temp).read_bytes()).hexdigest()!=digest:raise ValueError('Zig archive checksum mismatch')
            os.replace(temp,archive)
        finally:
            if os.path.exists(temp):os.unlink(temp)
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=digest:raise ValueError('Cached archive checksum mismatch; remove it and retry')
    if not binary.is_file():
        with tempfile.TemporaryDirectory(dir=root) as stage:
            with tarfile.open(archive) as tar:tar.extractall(stage,filter='data')
            os.replace(Path(stage)/name,root/name)
    if subprocess.check_output([str(binary),'version'],text=True).strip()!=VERSION:raise ValueError('Wrong extracted Zig version')
    print(binary)
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,subprocess.CalledProcessError) as error:sys.exit(str(error))
