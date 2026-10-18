#!/usr/bin/env python3
"""Compare the public action CLI to retained original ARM sequences; never runs ARM.

The oracle contains private local game-derived samples. It is passed explicitly,
not distributed with this tool. The executable and package perform their normal
identity checks; the comparator additionally pins the paired curve document.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import time

VECTOR_TOLERANCE = 1e-4
QUATERNION_TOLERANCE = 1e-5
# Indices exclude the leading TICK word. Six exact clock/identity fields precede
# position3, rotation4, previous translation3, previous rotation4, prediction3/4.
VECTOR_COLUMNS = set(range(6,9)) | set(range(13,16)) | set(range(20,23))
QUATERNION_COLUMNS = set(range(9,13)) | set(range(16,20)) | set(range(23,27))


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def json_file(path, maximum=4*1024*1024):
    require(path.stat().st_size <= maximum, f"JSON size exceeds comparator bound: {path}")
    return json.loads(path.read_text(encoding='utf-8'))


def local_name(directory, name):
    require(isinstance(name, str) and name and Path(name).name == name and name not in ('.','..'),
            'Oracle references must be immediate filenames')
    return directory / name


def tick_values(parts, tick):
    """Own the CLI/native float32 TICK representation shared by diagnostics."""
    require(len(parts)==28 and parts[0]=='TICK', 'Malformed CLI tick record')
    values=[float(value) for value in parts[1:]]
    require(all(math.isfinite(v) for v in values), 'Nonfinite CLI tick component')
    # Nine significant decimal digits round-trip binary32. Restore the
    # printed native float, avoiding decimal-formatting error in comparison.
    values=[struct.unpack('<f',struct.pack('<f',v))[0] for v in values]
    require(values[0]==tick, 'CLI tick sequence is discontinuous')
    return values


def rows(text, identity, profile='diagnostic-one-layer-v1'):
    lines=text.splitlines()
    require(lines[:2] == ['PROFILE\t'+profile,
            'ACTION\t'+str(identity['entry_id'])+'\t'+str(identity['slot'])+'\t'+str(identity['gltf_clip_index'])],
            'CLI diagnostic profile/action identity differs from oracle')
    result=[]
    for line in lines[2:]:
        result.append(tick_values(line.split('\t'),len(result)))
    return result


def run(args):
    start=time.monotonic();binary=args.binary.resolve();package=args.package.resolve();root=args.paired_root.resolve();oracle=args.oracle.resolve()
    require(binary.is_file() and package.is_file() and root.is_dir() and oracle.is_dir(), 'Missing selected input')
    binary_hash=digest(binary);package_hash=digest(package)
    manifest=json_file(oracle/'manifest.json');records={r['path']:r for r in manifest['files']}
    require(len(records)==len(manifest['files']), 'Duplicate oracle manifest paths')
    def verified(name):
        path=local_name(oracle,name);require(name in records, f'Oracle manifest omits {name}')
        record=records[name];require(path.stat().st_size==record['bytes'] and digest(path)==record['sha256'], f'Oracle identity mismatch: {name}')
        return path
    report=json_file(verified('report.json'))
    require(report.get('schema')=='mh4u-real-curves-arm-sequences-v1' and report.get('status')=='passed', 'Unsupported/incomplete ARM oracle')
    sequences=report['sequences'];require(len(sequences)==14 and report['actions']==7, 'Expected the reviewed seven-action, fourteen-sequence oracle')
    paired=json_file(root/'manifest.json');curve_name=paired['model_curves']
    require(curve_name=='model-curves.json' and digest(root/curve_name)==report['curves_sha256'], 'Paired curves differ from oracle')
    checks=[];compared=0;max_vector=0.;max_quaternion=0.;failures=[]
    def command(arguments):
        return subprocess.run([str(binary), *arguments], capture_output=True, text=True, timeout=30)
    common=['--package',str(package),'--paired-root',str(root)]
    for sequence in sequences:
        count=sequence['ticks'];index=sequence['action_index'];identity=sequence['identity']
        require(type(count) is int and 0<count<=100000 and type(index) is int and 0<=index<7, 'Oracle process request exceeds CLI bounds')
        expected=rows(verified(sequence['expected_tsv']).read_text(encoding='utf-8'),identity)
        require(len(expected)==count+1, 'Oracle tick count differs from declared sequence')
        arguments=common+['--action',str(index),'--ticks',str(count),'--step','1','--start','0']
        require(sequence['mode'] in ('loop','clamp'), 'Unknown oracle playback mode')
        if sequence['mode']=='loop':arguments.append('--repeat')
        result=command(arguments)
        require(result.returncode==0, f"Native CLI failed {sequence['name']}: {result.stderr.strip()}")
        observed=rows(result.stdout,identity);require(len(observed)==len(expected), 'CLI omitted/added ticks')
        local_vector=local_quaternion=0.;mismatches=0
        for tick,(want,got) in enumerate(zip(expected,observed)):
            for column,(a,b) in enumerate(zip(want,got)):
                error=abs(a-b)
                tolerance=VECTOR_TOLERANCE if column in VECTOR_COLUMNS else QUATERNION_TOLERANCE if column in QUATERNION_COLUMNS else 0
                if column in VECTOR_COLUMNS:local_vector=max(local_vector,error)
                elif column in QUATERNION_COLUMNS:local_quaternion=max(local_quaternion,error)
                if error>tolerance:
                    mismatches+=1
                    if len(failures)<20:failures.append({'sequence':sequence['name'],'tick':tick,'column':report['cli_columns'][column],
                        'expected':a,'observed':b,'absolute_error':error,'tolerance':tolerance})
        compared+=len(observed);max_vector=max(max_vector,local_vector);max_quaternion=max(max_quaternion,local_quaternion)
        checks.append({'sequence':sequence['name'],'ticks_including_initial':len(observed),'max_vector_error':local_vector,
                       'max_quaternion_error':local_quaternion,'mismatched_components':mismatches,'command':arguments,
                       'stdout_sha256':hashlib.sha256(result.stdout.encode()).hexdigest()})
    negatives=[]
    def rejected(name,arguments,reason):
        result=command(arguments)
        require(result.returncode!=0 and result.stderr.startswith('ERROR ') and reason in result.stderr,
                f'Negative CLI control {name} not rejected with expected reason: {result.stdout}{result.stderr}')
        require(not any(line.startswith('TICK\t') for line in result.stdout.splitlines()), f'Rejected request {name} emitted partial ticks')
        negatives.append({'name':name,'returncode':result.returncode,'reason':result.stderr.strip()})
    valid=common+['--action','0','--ticks','1']
    for name,extra,reason in [
        ('action-negative',['--action','-1'],'invalid_options'),('action-past-end',['--action','7'],'action_range'),
        ('ticks-negative',['--ticks','-1'],'invalid_options'),('ticks-over-budget',['--ticks','100001'],'invalid_options'),
        ('nonfinite-step',['--step','nan'],'invalid_options'),('infinite-step',['--step','inf'],'invalid_options'),
        ('step-outside-policy',['--step','29'],'motion_diagnostic_clock_policy'),
        ('start-negative',['--start','-1'],'motion_diagnostic_clock_policy'),('start-past-end',['--start','29'],'motion_diagnostic_clock_policy'),
        ('malformed-number',['--ticks','1x'],'invalid_number'),('unknown-option',['--unknown','1'],'unknown_option'),
        ('missing-value',['--step'],'missing_option_value')]:rejected(name,valid+extra,reason)
    with tempfile.TemporaryDirectory(prefix='mh4u-action-motion-smoke-') as directory:
        temporary=Path(directory);bad_package=temporary/'modified.bin';data=package.read_bytes()
        require(bool(data),'Empty selected package');bad_package.write_bytes(data[:-1]+bytes([data[-1]^1]))
        rejected('modified-package',['--package',str(bad_package),'--paired-root',str(root),'--action','0','--ticks','1'],'SHA-256')
        bad_root=temporary/'content';shutil.copytree(root,bad_root)
        buffer=next(item['uri'] for item in paired['files'] if item['uri'].endswith('.bin'))
        path=(bad_root/buffer).resolve();require(path.is_relative_to(bad_root.resolve()),'Paired test path escapes temporary root')
        data=path.read_bytes();require(bool(data),'Empty selected buffer');path.write_bytes(bytes([data[0]^1])+data[1:])
        rejected('modified-external-buffer',['--package',str(package),'--paired-root',str(bad_root),'--action','0','--ticks','1'],'selected hash')
    require(digest(binary)==binary_hash and digest(package)==package_hash, 'Executable/package changed during process comparison')
    summary={'schema':'mh4u-action-motion-process-comparison-v1','passed':not failures,'binary':str(binary),'binary_sha256':binary_hash,
        'package':str(package),'package_sha256':package_hash,'paired_root':str(root),'curves_sha256':report['curves_sha256'],
        'oracle_manifest_sha256':digest(oracle/'manifest.json'),'oracle_package_sha256':report['package_sha256'],
        'package_version_may_differ':'Container/binding version may change; exact paired curve bytes and ACTION source identities must match',
        'rows_compared':compared,'sequences':checks,'negative_controls':negatives,'max_vector_error':max_vector,'max_quaternion_error':max_quaternion,
        'tolerances':{'vectors_absolute':VECTOR_TOLERANCE,'quaternions_absolute':QUATERNION_TOLERANCE,'clock_flags_requests':0},
        'failures':failures,'arm_reexecuted':False,'original_game_runtime_executed':False,'elapsed_seconds':time.monotonic()-start,
        'producer_sha256':digest(Path(__file__))}
    if args.output:
        with args.output.open('x',encoding='utf-8') as handle:json.dump(summary,handle,indent=2);handle.write('\n')
    print(json.dumps({'passed':summary['passed'],'rows_compared':compared,'sequences':len(checks),
        'negative_controls':len(negatives),'max_vector_error':max_vector,'max_quaternion_error':max_quaternion,
        'failures':failures,'output':str(args.output) if args.output else None},ensure_ascii=False))
    return 0 if summary['passed'] else 1


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,required=True);parser.add_argument('--package',type=Path,required=True)
    parser.add_argument('--paired-root',type=Path,required=True);parser.add_argument('--oracle',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    try:return run(args)
    except (OSError,ValueError,AssertionError,KeyError,TypeError,struct.error,subprocess.TimeoutExpired) as error:
        print(json.dumps({'passed':False,'error':str(error)}));return 1

if __name__=='__main__':sys.exit(main())
