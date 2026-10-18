#!/usr/bin/env python3
"""Public viewer boundary: explicit Player profile with real MOD/LMT resources."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,required=True)
    parser.add_argument('--recipe',type=Path,required=True)
    parser.add_argument('--constants',type=Path,required=True)
    parser.add_argument('--world',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args();records=[]
    actor=['--model',str(args.recipe.resolve())]
    preview=['--player-preview',str(args.constants.resolve())]
    def run(name,options,error=None):
        command=[str(args.binary.resolve()),*options,'--inspect-only']
        result=subprocess.run(command,capture_output=True,text=True,timeout=30)
        records.append({'name':name,'command':command,'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
        if error:
            assert result.returncode==1 and error in result.stderr,(name,result)
            return {}
        assert result.returncode==0 and not result.stderr,(name,result)
        rows=dict(line.split(' ',1) for line in result.stdout.splitlines() if ' ' in line)
        assert rows['PLAYER_PROFILE']=='diagnostic-player-driver-v1'
        assert rows['PLAYER_JOINTS']=='19' and rows['PLAYER_PARTS']=='7'
        assert 'bootstrap=unknown' in rows['PLAYER_POLICY']
        return rows
    results={}
    for state,ticks in [(1,0),(1,1),(1,12),(1,96),(3,0),(3,12)]:
        rows=run(f'state-{state}-ticks-{ticks}',actor+preview+['--player-state',str(state),'--player-ticks',str(ticks)])
        assert int(rows['PLAYER_TICKS'])==ticks and int(rows['PLAYER_REQUEST_STATE'])==state
        assert int(rows['PLAYER_PALETTES'])>0
        # State 3 need not select a clip from this explicit reference seed.
        # Assert sampling only for the state-1 path that actually binds co0.
        if ticks and state==1:assert int(rows['PLAYER_SAMPLES'])>0
        results[state,ticks]=rows
    assert results[1,0]['BOUNDS']!=results[1,12]['BOUNDS'],'Driver did not change rendered geometry'
    repeated=run('deterministic-fresh-process',actor+preview+['--player-ticks','12'])
    assert repeated==results[1,12],'Explicit state/ticks depend on host time'
    world=run('world-with-player',['--world',str(args.world.resolve()),*preview,'--player-ticks','12'])
    assert world['BOUNDS']==results[1,12]['BOUNDS'] and 'WORLD_POSITION' in world
    run('missing-profile',actor+['--player-ticks','1'],'requerem --player-preview')
    run('unsupported-state',actor+preview+['--player-state','4'],'admite 1 ou 3')
    run('negative-ticks',actor+preview+['--player-ticks','-1'],'entre 0 e 100000')
    run('unbounded-ticks',actor+preview+['--player-ticks','100001'],'entre 0 e 100000')
    run('seek-conflict',actor+preview+['--time','1'],'Estado e tempo pertencem ao Player')
    run('network-conflict',['--world',str(args.world),*preview,'--connect-port','47124'],'rede e outros perfis')
    run('capture-host-time',actor+preview+['--frames','2'],'Captura do Player exige')
    with tempfile.TemporaryDirectory(prefix='mh4u-player-constants-') as directory:
        path=Path(directory)/'constants.bin';raw=args.constants.read_bytes()
        for name,data in [('truncated',raw[:-1]),('trailing',raw+b'\0'),('identity',raw[:40]+bytes([raw[40]^1])+raw[41:])]:
            path.write_bytes(data)
            run(name,actor+['--player-preview',str(path)],'Constantes do Player:')
    report={'status':'passed','cases':len(records),'scope':'Public native viewer CLI, actual 19-joint/seven-part resources. Explicit diagnostic seed; no original bootstrap, SDL policy or online action claim.','records':records}
    if args.output:args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'status':'passed','cases':len(records)}))


if __name__=='__main__':main()
