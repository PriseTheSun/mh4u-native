#!/usr/bin/env python3
"""Run a public native build and retain its inputs, output and measurement scope."""
import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def inputs():
    paths = {ROOT / 'CMakeLists.txt', ROOT / 'tools/build.py',
             ROOT / 'tools/cmake/windows-x64.cmake', Path(__file__).resolve()}
    paths.update(p for p in (ROOT / 'src').rglob('*')
                 if p.is_file() and p.suffix in {'.hpp', '.cpp', '.h', '.c'})
    return {str(p.relative_to(ROOT)): digest(p) for p in sorted(paths)}


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', choices=['macos', 'windows-x64'])
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=3, choices=range(1, 17))
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    before = inputs()
    save(output / 'inputs-before.json', before)
    command = ['python3', '-B', 'tools/build.py', args.target, '--jobs', str(args.jobs)]
    cpu_before = resource.getrusage(resource.RUSAGE_CHILDREN)
    start = time.monotonic()
    with (output / 'stdout.log').open('w') as stdout, (output / 'stderr.log').open('w') as stderr:
        result = subprocess.run(command, cwd=ROOT, stdout=stdout, stderr=stderr)
    elapsed = time.monotonic() - start
    cpu_after = resource.getrusage(resource.RUSAGE_CHILDREN)
    after = inputs()
    save(output / 'inputs-after.json', after)
    lines = ((output / 'stdout.log').read_text() + '\n' +
             (output / 'stderr.log').read_text()).splitlines()
    report = {
        'status': 'passed' if result.returncode == 0 and before == after else 'failed',
        'target': args.target, 'command': command, 'exit_code': result.returncode,
        'source_count': len(before), 'sources_stable': before == after,
        'wall_seconds': elapsed,
        'child_user_seconds': cpu_after.ru_utime - cpu_before.ru_utime,
        'child_system_seconds': cpu_after.ru_stime - cpu_before.ru_stime,
        'compiler_warnings': [line for line in lines if 'warning:' in line.lower()],
        'test_summary': [line for line in lines if any(s in line for s in
                        ('tests passed', 'Total Test time', '***Failed'))],
        'windows_executed': False,
        'measurement_scope': 'Warm incremental public build; process-family peak memory and '
                             'agent token costs unmeasured. Windows is cross-compilation only.',
        'outputs': [{'path': str(p), 'bytes': p.stat().st_size, 'sha256': digest(p)}
                    for p in (output / name for name in
                              ('stdout.log', 'stderr.log', 'inputs-before.json', 'inputs-after.json'))],
    }
    save(output / 'validation.json', report)
    print(json.dumps({k: v for k, v in report.items() if k != 'outputs'}), flush=True)
    raise SystemExit(0 if report['status'] == 'passed' else 1)


if __name__ == '__main__':
    main()
