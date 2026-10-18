#!/usr/bin/env python3
"""Inspect the observed SDL21 scene tracks without assuming gameplay semantics.

The layout is corroborated by RevilLib's x86 SDL records and the source files.
Bindings, timing and transforms require separate consumer evidence before export.
"""
import argparse
import copy
from collections import Counter, defaultdict
import json
import math
from pathlib import Path
import struct

import extract_model as resource
import index_resources as indexer

SCHEDULER = indexer.type_id('rScheduler')
MAX_TRACKS = 4096
MAX_KEYS = 65536
TRACK_NAMES = {1: 'root', 2: 'unit', 3: 'system', 4: 'separator',
               5: 'parameter', 6: 'integer', 7: 'vector', 8: 'float',
               9: 'boolean', 10: 'reference', 11: 'resource',
               12: 'string', 13: 'event', 14: 'hermite_curve'}
STRIDES = {6: 4, 7: 16, 8: 4, 9: 1, 10: 4, 11: 4, 12: 4, 13: 4, 14: 64}
LOADER_EVIDENCE = {
    'address': '0x16b670',
    'evidence_id': 'ev_cf15ef9ced602ea27e1024b71131db72bb80a268f7d954e8bf2287f8a4e70507',
    'retained_response_sha256': '43161739476955ce2fe347787c9a1e687917ba4136ea8e258cacab22b466519c',
    'subject_sha256': 'd4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435',
    'scope': '680 complete REA read bytes, independently disassembled by LLVM; no Ghidra function body or target execution',
}


def check(condition, message):
    if not condition:
        raise ValueError(message)


def parse_scheduler(data):
    check(len(data) <= 8*1024*1024, 'SDL exceeds 8 MiB inspection budget')
    magic, version, count, crc, frame_max, base, strings = resource.unpack('<4sHH4I', data, 0, 'SDL header')
    check(magic == b'SDL\0' and version == 21, 'Only observed little-endian SDL21 supported')
    check(0 < count <= MAX_TRACKS, 'SDL track count outside inspection budget')
    table_end = 24+24*count
    resource.region(data, 24, 24*count, 'SDL complete track table')
    check(table_end <= strings < len(data), 'SDL string table outside payload')
    check(base < count, 'SDL base track outside table')

    def cstring(offset):
        check(strings <= offset < len(data), 'SDL string pointer outside string table')
        end = data.find(b'\0', offset, min(len(data), offset+1025))
        check(end >= offset, 'SDL string lacks bounded terminator')
        return data[offset:end].decode('ascii')

    rows = [resource.unpack('<BBH5I', data, 24+i*24, 'SDL track') for i in range(count)]
    check(sum(r[2] for r in rows) <= MAX_KEYS, 'SDL cumulative key count exceeds inspection budget')
    tracks = []
    for i, (kind, prop, n, parent, name, hash_index, frames, values) in enumerate(rows):
        check(kind in TRACK_NAMES, 'Unsupported SDL track type')
        check(kind <= 4 or parent < i, 'SDL property parent must precede property')
        name_text = cstring(strings+name)
        check(kind > 5 or n == 0, 'Structural SDL track unexpectedly contains keys')
        keys = []
        if n:
            stride = STRIDES[kind]
            check(table_end <= frames <= strings and n*4 <= strings-frames,
                  'SDL key-time span outside data region')
            check(table_end <= values <= strings and n*stride <= strings-values,
                  'SDL value span outside data region')
            previous = -1
            for k in range(n):
                word = resource.unpack('<I', data, frames+4*k, 'SDL frame')[0]
                frame, mode = word & 0xffffff, word >> 24
                check(frame >= previous, 'SDL key frames decrease')
                previous = frame
                at = values+k*stride
                raw = resource.region(data, at, stride, 'SDL key data')
                key = {'index': k, 'frame': frame, 'mode_raw': mode,
                       'frame_offset': frames+4*k, 'value_offset': at,
                       'value_raw_hex': raw.hex()}
                if kind in (7, 8):
                    value = list(struct.unpack('<'+('4f' if kind == 7 else 'f'), raw))
                    check(all(math.isfinite(x) for x in value), 'Non-finite SDL numeric value')
                    key['vector_xyzw' if kind == 7 else 'float_values'] = value
                elif kind in (11, 12):
                    relative = struct.unpack('<I', raw)[0]
                    pointer = strings+relative
                    if relative == 0:
                        key['resource' if kind == 11 else 'string'] = None
                    elif kind == 11:
                        check(strings <= pointer <= len(data)-4, 'SDL resource pointer outside strings')
                        key['resource'] = {'type_id': struct.unpack_from('<I', data, pointer)[0],
                                           'name': cstring(pointer+4), 'offset': pointer}
                    else:
                        key['string'] = cstring(pointer)
                elif kind in (6, 10, 13):
                    key['u32_raw'] = struct.unpack('<I', raw)[0]
                elif kind == 9:
                    key['u8_raw'] = raw[0]
                keys.append(key)
        else:
            check(frames == 0 and values == 0, 'Empty SDL track has nonzero data pointers')
        tracks.append({'index': i, 'record_offset': 24+i*24,
                       'record_raw_hex': data[24+i*24:48+i*24].hex(),
                       'kind': TRACK_NAMES[kind], 'kind_raw': kind,
                       'property_type_raw': prop, 'key_count': n,
                       'parent_or_slot_raw': parent, 'name': name_text,
                       'hash_or_array_index_raw': hash_index,
                       'frames_offset': frames, 'data_offset': values, 'keys': keys})
    check(tracks[base]['kind'] == 'root', 'SDL base does not identify root track')
    return {'schema': 'mh4u-sdl21-inspection-v1', 'sha256': resource.sha(data),
            'header_raw_hex': data[:24].hex(), 'version': version,
            'track_count': count, 'crc_raw': crc, 'max_frame_raw': frame_max,
            'max_frame_low24': frame_max & 0xffffff, 'base_track': base,
            'strings_offset': strings, 'tracks': tracks,
            'unknowns': ['Runtime scheduling and frame rate', 'Key mode interpretation',
                         'Property setters, defaults and activation conditions']}


def inventory(index, entries, output):
    check(0 < len(entries) <= 32 and len(set(entries)) == len(entries), 'Select 1..32 distinct SDL entries')
    output.mkdir(parents=True, exist_ok=True)
    (output/'source').mkdir(exist_ok=True)
    connection, metadata = indexer.open_index(index)
    reports = []
    try:
        for entry in entries:
            path = output/'source'/f'entry-{entry}.bin'
            source = indexer.extract(connection, metadata, entry, path)
            check(source['entry']['type_id'] == SCHEDULER, 'Selected entry is not rScheduler')
            parsed = parse_scheduler(path.read_bytes())
            for track in parsed['tracks']:
                for key in track['keys']:
                    if 'resource' in key:
                        ref = key['resource']
                        if ref is None:
                            continue
                        matches = connection.execute('SELECT id FROM entries WHERE archive_id=? AND type_id=? AND name=?',
                            (source['entry']['archive_id'], ref['type_id'], ref['name'])).fetchall()
                        ref['same_archive_matches'] = [r[0] for r in matches]
            parsed['source'] = source
            resource.save(output/f'entry-{entry}.json', json.dumps(parsed, indent=2)+'\n')
            reports.append({'entry_id': entry, 'sha256': parsed['sha256'], 'tracks': parsed['track_count'],
                            'key_count': sum(t['key_count'] for t in parsed['tracks']),
                            'type_counts': dict(Counter(t['kind'] for t in parsed['tracks']))})
    finally:
        connection.close()
    report = {'schema': 'mh4u-sdl21-inventory-v1', 'source': metadata['source'], 'entries': reports,
              'loader_evidence': LOADER_EVIDENCE,
              'target_executed': False, 'producer_sha256': resource.sha(Path(__file__).read_bytes())}
    resource.save(output/'inventory.json', json.dumps(report, indent=2)+'\n')
    return report


def surface_components(core):
    """Exact-index edge connectivity; not navigation or game surface semantics."""
    vertices = core['vertices_xyzw']
    faces = core['faces']
    edges = defaultdict(list)
    for face in faces:
        ids = face['vertices']
        for a, b in zip(ids, ids[1:]+ids[:1]):
            edges[tuple(sorted((a, b)))].append(face['index'])
    adjacent = [set() for _ in faces]
    for ids in edges.values():
        for i in ids:
            adjacent[i].update(j for j in ids if j != i)
    unseen = set(range(len(faces)))
    components = []
    while unseen:
        pending = [min(unseen)]; selected = set(pending); unseen.remove(pending[0])
        while pending:
            for other in adjacent[pending.pop()] & unseen:
                unseen.remove(other); selected.add(other); pending.append(other)
        points = [vertices[j][:3] for i in selected for j in faces[i]['vertices']]
        components.append({'faces': sorted(selected), 'face_count': len(selected),
                           'bbox': {key: [fn(p[k] for p in points) for k in range(3)]
                                    for key, fn in [('min', min), ('max', max)]},
                           'source_normal_y_min': min(faces[i]['normal'][1] for i in selected),
                           'source_normal_y_max': max(faces[i]['normal'][1] for i in selected)})
    return {'components': components, 'component_count': len(components),
            'boundary_edges': [list(e) for e, owners in edges.items() if len(owners) == 1],
            'nonmanifold_edges': [list(e) for e, owners in edges.items() if len(owners) > 2],
            'basis': 'Shared source vertex indices and undirected triangle edges; no welding or adjustment',
            'unknowns': ['Walkability and collision flags', 'Player spawn and area transitions']}


def first_key_transform(scheduler):
    units = [t for t in scheduler['tracks'] if t['kind'] == 'unit']
    check(len(units) == 1 and units[0]['hash_or_array_index_raw'] == indexer.type_id('uModel'),
          'Snapshot requires one evidenced uModel track')
    unit = units[0]
    properties = defaultdict(list)
    for track in scheduler['tracks']:
        if track['kind_raw'] > 4 and track['parent_or_slot_raw'] == unit['index']:
            properties[track['name']].append(track)
    def prop(name, kind, value_count, property_type):
        tracks = properties[name]
        check(len(tracks) == 1 and tracks[0]['kind'] == kind
              and tracks[0]['property_type_raw'] == property_type and len(tracks[0]['keys']) == value_count,
              'Snapshot property missing, duplicated or outside supported profile: '+name)
        return tracks[0]
    position = prop('mPos', 'vector', 1, 0x14)
    scale = prop('mScale', 'vector', 1, 0x14)
    angle = prop('mAngle', 'vector', 2, 0x14)
    prop('mQuat', 'vector', 0, 0x16)
    model = prop('mpModel', 'resource', 1, 0x80)
    for track in (position, scale, angle, model):
        check(track['keys'][0]['frame'] == 0, 'Snapshot requires an authored first key at frame zero')
    check(all(x == 0 for x in angle['keys'][0]['vector_xyzw']),
          'Nonzero Euler angles require independently validated ordering; not exported')
    p = position['keys'][0]['vector_xyzw']; s = scale['keys'][0]['vector_xyzw']
    check(p[3] == s[3] == 0 and all(v == 1 for v in s[:3]),
          'Snapshot only accepts observed unit scale and vector padding')
    reference = model['keys'][0]['resource']
    check(isinstance(reference, dict) and reference['type_id'] == resource.MODEL,
          'Snapshot model property must reference rModel')
    return {'translation': p[:3], 'rotation': [0, 0, 0, 1], 'scale': s[:3],
            'model_reference': reference, 'unit_track': unit['index'],
            'property_tracks': {t['name']: t['index'] for t in (position, scale, angle, model)},
            'source_frame': 0, 'scope': 'Snapshot of explicitly authored first keys; no scheduler playback'}


def compose_snapshot(index, scheduler_entry, primary, detail, output):
    output.mkdir(parents=True, exist_ok=True); (output/'source').mkdir(exist_ok=True)
    connection, metadata = indexer.open_index(index)
    try:
        path = output/'source'/f'entry-{scheduler_entry}.bin'
        source = indexer.extract(connection, metadata, scheduler_entry, path)
        check(source['entry']['type_id'] == SCHEDULER, 'Selected entry is not rScheduler')
        parsed = parse_scheduler(path.read_bytes()); transform = first_key_transform(parsed)
        reference = transform['model_reference']
        matches = connection.execute('SELECT id FROM entries WHERE archive_id=? AND name=? AND type_id=?',
            (source['entry']['archive_id'], reference['name'], reference['type_id'])).fetchall()
        check(len(matches) == 1, 'Scheduler model reference lacks unique same-archive match')
        detail_entry = matches[0][0]
    finally:
        connection.close()
    doc = {'asset': {'version': '2.0', 'generator': 'mh4u-native stage_composition.py'},
           'scene': 0, 'scenes': [{'nodes': [0, 1]}],
           **{k: [] for k in ['nodes', 'meshes', 'buffers', 'bufferViews', 'accessors',
                              'materials', 'textures', 'images', 'samplers']},
           'extras': {'scope': 'Static snapshot of authored SDL first keys, not observed initial gameplay state',
                      'spawn': 'unknown', 'sourceFrame': 0,
                      'shader': 'Neutral opaque preview; original lighting/alpha/visibility unknown'}}
    inputs, files, totals = [], [], Counter()
    for index_in_scene, directory in enumerate((primary, detail)):
        check((directory/'scene.gltf').stat().st_size <= 8*1024*1024, 'Source glTF metadata exceeds budget')
        src = json.loads((directory/'scene.gltf').read_text())
        provenance = json.loads((directory/'provenance.json').read_text())
        check(provenance.get('schema') == 'mh4u-static-stage-v1', 'Requires validated static stage producer')
        mod = provenance['members'][0]['entry']
        check(mod['type_id'] == resource.MODEL and mod['archive_id'] == source['entry']['archive_id'],
              'Model and scheduler must share an archive')
        if index_in_scene == 1:
            check(mod['id'] == detail_entry, 'Detail model differs from the scheduler reference')
        check(src['nodes'] == [{'mesh': 0, 'name': 'source_coordinates'}] and len(src['meshes']) == 1
              and len(src['buffers']) == 1 and not src.get('animations') and not src.get('skins'),
              'Source does not match static stage producer profile')
        expected = {Path(o['path']).name: o['sha256'] for o in provenance['outputs']}
        data_files = [src['buffers'][0]['uri']]+[v['uri'] for v in src['images']]
        data_files += [f'texture-{t["entry_id"]}-alpha.pgm' for t in provenance['textures']]
        for name in ['scene.gltf']+data_files:
            check(Path(name).name == name and name in expected, 'Unsafe or unrecorded source filename')
            check((directory/name).stat().st_size <= 64*1024*1024, 'Source dependency exceeds budget')
            check(resource.sha((directory/name).read_bytes()) == expected[name], 'Source export digest changed')
        prefixes = {k: len(doc[k]) for k in ['meshes', 'buffers', 'bufferViews', 'accessors',
                                           'materials', 'textures', 'images', 'samplers']}
        source_copy = copy.deepcopy(src)
        prefix = f'entry-{mod["id"]}-'
        for name in data_files:
            files.append((prefix+name, (directory/name).read_bytes()))
        for buffer in source_copy['buffers']:
            buffer['uri'] = prefix+buffer['uri']
        for view in source_copy['bufferViews']:
            view['buffer'] += prefixes['buffers']
        for accessor in source_copy['accessors']:
            accessor['bufferView'] += prefixes['bufferViews']
        for image in source_copy['images']:
            image['uri'] = prefix+image['uri']
        for texture in source_copy['textures']:
            texture['source'] += prefixes['images']; texture['sampler'] += prefixes['samplers']
        for material in source_copy['materials']:
            material['pbrMetallicRoughness']['baseColorTexture']['index'] += prefixes['textures']
        for primitive in source_copy['meshes'][0]['primitives']:
            primitive['indices'] += prefixes['accessors']
            primitive['attributes'] = {k: v+prefixes['accessors'] for k, v in primitive['attributes'].items()}
            primitive['material'] += prefixes['materials']
            primitive['extras']['sourceEntry'] = mod['id']
        for k in prefixes:
            doc[k].extend(source_copy[k])
        node = {'mesh': prefixes['meshes'], 'name': mod['name'],
                'extras': {'sourceEntry': mod['id'], 'sourceCoordinatesPreserved': True}}
        if index_in_scene == 1:
            node.update({k: transform[k] for k in ['translation', 'rotation', 'scale']})
            node['extras']['schedulerFirstKey'] = transform
        doc['nodes'].append(node)
        inputs.append({'directory': str(directory.resolve()), 'entry_id': mod['id'],
                       'scene_sha256': expected['scene.gltf'], 'copied_dependencies': data_files,
                       'table_offsets': prefixes})
        totals.update(provenance['totals'])
        files.append((prefix+'provenance.json', (directory/'provenance.json').read_bytes()))
    files.append(('scene.gltf', json.dumps(doc, indent=2)+'\n'))
    outputs = [resource.save(output/name, content) for name, content in files]
    report = {'schema': 'mh4u-stage-first-key-snapshot-v1', 'source': metadata['source'],
              'producer_sha256': resource.sha(Path(__file__).read_bytes()),
              'loader_evidence': LOADER_EVIDENCE,
              'scheduler_source': source, 'scheduler': parsed, 'selected_transform': transform,
              'inputs': inputs, 'outputs': outputs, 'totals': dict(totals),
              'reflection_evidence': {'procedure': '0x6d33a8',
                'evidence_id': 'ev_475ea503f38ac2ef74e78667782814bfa2b2230357bfbe3e0a3411fef52362ee',
                'subject_sha256': 'd4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435'},
              'angle_evidence': [
                {'procedure': '0x6d336c',
                 'evidence_id': 'ev_0bac0a409ff231e493047c0d6548d7c6b906c8b85b844f5defe5f1d785448e44',
                 'retained_response_sha256': '025cde8b331858d5bbe932487c6c61ebe825392f0864dfa71304db74469eec08'},
                {'procedure': '0x6d4080',
                 'evidence_id': 'ev_cad792b05e8b3919087275f4869856a1cb58fdd33b8075bf2c5cd2b1fe6216ea',
                 'retained_response_sha256': '8b875a140c18445516ae90c5107a99405db5e14d2612d946bd0c511a81a754b3'}],
              'target_executed': False, 'unknowns': ['Original initial gameplay state', 'Spawn',
                'SDL timing and interpolation modes', 'Visibility, alpha, lighting and shaders']}
    resource.save(output/'provenance.json', json.dumps(report, indent=2)+'\n')
    return {'output': str(output.resolve()), 'totals': dict(totals), 'transform': transform,
            'scene_sha256': resource.sha((output/'scene.gltf').read_bytes())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--index', type=Path, required=True)
    parser.add_argument('--entry', type=int, action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--primary', type=Path)
    parser.add_argument('--detail', type=Path)
    args = parser.parse_args()
    if args.primary or args.detail:
        check(args.primary is not None and args.detail is not None and len(args.entry) == 1,
              'Snapshot requires both source directories and one scheduler entry')
        result = compose_snapshot(args.index, args.entry[0], args.primary, args.detail, args.output)
    else:
        result = inventory(args.index, args.entry, args.output)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
