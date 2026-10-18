#!/usr/bin/env python3
"""Verify a local package's files and public macOS resource/world entry points."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from world_recipe import RECIPE_NAME


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(); package=args.package.resolve()
    manifest=json.loads((package/'manifest.json').read_text()); listed=set()
    for item in manifest['files']:
        relative=Path(item['path'])
        assert not relative.is_absolute() and '..' not in relative.parts
        assert item['path'] not in listed
        listed.add(item['path']); path=package/relative
        assert not path.is_symlink()
        data=path.read_bytes()
        assert len(data)==item['size'] and hashlib.sha256(data).hexdigest()==item['sha256'],str(path)
    assert listed=={str(p.relative_to(package)) for p in package.rglob('*') if p.is_file() and p!=package/'manifest.json'}
    contents=package/'macOS/MH4U - Laboratorio.app/Contents'; assets=contents/'Resources/assets'
    mac_files={str(p.relative_to(assets)):p.read_bytes() for p in assets.rglob('*') if p.is_file()}
    win_assets=package/'Windows-x64/assets'
    assert mac_files=={str(p.relative_to(win_assets)):p.read_bytes() for p in win_assets.rglob('*') if p.is_file()}
    catalog=(assets/'catalog.tsv').read_text().splitlines()
    assert catalog[0]=='MH4U-LAB-CATALOG 1'
    texts=json.loads((assets/'texts/manifest.json').read_text())
    prefix=f"TEXT_CATALOG fragments={texts['fragment_count']} translated={texts['translated_count']}"
    records=[]

    def run(kind,command):
        result=subprocess.run(command,capture_output=True,text=True,timeout=30)
        assert result.returncode==0,(kind,result.stderr)
        records.append({'kind':kind,'command':command,'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
        return result.stdout

    for index in range(len(catalog)-1):
        stdout=run(f'resource-{index}',[str(contents/'MacOS/launch'),'--resource',str(index),'--inspect-only'])
        assert prefix in stdout
    recipe=assets/RECIPE_NAME
    world=run('world-launcher',[str(contents/'MacOS/launch'),'--world',str(recipe),'--hide-primitive','3','--inspect-only'])
    assert prefix in world and 'WORLD_PROFILE diagnostic-grounded-disk-v1' in world
    player_states=[]
    if manifest['milestone'] in ('resource-lab-0.15.0','resource-lab-0.16.0','resource-lab-0.17.0'):
        for state in (1,3):
            player=run(f'player-preview-state-{state}',[str(contents/'MacOS/launch'),'--world',str(recipe),
                '--player-preview',str(assets/'player-preview/constants.bin'),'--player-state',str(state),'--player-ticks','12','--inspect-only'])
            assert prefix in player and 'PLAYER_PROFILE diagnostic-player-driver-v1' in player and 'PLAYER_TICKS 12\n' in player
            assert f'PLAYER_REQUEST_STATE {state}\n' in player
            player_states.append(state)
    network=run('network-manifest',[str(contents/'MacOS/mh4u-network-lab'),'verify','--manifest',str(assets/'actor/session.manifest')])
    world_network=run('network-world',[str(contents/'MacOS/mh4u-network-lab'),'verify','--world',str(recipe)])
    motion=assets/'paired-motion'
    for index in range(7):
        output=run(f'paired-motion-{index}',[str(contents/'MacOS/mh4u-action-motion'),'--package',str(motion/'model-motion.bin'),
            '--paired-root',str(motion),'--action',str(index),'--ticks','3','--events-package',str(assets/'motion-events/motion-events.bin')])
        assert output.startswith('PROFILE\tdiagnostic-one-layer-events-v1\n') and output.count('\nTICK\t')==4 and output.count('\nEVENTS\t')==4
        viewed=run(f'paired-motion-viewer-{index}',[str(contents/'MacOS/launch'),'--motion-package',str(motion/'model-motion.bin'),
            '--paired-root',str(motion),'--motion-action',str(index),'--motion-ticks','3','--events-package',str(assets/'motion-events/motion-events.bin'),'--inspect-only'])
        assert prefix in viewed and 'MOTION_STATE\t3\t' in viewed and 'MOTION_MODEL_MATRIX\t' in viewed and 'MOTION_EVENTS\t3\t' in viewed
    report={'package':str(package),'milestone':manifest['milestone'],'files_sha256_verified':len(listed),
            'resources_loaded':len(catalog)-1,'native_catalog':texts,'cross_platform_resource_hashes_identical':True,
            'world_loaded':True,'player_preview_states':player_states,'records':records,'network_manifest':network,'network_world':world_network,
            'paired_actions_loaded':7,'paired_actions_composed_in_viewer':7,'paired_actions_with_event_tables':7,'windows_executed':False,
            'original_gameplay_fidelity_validated':False}
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report[k] for k in ('milestone','files_sha256_verified','resources_loaded','world_loaded')}))


if __name__=='__main__':
    main()
