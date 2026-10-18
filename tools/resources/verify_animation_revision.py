#!/usr/bin/env python3
"""Compare a preserved sampler revision with current source-bound exports."""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct

import animation_probe as current

ARTIFACTS = ['assets/local/animation-probe/em062-slot002', 'assets/local/animation-probe/em062-actions',
             'assets/local/player-probe/body001-actions', 'assets/local/player-assembly-probe/male001-actions']
SOURCES = [f'research/private/animation-probe/indexed/entry-{i}.lmt' for i in (9521, 9522, 9523)] + [
    f'assets/local/player-probe/inventory/source/entry-{i}.bin' for i in (54913, 92608)]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(root, before, artifacts_root=None):
    artifacts_root = artifacts_root or root
    spec = importlib.util.spec_from_file_location('preserved_motion_decoder', before/'tools/resources/animation_probe.py')
    previous = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(previous)
    checks, outputs = [], []
    def check(label, condition):
        if not condition:
            raise AssertionError(label)
        checks.append(label)
    changed_keys = changed_tracks = buffered_tracks = shifted_tracks = 0
    max_change = 0.0
    for source in SOURCES:
        raw = (root/source).read_bytes();old = previous.parse_lmt(raw);new = current.parse_lmt(raw)
        check(source+': source/action identity preserved', old['sha256'] == new['sha256'] and
              len(old['actions']) == len(new['actions']))
        for a, b in zip(old['actions'], new['actions']):
            if a['status'] == 'empty':
                check(source+f'/{a["slot"]}: empty preserved', b == a)
                continue
            check(source+f'/{a["slot"]}: header and track count unchanged', a['header_raw_hex'] == b['header_raw_hex'] and len(a['tracks']) == len(b['tracks']))
            for x, y in zip(a['tracks'], b['tracks']):
                check(source+f'/{a["slot"]}/{x["ordinal"]}: track provenance unchanged', all(x[k] == y[k] for k in
                      ['record_raw_hex', 'buffer_sha256', 'extremes_scale_bias', 'reference_xyzw_or_xyz', 'bone_id', 'path']))
                check(source+f'/{a["slot"]}/{x["ordinal"]}: stored key timing unchanged',
                      [(k['frame'], k['next_frame_delta'], k['buffer_offset']) for k in x['keys']] ==
                      [(k['frame'], k['next_frame_delta'], k['buffer_offset']) for k in y['keys']])
                if x['keys']:
                    buffered_tracks += 1
                    shifted_tracks += int(a['loop_frame'] < 1)
                if x['codec'] in (6, 7, 11, 12, 13):
                    changed_tracks += 1
                    for old_key, new_key in zip(x['keys'], y['keys']):
                        difference = max(abs(u-v) for u, v in zip(old_key['value'], new_key['value']))
                        changed_keys += int(difference != 0)
                        max_change = max(max_change, difference)
                else:
                    check(source+f'/{a["slot"]}/{x["ordinal"]}: other codec equations unchanged', x['keys'] == y['keys'])
    for relative in ARTIFACTS:
        directory, old_directory = artifacts_root/relative, before/relative
        doc = json.loads((directory/'animated.gltf').read_text());old = json.loads((old_directory/'animated.gltf').read_text())
        report = json.loads((directory/'animation-provenance.json').read_text())
        old_report = json.loads((old_directory/'animation-provenance.json').read_text())
        check(relative+': clip names/order/targets unchanged',
              [x['name'] for x in doc['animations']] == [x['name'] for x in old['animations']] and
              [x['channels'] for x in doc['animations']] == [x['channels'] for x in old['animations']])
        check(relative+': rejection identities/reasons unchanged', report.get('rejected_actions') == old_report.get('rejected_actions'))
        check(relative+': node/skin/material unchanged', all(doc.get(k) == old.get(k) for k in ['nodes', 'skins', 'materials', 'meshes', 'images', 'textures']))
        for path in sorted(directory.glob('model-*')):
            check(relative+'/'+path.name+': model dependency byte-identical', path.read_bytes() == (old_directory/path.name).read_bytes())
        data = (directory/'animation.bin').read_bytes()
        def values(index):
            accessor = doc['accessors'][index];view = doc['bufferViews'][accessor['bufferView']]
            width = {'SCALAR':1, 'VEC3':3, 'VEC4':4}[accessor['type']]
            start = view.get('byteOffset', 0)+accessor.get('byteOffset', 0)
            return list(struct.iter_unpack('<'+'f'*width, data[start:start+width*4*accessor['count']]))
        durations = []
        for clip in doc['animations']:
            end = (clip['extras']['source_frame_count']-1)/clip['extras']['export_fps'];durations.append(end)
            for sampler in clip['samplers']:
                times = [x[0] for x in values(sampler['input'])];rows = values(sampler['output'])
                check(relative+'/'+clip['name']+': complete bounded channel domain', times[0] == 0 and
                      abs(times[-1]-end) < 1e-6 and all(x<y for x,y in zip(times,times[1:])) and
                      len(rows) == len(times)*(3 if sampler['interpolation']=='CUBICSPLINE' else 1) and all(math.isfinite(x) for row in rows for x in row))
        outputs.append({'directory':relative, 'animations':len(doc['animations']),
                        'duration_range_seconds':[min(durations),max(durations)],
                        'changed_files': [name for name in ['animated.gltf','animation.bin','animation-provenance.json']
                                          if (directory/name).read_bytes() != (old_directory/name).read_bytes()],
                        'old_gltf_sha256':digest(old_directory/'animated.gltf'), 'gltf_sha256':digest(directory/'animated.gltf'),
                        'old_animation_sha256':digest(old_directory/'animation.bin'), 'animation_sha256':digest(directory/'animation.bin')})
    return {'status':'passed','checks':len(checks),'decoder_profile':current.DECODER_PROFILE,
            'current_producer_sha256':digest(Path(current.__file__)), 'previous_producer_sha256':digest(Path(previous.__file__)),
            'raw_corpus_comparison':{'quant_quaternion_tracks':changed_tracks, 'keys_with_changed_values':changed_keys,
                                    'max_component_change_source_units':max_change,'buffered_tracks':buffered_tracks},
            'artifacts':outputs,'target_executed':False,
            'scope':'Source decoder and export comparison, not original-game playback equivalence'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path.cwd())
    parser.add_argument('--before',type=Path,required=True)
    parser.add_argument('--artifacts-root',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args();report = verify(args.root,args.before,args.artifacts_root)
    current.save(args.output,json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()
