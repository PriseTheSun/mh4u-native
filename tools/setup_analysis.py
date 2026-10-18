#!/usr/bin/env python3
"""Prepare Ghidra's Apple Silicon decompiler and a local REA configuration."""
import argparse
import json
from pathlib import Path
import platform
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rea-launcher', type=Path,
                        default=ROOT/'.tools/rea/node_modules/rea-agents/scripts/rea.mjs')
    args = parser.parse_args()
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('This setup script supports macOS arm64 only')
    if not args.rea_launcher.is_file():
        parser.error('Install the pinned REA package with: npm ci --prefix .tools/rea --ignore-scripts')
    ghidra = ROOT/'.tools/ghidra_12.1.4_PUBLIC'
    java = ROOT/'.tools/jdk-21.0.12.1+1/Contents/Home'
    native = ghidra/'Ghidra/Features/Decompiler/os/mac_arm_64/decompile'
    if not native.is_file():
        source = ghidra/'Ghidra/Features/Decompiler/src/decompile/cpp'
        subprocess.run(['make', '-j3', 'ghidra_opt', 'ARCH_TYPE=-arch arm64',
                        'OSDIR=mac_arm_64', 'ADDITIONAL_FLAGS=-mmacosx-version-min=11.0 -w'],
                       cwd=source, check=True)
        native.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source/'ghidra_opt', native)
    subprocess.run(['file', str(native)], check=True)
    subprocess.run([str(java/'bin/java'), '-version'], check=True)
    node = shutil.which('node')
    if node is None:
        parser.error('Node.js is required by REA')
    value = {'ghidra_install_dir': str(ghidra), 'java_home': str(java),
             'rea_launcher': str(args.rea_launcher.resolve()), 'node': node,
             'language_id': 'ARM:LE:32:v6', 'heap': '3G', 'startup_timeout_ms': 1200000}
    target = ROOT/'research/private/analysis-tools.json'
    target.parent.mkdir(parents=True, exist_ok=True)
    content = json.dumps(value, indent=2)+'\n'
    if target.exists() and target.read_text() != content:
        shutil.copy2(target, target.with_name(target.name+f'.backup-{time.time_ns()}'))
    if not target.exists() or target.read_text() != content:
        target.write_text(content)
    print(target)


if __name__ == '__main__':
    main()
