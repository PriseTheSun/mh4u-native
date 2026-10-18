#!/usr/bin/env python3
"""Read back a source-bound assembly and optional animation; never executes the original game."""
import argparse
import json
from pathlib import Path
import struct

import extract_model as resource
import model_scene as model


def verify(directory, animated=None, original_animated=None):
    document = json.loads((directory/'scene.gltf').read_text())
    binary = (directory/'scene.bin').read_bytes()
    report = json.loads((directory/'provenance.json').read_text())
    checks = []
    def check(label, condition):
        if not condition:
            raise AssertionError(label)
        checks.append(label)
    def values(primitive, semantic):
        return model.accessor_values(document, binary, primitive['attributes'][semantic])
    def floats(rows):
        return [struct.unpack('<'+'f'*len(row), struct.pack('<'+'f'*len(row), *row)) for row in rows]
    accepted = report['accepted']
    check('explicit accepted/rejected/inspection coverage',
          set([report['base_entry_id'], *report['explicit_piece_entries']]) ==
          {r['entry_id'] for r in accepted+report['rejected']} and
          set(report['explicit_inspect_entries']) == {r['entry_id'] for r in report['inspections_not_exported']})
    check('rejected pieces excluded from glTF', not ({r['entry_id'] for r in report['rejected']} &
          {mesh['extras']['sourceEntryId'] for mesh in document['meshes']}))
    check('inspection-only alternatives excluded from glTF', not (set(report['explicit_inspect_entries']) &
          {mesh['extras']['sourceEntryId'] for mesh in document['meshes']}))
    check('serialized bind validation', model.validate_scene(document, binary)['max_bind_position_error_source_units'] < 1e-3)
    for record in accepted:
        identifier = record['entry_id'];raw = (directory/'source'/str(identifier)/f'entry-{identifier}.bin').read_bytes()
        check(f'{identifier}: source digest', resource.sha(raw) == record['source_sha256'])
        header = resource.unpack('<4s4H13I', raw, 0, 'MOD header')
        source = model.parse_actor(raw, list(range(header[3])), allow_multiple_materials=True,
                                  allow_two_influence_vertices=True, allow_single_joint=True)
        mapping = {j['source_joint_index']: j['target_joint_index'] for j in record['association']['joints']}
        exported = [mesh for mesh in document['meshes'] if mesh['extras']['sourceEntryId'] == identifier]
        check(f'{identifier}: complete primitive accounting', len(exported) == len(source['meshes']))
        for mesh in source['meshes']:
            match = [m for m in exported if m['extras']['sourcePrimitiveIndex'] == mesh['primitive_index']]
            check(f'{identifier}/{mesh["primitive_index"]}: unique original primitive', len(match) == 1)
            p = match[0]['primitives'][0]
            for semantic, field in [('POSITION','positions'),('NORMAL','normals'),('TEXCOORD_0','uvs'),('WEIGHTS_0','weights')]:
                check(f'{identifier}/{mesh["primitive_index"]}: {semantic} float32 unchanged', values(p, semantic) == floats(mesh[field]))
            check(f'{identifier}/{mesh["primitive_index"]}: exact joint remap',
                  values(p, 'JOINTS_0') == [tuple(mapping[index] if weight else 0 for index,weight in zip(row,weights))
                                           for row,weights in zip(mesh['joints'],mesh['weights'])])
            check(f'{identifier}/{mesh["primitive_index"]}: exact triangles',
                  model.accessor_values(document, binary, p['indices']) == [(i,) for face in mesh['triangles'] for i in face])
            material = document['materials'][p['material']]
            binding = record['materials']['bindings'][mesh['material_index']]
            check(f'{identifier}/{mesh["primitive_index"]}: exact MRL material', material['name'] == binding['name'])
            image = document['images'][document['textures'][material['pbrMetallicRoughness']['baseColorTexture']['index']]['source']]
            selected_texture = next(t for t in record['textures'] if t['name'] == binding['texture_name'])
            check(f'{identifier}/{mesh["primitive_index"]}: exact MRL texture entry', image['uri'] == f'texture-{selected_texture["entry_id"]}.png')
    if animated:
        current = json.loads(animated.read_text());old = json.loads(original_animated.read_text())
        def accessor(doc, path, index):
            a = doc['accessors'][index];v = doc['bufferViews'][a['bufferView']]
            data = (path.parent/doc['buffers'][v['buffer']]['uri']).read_bytes()
            width = {'SCALAR':1,'VEC3':3,'VEC4':4}[a['type']];assert a['componentType'] == 5126
            start = v.get('byteOffset',0)+a.get('byteOffset',0)
            return data[start:start+a['count']*4*width]
        check('animation count preserved from source body export', len(current['animations']) == len(old['animations']))
        check('same nineteen rest joints and hierarchy', current['nodes'][:19] == old['nodes'][:19])
        check('animation keeps complete assembly geometry buffer',
              (animated.parent/current['buffers'][0]['uri']).read_bytes() == binary)
        for a, b in zip(current['animations'], old['animations']):
            check(a['name']+': channel targets unchanged', a['name'] == b['name'] and a['channels'] == b['channels'])
            check(a['name']+': timing and TRS bytes unchanged', len(a['samplers']) == len(b['samplers']) and all(
                x['interpolation'] == y['interpolation'] and all(accessor(current, animated, x[k]) == accessor(old, original_animated, y[k])
                for k in ('input','output')) for x,y in zip(a['samplers'],b['samplers'])))
    return {'status':'passed','checks':checks,'count':len(checks),
            'basis':'Decoded source MOD compared to serialized glTF accessors and original body animation export',
            'target_executed':False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assembly',type=Path,required=True)
    parser.add_argument('--animated',type=Path)
    parser.add_argument('--original-animated',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args()
    if bool(args.animated) != bool(args.original_animated):
        parser.error('--animated and --original-animated must be supplied together')
    report = verify(args.assembly,args.animated,args.original_animated)
    resource.save(args.output,json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k != 'checks'},indent=2))


if __name__ == '__main__':
    main()
