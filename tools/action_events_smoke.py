#!/usr/bin/env python3
"""Compare public native event CLI/viewer processes to a caller-pinned ARM oracle.

No ARM/game execution, GUI or event semantic interpretation. TICK numeric
representation/tolerances belong to action_motion_smoke; EVENTS remain uint32.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

import action_motion_smoke as motion

PROFILE='diagnostic-one-layer-events-v1'
require=motion.require
digest=motion.digest


def event_values(parts,tick,prefix='EVENTS'):
    require(len(parts)==10 and parts[0]==prefix,'Malformed event row')
    require(all(x.isascii() and x.isdigit() for x in parts[1:]),'Events must contain decimal unsigned integers')
    values=[int(x) for x in parts[1:]]
    require(values[0]==tick and all(0<=x<=0xffffffff for x in values[1:]),'Event tick/uint32 domain differs')
    return values


def rows(text,identity):
    lines=text.splitlines();require(len(lines)>=4 and (len(lines)-2)%2==0,'Missing paired TICK/EVENTS rows')
    ticks=[];events=[]
    for index in range((len(lines)-2)//2):
        tick,event=lines[2+2*index:4+2*index]
        require(tick.startswith('TICK\t'),'TICK and EVENTS are out of order')
        ticks.append(tick);events.append(event_values(event.split('\t'),index))
    return motion.rows('\n'.join(lines[:2]+ticks),identity,PROFILE),events


def compare_ticks(expected,actual,columns):
    require(len(expected)==len(actual),'TICK row lengths differ')
    vector=quaternion=0.;errors=[]
    for column,(wanted,got) in enumerate(zip(expected,actual)):
        delta=abs(wanted-got)
        tolerance=motion.VECTOR_TOLERANCE if column in motion.VECTOR_COLUMNS else motion.QUATERNION_TOLERANCE if column in motion.QUATERNION_COLUMNS else 0
        if column in motion.VECTOR_COLUMNS:vector=max(vector,delta)
        elif column in motion.QUATERNION_COLUMNS:quaternion=max(quaternion,delta)
        if delta>tolerance:errors.append({'column':columns[column],'expected':wanted,'observed':got,'error':delta,'tolerance':tolerance})
    return vector,quaternion,errors


def run(args):
    started=time.monotonic()
    for name in ('binary','package','events_package','paired_root','oracle'):
        setattr(args,name,getattr(args,name).resolve())
    if args.viewer:args.viewer=args.viewer.resolve()
    pins={args.package:args.package_sha256,args.events_package:args.events_sha256,
          args.paired_root/'manifest.json':args.paired_sha256,args.oracle/'manifest.json':args.oracle_sha256}
    for path,sha in pins.items():require(path.is_file() and digest(path)==sha,f'Input identity mismatch: {path}')
    executable_hashes={p:digest(p) for p in (args.binary,args.viewer) if p}
    manifest=motion.json_file(args.oracle/'manifest.json')
    require(manifest.get('schema')=='mh4u-real-events-arm-sequences-manifest-v1' and manifest.get('profile')==PROFILE,'Unsupported oracle manifest')
    records={r['path']:r for r in manifest['files']};require(len(records)==len(manifest['files']),'Duplicate oracle path')
    verified_inputs={}
    def verified(name):
        path=motion.local_name(args.oracle,name);require(name in records,'Oracle omits requested file')
        record=records[name]
        require(path.stat().st_size==record['bytes'] and digest(path)==record['sha256'],f'Oracle bytes differ: {name}')
        verified_inputs[name]=record['sha256'];return path
    report=motion.json_file(verified('report.json'))
    require(report.get('schema')=='mh4u-real-events-arm-sequences-v1' and report.get('status')=='passed' and
            report.get('profile')==PROFILE and report.get('actions')==7,'Unsupported/incomplete event oracle')
    sequences=report['sequences'];require(len(sequences)==14,'Expected fourteen retained sequences')
    paired=motion.json_file(args.paired_root/'manifest.json')
    require(paired['model_curves']=='model-curves.json' and digest(args.paired_root/'model-curves.json')==report['curves_sha256'],
            'Paired model curves differ from ARM source')
    require(len(paired['pairs'])==7,'Expected seven paired actions')
    source_hashes={s['entry']:s['sha256'] for s in report['sources']}
    require(source_hashes=={s['entry']:s['sha256'] for s in manifest['source_identities']},'Oracle source identities differ')
    checks=[];viewer_checks=[];negatives=[];failures=[];total=0;max_vector=max_quaternion=0.;processes=0
    def command(argv):
        nonlocal processes
        processes+=1
        return subprocess.run([str(x) for x in argv],capture_output=True,text=True,timeout=30)
    common=[args.binary,'--package',args.package,'--paired-root',args.paired_root,'--events-package',args.events_package]
    seen=set()
    for sequence in sequences:
        index=sequence['action_index'];count=sequence['ticks'];identity=sequence['identity'];mode=sequence['mode']
        require(type(index) is int and 0<=index<7 and type(count) is int and 0<count<=100000 and mode in ('loop','clamp'),
                'Oracle process request exceeds policy')
        require((index,mode) not in seen,'Duplicate oracle sequence');seen.add((index,mode))
        pair=paired['pairs'][index]
        require(identity['entry_id']==pair['entry_id'] and identity['slot']==pair['source_slot'] and
                identity['lmt_sha256']==pair['source_lmt_sha256']==source_hashes[identity['entry_id']] and
                identity['frame_count']==pair['source_frame_count'] and identity['gltf_clip_index']==pair['joint_clip_index'],
                'Action identity differs between oracle and paired source')
        expected_ticks,expected_events=rows(verified(sequence['expected_tsv']).read_text(),identity)
        require(len(expected_ticks)==count+1,'Oracle tick count differs')
        argv=common+['--action',str(index),'--ticks',str(count),'--step','1','--start','0']+(['--repeat'] if mode=='loop' else [])
        result=command(argv);require(result.returncode==0,f'CLI failed: {sequence["name"]}: {result.stderr}')
        actual_ticks,actual_events=rows(result.stdout,identity)
        require(len(actual_ticks)==len(expected_ticks),'Native omitted/added rows')
        local_vector=local_quaternion=0.;event_mismatches=0
        for tick,(want,got) in enumerate(zip(expected_ticks,actual_ticks)):
            v,q,errors=compare_ticks(want,got,report['cli_columns']);local_vector=max(local_vector,v);local_quaternion=max(local_quaternion,q)
            failures.extend({'sequence':sequence['name'],'tick':tick,**e} for e in errors[:max(0,20-len(failures))])
            if expected_events[tick]!=actual_events[tick]:
                event_mismatches+=1
                if len(failures)<20:failures.append({'sequence':sequence['name'],'tick':tick,'expected_events':expected_events[tick],'actual_events':actual_events[tick]})
        total+=len(actual_ticks);max_vector=max(max_vector,local_vector);max_quaternion=max(max_quaternion,local_quaternion)
        checks.append({'sequence':sequence['name'],'rows':len(actual_ticks),'event_mismatches':event_mismatches,
                       'max_vector_error':local_vector,'max_quaternion_error':local_quaternion,
                       'stdout_sha256':hashlib.sha256(result.stdout.encode()).hexdigest(),'command':[str(x) for x in argv]})
        if args.viewer:
            n=identity['frame_count'];points={0,(n-1)//2,n-1,min(n+1,count)}
            for component in (slice(1,5),slice(5,9)):
                active=next((i for i,row in enumerate(expected_events) if any(row[component])),None)
                if active is not None:points.add(active)
            for tick in sorted(points):
                argv=[args.viewer,'--motion-package',args.package,'--paired-root',args.paired_root,'--events-package',args.events_package,
                      '--motion-action',str(index),'--motion-ticks',str(tick),'--inspect-only']+(['--motion-repeat'] if mode=='loop' else [])
                inspected=command(argv);require(inspected.returncode==0,f'Viewer inspection failed: {inspected.stderr}')
                state=[x for x in inspected.stdout.splitlines() if x.startswith('MOTION_STATE\t')]
                masks=[x for x in inspected.stdout.splitlines() if x.startswith('MOTION_EVENTS\t')]
                require(len(state)==len(masks)==1,'Viewer omitted/duplicated state or event record')
                state=motion.tick_values(['TICK']+state[0].split('\t')[1:],tick)
                masks=event_values(masks[0].split('\t'),tick,'MOTION_EVENTS')
                v,q,errors=compare_ticks(expected_ticks[tick],state,report['cli_columns'])
                require(not errors and masks==expected_events[tick],f'Viewer differs from ARM at action {index}, tick {tick}: {errors}')
                viewer_checks.append({'sequence':sequence['name'],'tick':tick,'current':masks[1:5],'previous':masks[5:9],
                                      'max_vector_error':v,'max_quaternion_error':q,'stdout_sha256':hashlib.sha256(inspected.stdout.encode()).hexdigest()})
    require(total==report['tick_rows']==report['events_rows']==2604,'Complete oracle row coverage differs')
    def reject(name,argv,reason,viewer=False):
        result=command(argv)
        require(result.returncode!=0 and reason.lower() in result.stderr.lower(),f'Negative {name} not rejected as expected: {result.stdout}{result.stderr}')
        prohibited=('TICK\t','EVENTS\t','MOTION_STATE\t','MOTION_EVENTS\t','OPENGL ')
        require(not any(line.startswith(prohibited) for line in result.stdout.splitlines()),f'Negative {name} published state')
        negatives.append({'name':name,'surface':'viewer' if viewer else 'cli','returncode':result.returncode,'reason':result.stderr.strip()})
    with tempfile.TemporaryDirectory(prefix='mh4u-action-events-smoke-') as directory:
        temporary=Path(directory);changed=temporary/'changed.bin';payload=args.events_package.read_bytes()
        require(payload,'Empty event package');changed.write_bytes(payload[:-1]+bytes([payload[-1]^1]))
        for label,path,reason in [('changed',changed,'SHA-256'),('missing',temporary/'missing.bin','open')]:
            reject('events-'+label,[args.binary,'--package',args.package,'--paired-root',args.paired_root,'--events-package',path,'--action','0','--ticks','1'],reason)
            if args.viewer:reject('events-'+label,[args.viewer,'--motion-package',args.package,'--paired-root',args.paired_root,
                '--events-package',path,'--motion-action','0','--motion-ticks','1','--inspect-only'],reason,True)
    for path,sha in {**pins,**executable_hashes}.items():require(digest(path)==sha,f'Input changed during comparison: {path}')
    for name,sha in verified_inputs.items():require(digest(motion.local_name(args.oracle,name))==sha,'Oracle changed during comparison')
    summary={'schema':'mh4u-action-events-process-comparison-v1','passed':not failures,'profile':PROFILE,
        'input_pins':[{'path':str(p),'sha256':s} for p,s in pins.items()],
        'executables':[{'path':str(p),'sha256':s} for p,s in executable_hashes.items()],
        'oracle_files':verified_inputs,'source_lmt_hashes':source_hashes,'event_inventory_sha256':report['events_inventory']['sha256'],
        'sequences':checks,'tick_rows_compared':total,'event_rows_compared':total,'event_integer_components':total*8,
        'max_vector_error':max_vector,'max_quaternion_error':max_quaternion,'failures':failures,
        'viewer_checks':viewer_checks,'negative_controls':negatives,'processes':processes,
        'tolerances':{'vectors':motion.VECTOR_TOLERANCE,'quaternions':motion.QUATERNION_TOLERANCE,'clock_and_events':0},
        'arm_reexecuted':False,'game_executed':False,'gui_opened':False,'elapsed_seconds':time.monotonic()-started,
        'producer_sha256':digest(Path(__file__)),'tick_owner_sha256':digest(Path(motion.__file__))}
    if args.output:
        with args.output.open('x') as file:json.dump(summary,file,indent=2);file.write('\n')
    print(json.dumps({k:summary[k] for k in ('passed','tick_rows_compared','event_rows_compared','max_vector_error','max_quaternion_error','failures','processes')},ensure_ascii=False))
    return 0 if summary['passed'] else 1


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('binary','package','events-package','paired-root','oracle'):p.add_argument('--'+name,type=Path,required=True)
    for name in ('package-sha256','events-sha256','paired-sha256','oracle-sha256'):p.add_argument('--'+name,required=True)
    p.add_argument('--viewer',type=Path);p.add_argument('--output',type=Path);args=p.parse_args()
    try:return run(args)
    except (OSError,ValueError,AssertionError,KeyError,TypeError,subprocess.TimeoutExpired) as error:
        print(json.dumps({'passed':False,'error':str(error)}));return 1


if __name__=='__main__':raise SystemExit(main())
