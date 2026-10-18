#!/usr/bin/env python3
"""Extract the evidenced geometric core of MH4U SBC collision resources.

No target execution or physical simulation. SBC 2010091000 geometric core only;
acceleration trees and collision semantics are retained but not interpreted.
See docs/collision-findings.md for source/REA evidence and uncertainty.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import struct

from extract_model import MAX_MEMBER, region, save, sha, unpack
from index_resources import extract, open_index, type_id

VERSION = 2010091000
COLLISION = type_id('rCollision')
# Conservative accounting for Python dictionaries, JSON, OBJ, and glTF copies.
# This is a tool memory budget, not a claimed format limit.
REPRESENTATION_BUDGET = 128 * 1024 * 1024
EVIDENCE = {
    'loader_address': '0x16014c',
    'evidence_id': 'ev_01511f30730443caddfd86a170c3c2bd77261ffc9c5b2a974172ffbb5c47e2a6',
    'analysis_subject_sha256': 'd4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435',
    'provider': 'Ghidra 12.1.4; ARMv6; ephemeral-source-immutable',
    'observation_kind': 'static decompilation; no game execution',
    'consumer_address': '0x16c380',
    'consumer_evidence_id': 'ev_f9126f947889e85fb0e7110e96cdfe6b20860f269c228d343d0d166f319ee71e',
    'corroborating_format': 'https://github.com/Brachi/albam/blob/d1fda41beeae129fbb26128157511afdee4002ea/albam/engines/mtfw/structs/sbc-211.ksy',
    'corroborating_format_sha256': '22638e689d1fdd1ab508d13704a6f2ef0a698cfb9314bac11e77ce53dea91bea',
}


def finite(values, label):
    if not all(math.isfinite(v) for v in values):
        raise ValueError(f'{label}: non-finite float')


def checked_bbox(values, label):
    finite(values, label)
    if any(values[i] > values[i+4] for i in range(3)):
        raise ValueError(f'{label}: inverted bounds')
    return {'min': list(values[:4]), 'max': list(values[4:])}


def parse_collision(data):
    """Return source values and validation, not inferred walkability or physics."""
    if len(data) > MAX_MEMBER:
        raise ValueError('SBC exceeds the shared 64 MiB member bound')
    h = unpack('<4s3I2H5I8f', data, 0, 'SBC header')
    if h[0] != b'SBC\xff' or h[1] != VERSION:
        raise ValueError('Unsupported SBC magic/version (requires SBC ff / 2010091000)')
    group_count, record_count, pair_count, face_count, vertex_count = h[4:9]
    if not group_count or not face_count or not vertex_count:
        raise ValueError('Empty collision geometry is not exportable')
    counts = (group_count, pair_count, face_count, vertex_count, record_count)
    strides = (72, 10, 32, 16, 32)
    names = ('groups', 'pairs', 'faces', 'vertices', 'attribute_records')
    spans = {}; offset = 72
    # Check every byte extent before count-based collection allocations.
    for name, count, stride in zip(names, counts, strides):
        size = count*stride
        region(data, offset, size, name)
        spans[name] = {'offset': offset, 'count': count, 'stride': stride, 'size': size}
        offset += size
    estimate = (len(data)*4 + group_count*4096 + pair_count*1024 + face_count*4096 +
                vertex_count*1024 + record_count*512)
    if estimate > REPRESENTATION_BUDGET:
        raise ValueError('Decoded/export representation exceeds 128 MiB accounting budget')
    bbox = checked_bbox(h[11:], 'header bbox')
    groups = []
    for i in range(group_count):
        g = unpack('<8f10I', data, 72+i*72, 'group')
        group = {'index': i, 'raw_hex': data[72+i*72:144+i*72].hex(),
                 'bbox': checked_bbox(g[:8], 'group bbox'),
                 'unknown_loader_zeroed_u32_20_24_28': list(g[8:11]),
                 'pair_start': g[11], 'pair_count': g[12],
                 'face_start': g[13], 'face_count': g[14],
                 'vertex_start': g[15], 'vertex_count': g[16], 'index_id_raw': g[17]}
        groups.append(group)
    # Establish a unique owner before interpreting any group-relative indices.
    owners = {}
    for kind, total in [('pair', pair_count), ('face', face_count), ('vertex', vertex_count)]:
        owners[kind] = [None]*total
        for group in groups:
            start, count = group[kind+'_start'], group[kind+'_count']
            if start > total or count > total-start:
                raise ValueError(f'Group {kind} span out of bounds')
            for j in range(start, start+count):
                if owners[kind][j] is not None:
                    raise ValueError(f'Overlapping group {kind} spans')
                owners[kind][j] = group['index']
        if any(v is None for v in owners[kind]):
            raise ValueError(f'Groups do not cover complete {kind} array')
    vertices = [list(unpack('<4f', data, spans['vertices']['offset']+16*i, 'vertex'))
                for i in range(vertex_count)]
    for v in vertices:
        finite(v, 'vertex')
    extrema = {'min': [min(v[k] for v in vertices) for k in range(3)],
               'max': [max(v[k] for v in vertices) for k in range(3)]}
    # Allow a few float32 ulps when checking exported source coordinates.
    tolerance = max(1.0, *(abs(v) for v in extrema['min']+extrema['max']))*1e-6
    boxes = [('header', bbox, extrema)]
    for group in groups:
        subset = vertices[group['vertex_start']:group['vertex_start']+group['vertex_count']]
        if subset:
            boxes.append((f'group {group["index"]}', group['bbox'],
                          {'min': [min(v[k] for v in subset) for k in range(3)],
                           'max': [max(v[k] for v in subset) for k in range(3)]}))
    for label, box, ext in boxes:
        if any(ext['min'][k] < box['min'][k]-tolerance or
               ext['max'][k] > box['max'][k]+tolerance for k in range(3)):
            raise ValueError(f'Vertices escape {label} bbox')
    faces = []; normal_errors = []; normal_dots = []; degenerate = []
    for i in range(face_count):
        row = unpack('<3f4HI4BI', data, spans['faces']['offset']+32*i, 'face')
        group = groups[owners['face'][i]]
        normal = row[:3]; local_indices = row[3:6]
        finite(normal, 'face normal')
        if any(j >= group['vertex_count'] for j in local_indices):
            raise ValueError('Face vertex index outside group vertex array')
        if row[6] >= record_count:
            raise ValueError('Face attribute index outside global attribute array')
        # REA 0x16c380 explicitly adds group.vertex_start before local indices.
        indices = [group['vertex_start']+j for j in local_indices]
        nl = math.sqrt(sum(v*v for v in normal))
        if nl == 0:
            raise ValueError('Zero source face normal')
        normal_errors.append(abs(nl-1))
        a, b, c = (vertices[j] for j in indices)
        u = [b[k]-a[k] for k in range(3)]; v = [c[k]-a[k] for k in range(3)]
        cross = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
        area2 = math.sqrt(sum(x*x for x in cross))
        if area2 == 0:
            degenerate.append(i)
        else:
            normal_dots.append(sum(cross[k]*normal[k]/(area2*nl) for k in range(3)))
        faces.append({'index': i, 'group': group['index'], 'normal': list(normal),
                      'vertices_local_raw': list(local_indices), 'vertices': indices,
                      'attribute_id_raw': row[6], 'unknown_u32_14': row[7],
                      'adjacency_candidate_u8': list(row[8:11]),
                      'unknown_u8_1b': row[11], 'unknown_u32_1c': row[12]})
    pairs = []
    for i in range(pair_count):
        row = unpack('<HH4BH', data, spans['pairs']['offset']+10*i, 'pair')
        group = groups[owners['pair'][i]]
        if row[0] >= group['face_count'] or (row[1] != 0xffff and row[1] >= group['face_count']):
            raise ValueError('Pair face index outside group face array')
        pairs.append({'index': i, 'group': group['index'], 'face_a_local_raw': row[0], 'face_b_local_raw': row[1],
                      'quad_order_candidate_u8': list(row[2:6]), 'attribute_id_raw': row[6]})
    records = [region(data, spans['attribute_records']['offset']+i*32, 32, 'attribute').hex()
               for i in range(record_count)]
    return {
        'schema': 'mh4u-sbc-core-v2', 'version': h[1], 'source_size': len(data),
        'source_sha256': sha(data), 'header_raw_hex': data[:72].hex(),
        'global_tree_type_raw': h[2], 'group_tree_type_raw': h[3],
        'unknown_header_u32_20_24': list(h[9:11]), 'bbox': bbox, 'spans': spans,
        'groups': groups,
        'vertices_xyzw': vertices, 'faces': faces, 'pairs': pairs,
        'attribute_records_raw_hex': records,
        'unparsed_tail': {'offset': offset, 'size': len(data)-offset,
                          'sha256': sha(data[offset:]),
                          'meaning': 'Per-group and global spatial trees and associated pair data; not decoded'},
        'validation': {'vertex_bounds': extrema, 'bbox_tolerance_source_units': tolerance,
                       'all_face_and_pair_indices_in_bounds': True,
                       'all_face_attribute_indices_in_bounds': True,
                       'group_ranges_partition_arrays': True,
                       'max_source_normal_length_error': max(normal_errors),
                       'minimum_winding_normal_dot': min(normal_dots) if normal_dots else None,
                       'degenerate_face_indices': degenerate,
                       'face_attribute_histogram_raw': dict(sorted(Counter(f['attribute_id_raw'] for f in faces).items())),
                       'pair_attribute_histogram_raw': dict(sorted(Counter(p['attribute_id_raw'] for p in pairs).items()))},
        'limitations': ['No acceleration-tree parsing or complete-file semantic validation',
                       'No mapping of attribute IDs to walkability, friction, damage, camera, or gameplay rules',
                       'Coordinates and winding preserved; units and world placement are not established',
                       'Group transforms beyond stored source coordinates are not established'],
    }


def make_obj(core):
    lines = ['# Real SBC triangle geometry; source units; no inferred physical material.']
    lines += ['v '+' '.join(format(v, '.9g') for v in point[:3]) for point in core['vertices_xyzw']]
    lines += ['vn '+' '.join(format(v, '.9g') for v in face['normal']) for face in core['faces']]
    lines += ['f '+' '.join(f'{j+1}//{i+1}' for j in face['vertices']) for i, face in enumerate(core['faces'])]
    return '\n'.join(lines)+'\n'


def make_gltf(core):
    # Expand triangles to retain each real source face normal. The normalized
    # preview normals do not replace the unmodified normals in the source JSON.
    positions = []; normals = []
    for face in core['faces']:
        n = face['normal']; length = math.sqrt(sum(v*v for v in n))
        for i in face['vertices']:
            positions.extend(core['vertices_xyzw'][i][:3])
            normals.extend(v/length for v in n)
    position_bytes = struct.pack('<'+'f'*len(positions), *positions)
    normal_bytes = struct.pack('<'+'f'*len(normals), *normals)
    binary = position_bytes+normal_bytes
    count = len(positions)//3
    doc = {
        'asset': {'version': '2.0', 'generator': 'mh4u-native collision_probe.py'},
        'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'name': 'SBC geometric core', 'mesh': 0}],
        'buffers': [{'uri': 'scene.bin', 'byteLength': len(binary)}],
        'bufferViews': [{'buffer': 0, 'byteOffset': 0, 'byteLength': len(position_bytes), 'target': 34962},
                        {'buffer': 0, 'byteOffset': len(position_bytes), 'byteLength': len(normal_bytes), 'target': 34962}],
        'accessors': [{'bufferView': 0, 'componentType': 5126, 'count': count, 'type': 'VEC3',
                       'min': [min(positions[k::3]) for k in range(3)],
                       'max': [max(positions[k::3]) for k in range(3)]},
                      {'bufferView': 1, 'componentType': 5126, 'count': count, 'type': 'VEC3'}],
        'meshes': [{'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1}, 'mode': 4, 'material': 0}]}],
        'materials': [{'name': 'Diagnostic gray (not a source game material)', 'doubleSided': True,
                       'pbrMetallicRoughness': {'baseColorFactor': [0.6, 0.65, 0.7, 1], 'metallicFactor': 0, 'roughnessFactor': 1}}],
        'extras': {'sourceFormat': 'SBC', 'sourceSha256': core['source_sha256'],
                   'sourceCoordinatesPreserved': True, 'previewNormalRenormalization': True,
                   'sourceTriangleCount': len(core['faces']), 'physicalSemantics': 'unknown'},
    }
    return doc, binary


def json_bytes(value):
    return json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False)+'\n'


def inventory(index):
    connection, meta = open_index(index)
    try:
        rows = [dict(r) for r in connection.execute('''SELECT e.id,e.name,a.path archive,
                    e.uncompressed_size,e.compressed_sha256 FROM entries e JOIN archives a
                    ON a.id=e.archive_id WHERE e.type_id=? ORDER BY e.id''', (COLLISION,))]
        return {'schema': 'mh4u-collision-inventory-v1', 'source_sha256_from_index': meta['source']['sha256'],
                'type_name': 'rCollision', 'type_id': hex(COLLISION), 'count': len(rows),
                'coverage': 'Every rCollision directory entry in complete index; payloads not decoded by inventory',
                'entries': rows}
    finally:
        connection.close()


def export(index, entry_id, output):
    connection, meta = open_index(index)
    try:
        row = connection.execute('SELECT type_id FROM entries WHERE id=?', (entry_id,)).fetchone()
        if row is None or row['type_id'] != COLLISION:
            raise ValueError('Selected entry is not indexed as rCollision')
        output.mkdir(parents=True, exist_ok=True)
        source = extract(connection, meta, entry_id, output/'source.sbc')
    finally:
        connection.close()
    raw = (output/'source.sbc').read_bytes()
    core = parse_collision(raw)
    doc, binary = make_gltf(core)
    outputs = [save(output/'geometry.json', json_bytes(core)),
               save(output/'collision.obj', make_obj(core)),
               save(output/'scene.bin', binary), save(output/'scene.gltf', json_bytes(doc)),
               save(output/'unparsed-tail.bin', raw[core['unparsed_tail']['offset']:])]
    provenance = {'schema': 'mh4u-collision-export-v2', 'source': source, 'evidence': EVIDENCE,
                  'producer_sha256': sha(Path(__file__).read_bytes()), 'outputs': outputs,
                  'validation': core['validation'], 'limitations': core['limitations']}
    save(output/'provenance.json', json_bytes(provenance))
    return {'output': str(output.resolve()), 'vertices': len(core['vertices_xyzw']),
            'triangles': len(core['faces']), 'validation': core['validation'],
            'unparsed_tail': core['unparsed_tail']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    for name in ('inventory', 'export'):
        p = commands.add_parser(name)
        p.add_argument('--index', type=Path, required=True)
        p.add_argument('--output', type=Path, required=True)
        if name == 'export':
            p.add_argument('--entry', type=int, required=True)
    args = parser.parse_args()
    if args.command == 'inventory':
        result = inventory(args.index)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        save(args.output, json_bytes(result))
        print(json.dumps({'output': str(args.output.resolve()), 'entries': result['count']}))
    else:
        print(json.dumps(export(args.index, args.entry, args.output), indent=2))


if __name__ == '__main__':
    main()
