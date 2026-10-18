"""Exercise the public native actor loader against bounded, malformed recipes."""
import argparse,json,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--recipe',type=Path,required=True);a=p.parse_args()
recipe=a.recipe.resolve();binary=a.binary.resolve();original=recipe.read_text();rows=original.splitlines();model=next(x for x in rows if x.startswith('model\t'));texture=next(x for x in rows if x.startswith('texture\t'));action=next(x for x in rows if x.startswith('action\t'));primitive=next(x for x in rows if x.startswith('primitive\t'))
mutations={
 'version':original.replace('ACTOR 1','ACTOR 2',1),
 'newline':original[:-1],
 'nul':original+'\x00\n',
 'oversize':'X'*65537,
 'duplicate-rate':original+'rate\t60\n',
 'missing-rate':original.replace('rate\t60\n',''),
 'model-hash':original.replace(model,model.rsplit('\t',1)[0]+'\t'+'0'*64),
 'texture-hash':original.replace(texture,texture.rsplit('\t',1)[0]+'\t'+'0'*64),
 'traversal':original.replace(model.split('\t')[1],'../'+model.split('\t')[1]),
 'absolute-path':original.replace(model.split('\t')[1],str(recipe.parent/model.split('\t')[1])),
 'duplicate-primitive':original+primitive+'\n',
 'unknown-primitive':original.replace(primitive,'primitive\t0\t99999\t0'),
 'duplicate-action':original+action+'\n',
 'out-of-range-slot':original.replace(action,'action\t0\t256\tinvalid'),
 'unknown-motion':original.replace(action,'action\t99\t1\tinvalid'),
 'unknown-row':original+'camera\tunknown\n',
}
command=next(x for x in rows if x.startswith('commands\t'))
mutations.update({
 'command-hash':original.replace(command,command.rsplit('\t',1)[0]+'\t'+'0'*64),
 'duplicate-command':original+command+'\n',
 'command-traversal':original.replace(command.split('\t')[1],'../'+command.split('\t')[1]),
})
records=[]
with tempfile.TemporaryDirectory(prefix='mh4u-native-recipe-') as temporary:
 root=Path(temporary)
 for row in rows:
  fields=row.split('\t')
  if fields[0] in ('model','motion','texture','commands'):shutil.copyfile(recipe.parent/fields[1],root/fields[1])
 selected=root/'test.mhnative'
 for name,text in [('valid',original),*mutations.items()]:
  selected.write_text(text);result=subprocess.run([str(binary),'--model',str(selected),'--inspect-only'],capture_output=True,text=True,timeout=15)
  if name=='valid':assert result.returncode==0 and 'NATIVE_PROFILE mod-lmt-transition-v3' in result.stdout and not result.stderr,result
  else:assert result.returncode==1 and not result.stdout and 'Ator nativo:' in result.stderr,(name,result)
  records.append({'case':name,'returncode':result.returncode,'stderr':result.stderr.strip()})
print(json.dumps({'status':'passed','cases':len(records),'records':records},ensure_ascii=False))
