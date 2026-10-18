#!/usr/bin/env python3
"""Exercise the public native scene loader and movement with real local assets."""
import argparse
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,required=True)
    parser.add_argument('--recipe',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    recipe=args.recipe.resolve(); binary=args.binary.resolve(); data=recipe.read_text(encoding='utf-8')
    records=[]

    def run(name,options,expected_error=None):
        result=subprocess.run([str(binary),*options,'--inspect-only'],capture_output=True,text=True,timeout=20)
        records.append({'name':name,'options':options,'exit_code':result.returncode,
                        'stdout':result.stdout,'stderr':result.stderr})
        if expected_error is not None:
            assert result.returncode!=0 and expected_error in result.stderr,(name,result)
            return {}
        assert result.returncode==0,(name,result.stderr)
        return {line.split(' ',1)[0]:line.split(' ',1)[1] for line in result.stdout.splitlines() if ' ' in line}

    base=['--world',str(recipe)]
    initial=run('real-world',base)
    position=lambda rows: tuple(map(float,rows['WORLD_POSITION'].split()))
    start=position(initial)
    assert initial['WORLD_GEOMETRY']=='floor=86 walls=90 boundaries=8 segments=45'
    assert math.dist(start,(-61.14208710193634,-17.212244669596355,11963.744791666666))<1e-6
    moved=run('real-free-motion',base+['--world-step-x','20','--world-step-z','10'])
    end=position(moved)
    assert abs(end[0]-start[0]-20)<1e-6 and abs(end[2]-start[2]-10)<1e-6
    assert moved['WORLD_BLOCKED_STEPS']=='0' and moved['WORLD_STEPS']=='1'
    blocked=run('real-wall-sweep',base+['--world-step-x','4000'])
    assert blocked['WORLD_BLOCKED_STEPS']=='1' and math.dist(start,position(blocked))<1500
    fallback=run('explicit-world-over-launcher-default', ['--default-catalog','absent.tsv',*base])
    assert position(fallback)==start
    run('explicit-input-conflict',base+['--catalog','absent.tsv'],'--world substitui')
    run('step-budget',base+['--world-step-x','5000'],'step budget')
    run('nonfinite-cli-step',base+['--world-step-z','nan'],'Passo de cena deve ser finito')

    variants=[('wrong-version',data.replace('WORLD 1','WORLD 2',1),'Versão'),
              ('duplicate',data+'radius\t22\n','duplicado'),
              ('unknown',data+'teleport\t1\n','desconhecido'),
              ('unsupported-geometry-height',data.replace('height\t175\n','height\t9000\n'),'coverage'),
              ('traversal',data.replace('stage\t','stage\t../',1),'não pode conter'),
              ('mismatched-spawn',data.replace('spawn-y\t-17.212244669596355','spawn-y\t20'),'não corresponde'),
              ('nonfinite-recipe',data.replace('speed\t150','speed\tnan'),'Número inválido'),
              ('incomplete-line',data.rstrip('\n'),'linhas LF')]
    for name,contents,error in variants:
        descriptor,filename=tempfile.mkstemp(prefix='.world-smoke-',suffix='.tsv',dir=recipe.parent)
        try:
            with os.fdopen(descriptor,'w',encoding='utf-8',newline='\n') as stream:stream.write(contents)
            run(name,['--world',filename],error)
        finally:
            Path(filename).unlink()
    report={'status':'passed','cases':len(records),'scope':'macOS public CLI with actual extracted scene/actor/floor/walls; no Windows runtime or original gameplay claim','records':records}
    if args.output:args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'status':'passed','cases':len(records)}))


if __name__=='__main__':
    main()
