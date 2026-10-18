#!/usr/bin/env python3
"""Install pinned development tools locally. No system profile/package changes."""
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PACKAGES = [
    dict(name='Ghidra', version='12.1.4',
         url='https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.1.4_build/ghidra_12.1.4_PUBLIC_20260921.zip',
         sha256='ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db',
         destination='.tools', root='ghidra_12.1.4_PUBLIC'),
    dict(name='Temurin JDK', version='21.0.12.1+1',
         url='https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.12.1%2B1/OpenJDK21U-jdk_aarch64_mac_hotspot_21.0.12.1_1.tar.gz',
         sha256='3623232f33a9c3baadf304480b2535f9a3cba8a58d42ecbb438ba267315d9998',
         destination='.tools', root='jdk-21.0.12.1+1'),
    dict(name='CMake', version='4.4.4',
         url='https://github.com/Kitware/CMake/releases/download/v4.4.4/cmake-4.4.4-macos-universal.tar.gz',
         sha256='4b7b73704b1db9b374e5c9ab8e17ac6148b817b6396ee75cd94a852cbac9d305',
         destination='.tools', root='cmake-4.4.4-macos-universal'),
    dict(name='SDL', version='3.4.18',
         url='https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz',
         sha256='9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3',
         destination='vendor', root='SDL3-3.4.18'),
    dict(name='LLVM MinGW', version='20261006',
         url='https://github.com/mstorsjo/llvm-mingw/releases/download/20261006/llvm-mingw-20261006-ucrt-macos-universal.tar.xz',
         sha256='a7a19b11048e89372637d26279cdb7c430ed3cf8c1e224310421312bf986ebf6',
         destination='.tools', root='llvm-mingw-20261006-ucrt-macos-universal'),
]
IMGUI = dict(name='Dear ImGui', version='1.93.0',
             url='https://github.com/ocornut/imgui.git',
             tag='v1.93.0', commit='c1aaac65e896a45f81f47a4d68f9959e0f86c859')


def sha256(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(4*1024*1024), b''):
            h.update(block)
    return h.hexdigest()


def prepare(package):
    downloads = ROOT/'.tools/downloads'
    downloads.mkdir(parents=True, exist_ok=True)
    archive = downloads/package['url'].rsplit('/', 1)[-1]
    if not archive.exists():
        partial = archive.with_name(archive.name+'.part')
        print('Downloading '+package['name'], flush=True)
        subprocess.run(['curl', '--fail', '--location', '--retry', '3', '--connect-timeout', '30',
                        '--silent', '--show-error', '--output', str(partial), package['url']], check=True)
        if sha256(partial) != package['sha256']:
            raise ValueError('Download checksum mismatch: '+package['name'])
        partial.rename(archive)
    if sha256(archive) != package['sha256']:
        raise ValueError('Cached checksum mismatch: '+package['name'])
    destination = ROOT/package['destination']
    installed = destination/package['root']
    marker = installed/'.mh4u-install.json'
    if installed.exists():
        if not marker.exists() or json.loads(marker.read_text())['sha256'] != package['sha256']:
            raise FileExistsError('Existing installation is not owned by this bootstrap: '+str(installed))
        print('Verified existing '+package['name'], flush=True)
        return package
    staging = destination/(package['root']+'.extracting')
    staging.mkdir(exist_ok=False)
    if archive.suffix == '.zip':
        with zipfile.ZipFile(archive) as z:
            for entry in z.infolist():
                target = (staging/entry.filename).resolve()
                if not target.is_relative_to(staging.resolve()):
                    raise ValueError('Archive path escapes staging')
                z.extract(entry, staging)
                mode = entry.external_attr >> 16
                if mode & 0o777:
                    target.chmod(mode & 0o777)
    else:
        with tarfile.open(archive) as tar:
            for entry in tar.getmembers():
                target = (staging/entry.name).resolve()
                if not target.is_relative_to(staging.resolve()):
                    raise ValueError('Archive path escapes staging')
                if entry.issym() or entry.islnk():
                    parent = target.parent if entry.issym() else staging
                    if not (parent/entry.linkname).resolve().is_relative_to(staging.resolve()):
                        raise ValueError('Archive link escapes staging')
        subprocess.run(['tar','-xf',str(archive),'-C',str(staging)],check=True)
    extracted = staging/package['root']
    if not extracted.is_dir():
        raise ValueError('Unexpected archive root: '+str(list(staging.iterdir())))
    (extracted/'.mh4u-install.json').write_text(json.dumps(package, indent=2)+'\n')
    extracted.rename(installed)
    staging.rmdir()
    print('Installed '+package['name']+' at '+str(installed),flush=True)
    return package


def prepare_imgui():
    destination = ROOT/'vendor/imgui'
    if not destination.exists():
        subprocess.run(['git','clone','--depth','1','--branch',IMGUI['tag'],
                        IMGUI['url'],str(destination)],check=True)
    commit = subprocess.check_output(['git','-C',str(destination),'rev-parse','HEAD'],text=True).strip()
    if commit != IMGUI['commit']:
        raise ValueError('Existing ImGui checkout does not match the pinned commit')
    subprocess.run(['git','-C',str(destination),'diff','--exit-code','HEAD'],check=True)
    return IMGUI


def prepare_rea():
    destination = ROOT/'.tools/rea'
    destination.mkdir(exist_ok=True)
    for name in ('package.json','package-lock.json'):
        source = ROOT/'tools/rea'/name
        target = destination/name
        if target.exists() and target.read_bytes() != source.read_bytes():
            shutil.copy2(target,target.with_name(name+f'.backup-{time.time_ns()}'))
        if not target.exists() or target.read_bytes() != source.read_bytes():
            shutil.copy2(source,target)
    subprocess.run(['npm','ci','--prefix',str(destination),'--ignore-scripts',
                    '--no-audit','--no-fund'],check=True)
    return dict(name='REA',version='6.5.0',lockfile='tools/rea/package-lock.json')


def prepare_cgltf():
    lock = json.loads((ROOT/'tools/dependencies/cgltf.lock.json').read_text())
    for item in lock['files']:
        target = (ROOT/item['path']).resolve()
        if not target.is_relative_to((ROOT/'vendor').resolve()):
            raise ValueError('Pinned dependency destination escapes vendor')
        target.parent.mkdir(parents=True,exist_ok=True)
        if target.exists():
            if sha256(target) != item['sha256']:
                raise ValueError('Existing cgltf file differs from pinned digest: '+str(target))
            continue
        partial = target.with_name(target.name+'.part')
        subprocess.run(['curl','--fail','--location','--retry','3','--silent','--show-error',
                        '--output',str(partial),item['url']],check=True)
        if sha256(partial) != item['sha256']:
            raise ValueError('cgltf download digest mismatch')
        partial.rename(target)
    return dict(name=lock['name'],commit=lock['commit'],lockfile='tools/dependencies/cgltf.lock.json')


if __name__ == '__main__':
    start=time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        results=list(pool.map(prepare,PACKAGES))
    results += [prepare_imgui(),prepare_rea(),prepare_cgltf()]
    (ROOT/'tools/dependencies.lock.json').write_text(json.dumps(results,indent=2)+'\n')
    print(f'Completed in {time.monotonic()-start:.1f}s',flush=True)
