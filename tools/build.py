#!/usr/bin/env python3
"""Build the native resource lab using the project's pinned local CMake."""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', choices=('macos', 'windows-x64'))
    parser.add_argument('--jobs', type=int, default=3)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 16:
        parser.error('--jobs must be between 1 and 16')
    cmake = ROOT/'.tools/cmake-4.4.4-macos-universal/CMake.app/Contents/bin/cmake'
    build = ROOT/'build' if args.target == 'macos' else ROOT/'build/windows-x64'
    command = [str(cmake), '-S', str(ROOT), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release']
    if args.target == 'windows-x64':
        command += ['-DCMAKE_TOOLCHAIN_FILE='+str(ROOT/'tools/cmake/windows-x64.cmake')]
    subprocess.run(command, check=True)
    subprocess.run([str(cmake), '--build', str(build), '--parallel', str(args.jobs)], check=True)
    if args.target == 'macos':
        subprocess.run([str(cmake.with_name('ctest')), '--test-dir', str(build), '--output-on-failure'], check=True)


if __name__ == '__main__':
    main()
