#!/usr/bin/env python3
"""Export observed static MOD230/MRL32/TEX resources to a neutral glTF preview.

Read-only source access through the verified resource index. Preserves all source
coordinates, primitive/material identities and unknown records. Does not infer
scene placement, shaders, alpha semantics, gameplay or physical units.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import struct
import zlib

import extract_model as resource
import index_resources as indexer

MAX_VERTICES = 1_000_000
MAX_PRIMITIVES = 4096
MAX_INDICES = 6_000_000
MAX_MATERIAL_COMMANDS = 65536
ALBEDO_SHADER_HASH = 0xcd06f
REFERENCE_COMMIT = 'd1fda41beeae129fbb26128157511afdee4002ea'


class StageError(ValueError):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


def require(condition, code, message):
    if not condition:
        raise StageError(code, message)


def bounds(points):
    return {name: [op(p[axis] for p in points) for axis in range(3)]
            for name, op in [('min', min), ('max', max)]}


def parse_stage(data):
    require(len(data) <= resource.MAX_MEMBER, 'resource_limit', 'MOD exceeds 64 MiB bound')
    h = resource.unpack('<4s4H13I', data, 0, 'MOD header')
    require(h[2] == 0, 'unsupported_skeleton', 'Static export requires zero joints')
    require(0 < h[3] <= MAX_PRIMITIVES and 0 < h[5] <= MAX_VERTICES,
            'resource_limit', 'Primitive/vertex count outside explicit probe bounds')
    # Repeated or overlapping spans must not expand a small file without bound.
    resource.region(data, h[15], h[3]*48, 'complete primitive table')
    primitive_records = [resource.unpack('<HHIHBB9I', data, h[15]+48*i, 'primitive preflight') for i in range(h[3])]
    require(sum(row[1] for row in primitive_records) == h[5],
            'vertex_count_mismatch', 'Primitive vertex counts disagree with header')
    require(sum(row[10]+1 for row in primitive_records) <= MAX_INDICES,
            'resource_limit', 'Cumulative strip-index count exceeds explicit probe bound')
    model = resource.parse_mod(data)
    for mesh in model['meshes']:
        require(mesh['triangles'], 'empty_primitive', 'Empty primitive cannot be exported')
        normalized = []
        for normal in mesh['normals']:
            length = math.sqrt(sum(x*x for x in normal))
            require(math.isfinite(length) and length > 1e-8, 'invalid_normal', 'Invalid source normal')
            normalized.append([x/length for x in normal])
        mesh['source_uv_bounds'] = {'min': [min(v[i] for v in mesh['uvs']) for i in range(2)],
                                    'max': [max(v[i] for v in mesh['uvs']) for i in range(2)]}
        mesh['normals'] = normalized
        mesh['uvs'] = [(u, 1-v) for u, v in mesh['uvs']]
        mesh['bounds'] = bounds(mesh['positions'])
    model['bounds'] = bounds([p for m in model['meshes'] for p in m['positions']])
    return model


def parse_materials(data, names):
    h = resource.unpack('<7I', data, 0, 'MRL header')
    require(data[:4] == b'MRL\0' and h[1] == 32, 'unsupported_mrl', 'Only observed MRL version32 is supported')
    require(h[2] == len(names) and len(names) <= 4096 and h[3] <= 4096,
            'material_count_mismatch', 'Bounded MOD/MRL material counts must agree')
    resource.region(data, h[5], h[3]*76, 'MRL resource table')
    resource.region(data, h[6], h[2]*60, 'MRL material table')
    require(sum(resource.unpack('<I', data, h[6]+60*i+24, 'command-count preflight')[0]&4095
                for i in range(h[2])) <= MAX_MATERIAL_COMMANDS,
            'resource_limit', 'Cumulative material-command count exceeds explicit probe bound')
    resources = []
    for i in range(h[3]):
        offset = h[5]+76*i
        row = resource.region(data, offset, 76, 'MRL resource')
        kind = resource.unpack('<I', row, 0, 'MRL resource kind')[0]
        name = None
        if kind == resource.TEXTURE:
            require(b'\0' in row[12:], 'invalid_resource_name', 'Texture resource lacks terminator')
            name = row[12:].split(b'\0', 1)[0].decode('ascii')
            require(bool(name), 'invalid_resource_name', 'Empty texture name')
        resources.append({'index': i, 'offset': offset, 'kind_raw': kind,
                          'name': name, 'raw_hex': row.hex()})
    records = []
    for i in range(h[2]):
        offset = h[6]+60*i
        words = resource.unpack('<15I', data, offset, 'MRL material')
        count, cmd_offset, size = words[6]&4095, words[13], words[2]
        resource.region(data, cmd_offset, size, 'MRL command buffer')
        require(count*12 <= size, 'invalid_command_count', 'Commands exceed command buffer')
        commands = []
        for j in range(count):
            command_offset = cmd_offset+12*j
            control, value, shader = resource.unpack('<3I', data, command_offset, 'MRL command')
            commands.append({'offset': command_offset, 'words': [control, value, shader],
                             'type': control&15, 'shader_hash': shader >> 12,
                             'shader_index_control': control >> 20, 'shader_index_id': shader&4095})
        records.append({'index': i, 'offset': offset, 'name_hash': words[1], 'words': list(words),
                        'raw_hex': resource.region(data, offset, 60, 'MRL material raw').hex(),
                        'command_buffer_raw_hex': resource.region(data, cmd_offset, size, 'commands raw').hex(),
                        'commands': commands, 'animation_size_raw': words[12],
                        'animation_pointer_raw': words[14]})
    bindings = []
    for i, name in enumerate(names):
        name_hash = (~zlib.crc32(name.encode('ascii')))&0xffffffff
        matches = [r for r in records if r['name_hash'] == name_hash]
        require(len(matches) == 1, 'ambiguous_material', 'Material name hash lacks a unique MRL record')
        record = matches[0]
        matches = [c for c in record['commands'] if c['type'] == 3 and c['shader_hash'] == ALBEDO_SHADER_HASH]
        require(len(matches) == 1, 'ambiguous_albedo', 'Material must have one observed tAlbedoMap command')
        command = matches[0]
        require(command['shader_index_control'] == command['shader_index_id'],
                'shader_index_mismatch', 'Shader command index disagrees with shader ID')
        texture_index = command['words'][1]-1
        require(0 <= texture_index < len(resources), 'invalid_texture_index', 'Albedo texture index outside resource table')
        texture = resources[texture_index]
        require(texture['kind_raw'] == resource.TEXTURE, 'unsupported_albedo_resource', 'Albedo does not refer to a TEX resource')
        bindings.append({'mod_material_index': i, 'name': name, 'jamcrc32': name_hash,
                         'mrl_material_index': record['index'], 'albedo_command': command,
                         'texture_resource_index': texture_index, 'texture_name': texture['name']})
    return {'header_words': list(h), 'resources': resources, 'records': records, 'bindings': bindings,
            'binding_basis': 'Unique MOD-name JAMCRC32 to MRL record; command type3, shader hash0xcd06f, one-based resource index',
            'unknowns': ['Non-albedo commands and constants', 'Sampler, blending, alpha, draw-pass and animation semantics']}


def make_gltf(model, materials, texture_uris):
    binary = bytearray()
    doc = {'asset': {'version': '2.0', 'generator': 'MH4U static stage probe'},
           'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0, 'name': 'source_coordinates'}],
           'meshes': [{'name': 'source_primitives', 'primitives': []}], 'bufferViews': [], 'accessors': [],
           'images': [], 'textures': [], 'materials': [], 'samplers': [{'wrapS': 10497, 'wrapT': 10497}],
           'extras': {'shader': 'Neutral opaque PBR preview; original shader and alpha semantics unknown',
                      'coordinates': 'Source XYZ preserved; no alignment or placement transform',
                      'uv': '(u,1-v); source pixels top-to-bottom', 'units': 'unknown source units'}}

    def accessor(rows, shape, component=5126, target=34962, extent=False):
        while len(binary)%4:
            binary.append(0)
        offset = len(binary)
        for row in rows:
            binary.extend(struct.pack('<'+('f' if component == 5126 else 'H')*len(row), *row))
        view = len(doc['bufferViews'])
        doc['bufferViews'].append({'buffer': 0, 'byteOffset': offset, 'byteLength': len(binary)-offset, 'target': target})
        item = {'bufferView': view, 'componentType': component, 'count': len(rows), 'type': shape}
        if extent:
            item.update(bounds(rows))
        doc['accessors'].append(item)
        return len(doc['accessors'])-1

    texture_indices = {}
    for name, uri in sorted(texture_uris.items()):
        texture_indices[name] = len(doc['textures'])
        doc['images'].append({'uri': uri})
        doc['textures'].append({'source': len(doc['images'])-1, 'sampler': 0})
    for binding in materials['bindings']:
        doc['materials'].append({'name': binding['name'], 'pbrMetallicRoughness': {
            'baseColorTexture': {'index': texture_indices[binding['texture_name']]},
            'metallicFactor': 0, 'roughnessFactor': 1}, 'alphaMode': 'OPAQUE',
            'extras': {'sourceMaterialIndex': binding['mod_material_index'],
                      'sourceMrlMaterialIndex': binding['mrl_material_index'],
                      'sourceTexture': binding['texture_name'], 'originalShaderReimplemented': False}})
    for mesh in model['meshes']:
        attrs = {key: accessor(mesh[field], shape, extent=key == 'POSITION')
                 for key, field, shape in [('POSITION', 'positions', 'VEC3'), ('NORMAL', 'normals', 'VEC3'),
                                           ('TEXCOORD_0', 'uvs', 'VEC2')]}
        indices = accessor([(i,) for t in mesh['triangles'] for i in t], 'SCALAR', 5123, 34963)
        doc['meshes'][0]['primitives'].append({'attributes': attrs, 'indices': indices, 'mode': 4,
            'material': mesh['material_index'], 'extras': {'sourcePrimitiveIndex': mesh['primitive_index']}})
    doc['buffers'] = [{'uri': 'scene.bin', 'byteLength': len(binary)}]
    return doc, bytes(binary)


def sub(a, b):
    return [x-y for x, y in zip(a, b)]


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def segment_distance_squared(p, a, b):
    ab = sub(b, a); length = dot(ab, ab)
    t = max(0, min(1, dot(sub(p, a), ab)/length)) if length else 0
    delta = sub(p, [x+t*y for x, y in zip(a, ab)])
    return dot(delta, delta)


def triangle_distance_squared(p, a, b, c):
    ab, ac, ap = sub(b, a), sub(c, a), sub(p, a)
    aa, bb, cc, d, e = dot(ab, ab), dot(ab, ac), dot(ac, ac), dot(ap, ab), dot(ap, ac)
    denominator = aa*cc-bb*bb
    if denominator > 1e-20:
        u, v = (cc*d-bb*e)/denominator, (aa*e-bb*d)/denominator
        if u >= 0 and v >= 0 and u+v <= 1:
            delta = sub(ap, [u*x+v*y for x, y in zip(ab, ac)])
            return dot(delta, delta)
    return min(segment_distance_squared(p, a, b), segment_distance_squared(p, b, c),
               segment_distance_squared(p, c, a))


def compare_collision(model, collision):
    """Measure surfaces in unchanged source coordinates; never fit transforms."""
    vertices = collision['vertices_xyzw']
    require(0 < len(vertices) <= 10000, 'collision_limit', 'Collision sample exceeds explicit comparison bound')
    triangles = [(m['primitive_index'], [m['positions'][i] for i in face])
                 for m in model['meshes'] for face in m['triangles']]
    require(len(vertices)*len(triangles) <= 5_000_000, 'comparison_limit', 'Surface comparison exceeds five million point-triangle pairs')
    points = []
    for i, row in enumerate(vertices):
        p = row[:3]
        require(len(p) == 3 and all(math.isfinite(x) for x in p), 'invalid_collision_vertex', 'Invalid collision point')
        closest = min((triangle_distance_squared(p, *t), primitive) for primitive, t in triangles)
        points.append({'collision_vertex': i, 'nearest_model_primitive': closest[1],
                       'surface_distance_source_units': math.sqrt(max(0, closest[0]))})
    distances = sorted(p['surface_distance_source_units'] for p in points)
    cb = bounds(vertices)
    overlap = [max(0, min(model['bounds']['max'][a], cb['max'][a])-max(model['bounds']['min'][a], cb['min'][a]))
               for a in range(3)]
    return {'source_coordinate_transform': 'identity; no fitting, translation, scaling or axis swap',
            'collision_sha256_from_geometry': collision['source_sha256'], 'model_bounds': model['bounds'],
            'collision_bounds': cb, 'bbox_intersection_extent': overlap,
            'point_count': len(points), 'model_triangle_count': len(triangles),
            'distance_min': distances[0], 'distance_median': (distances[(len(distances)-1)//2]+distances[len(distances)//2])/2,
            'distance_max': distances[-1],
            'within_source_units': {str(t): sum(d <= t for d in distances) for t in [0.001, 0.01, 0.1, 1, 10]},
            'samples': points, 'interpretation': 'Coordinate-region/surface comparison only; no claim of identical meshes, walkability, placement or runtime collision'}


def export(index, entry_id, output, collision_path=None):
    output.mkdir(parents=True, exist_ok=True)
    source_dir = output/'source'; source_dir.mkdir(exist_ok=True)
    connection, metadata = indexer.open_index(index)
    members = []
    try:
        def extract(entry):
            path = source_dir/f'entry-{entry}.bin'
            record = indexer.extract(connection, metadata, entry, path)
            members.append(record)
            return path.read_bytes(), record['entry']
        mod, record = extract(entry_id)
        require(record['type_id'] == resource.MODEL, 'wrong_resource_type', 'Selected entry is not rModel')
        model = parse_stage(mod)
        def find(name, kind):
            rows = connection.execute('SELECT id FROM entries WHERE archive_id=? AND name=? AND type_id=?',
                                      (record['archive_id'], name, kind)).fetchall()
            require(len(rows) == 1, 'ambiguous_resource', 'Missing or duplicate same-archive resource: '+name)
            return rows[0][0]
        mrl, _ = extract(find(record['name'], resource.MATERIAL))
        materials = parse_materials(mrl, model['material_names'])
        textures, files, uris = [], [], {}
        for name in sorted(set(b['texture_name'] for b in materials['bindings'])):
            texture_id = find(name, resource.TEXTURE)
            tex, _ = extract(texture_id)
            try:
                info, rgb, alpha = resource.parse_tex(tex)
            except ValueError as error:
                raise StageError('unsupported_texture', f'{name}: {error}') from error
            uri = f'texture-{texture_id}.png'; uris[name] = uri
            files.append((uri, resource.png_rgb(info['width'], info['height'], rgb)))
            files.append((f'texture-{texture_id}-alpha.pgm', f'P5\n{info["width"]} {info["height"]}\n255\n'.encode()+alpha))
            textures.append({'entry_id': texture_id, 'name': name, 'info': info,
                             'alpha_value_histogram': dict(sorted(Counter(alpha).items())),
                             'preview_alpha': 'opaque; original alpha retained separately with unknown semantics'})
        document, binary = make_gltf(model, materials, uris)
        files += [('scene.gltf', json.dumps(document, indent=2)+'\n'), ('scene.bin', binary)]
        comparison = None
        if collision_path:
            collision_raw = collision_path.read_bytes()
            comparison = compare_collision(model, json.loads(collision_raw))
            comparison['input_geometry_json_sha256'] = resource.sha(collision_raw)
            files.append(('collision-comparison.json', json.dumps(comparison, indent=2)+'\n'))
        outputs = [resource.save(output/name, data) for name, data in files]
        report = {'schema': 'mh4u-static-stage-v1', 'producer': 'stage_scene.py; local source analysis, not REA Evidence',
                  'producer_sha256': resource.sha(Path(__file__).read_bytes()), 'members': members,
                  'model': {k: v for k, v in model.items() if k != 'meshes'},
                  'primitives': [{k: v for k, v in m.items() if k not in ('positions', 'normals', 'uvs', 'triangles')} for m in model['meshes']],
                  'materials': materials, 'textures': textures, 'outputs': outputs,
                  'totals': {'primitives': len(model['meshes']), 'materials': len(model['material_names']),
                             'textures': len(textures), 'vertices': sum(m['vertex_count'] for m in model['meshes']),
                             'triangles': sum(m['triangle_count'] for m in model['meshes'])},
                  'collision_comparison': comparison, 'reference_commit': REFERENCE_COMMIT,
                  'reference': 'https://github.com/Brachi/albam/blob/'+REFERENCE_COMMIT+'/albam/engines/mtfw/structs/mrl.ksy',
                  'unknowns': ['Original shader, sampler and alpha rules', 'Primitive visibility/draw-pass/LOD policy',
                               'Stage placement, lighting, spawn and gameplay', 'Physical units'],
                  'conversion': document['extras']}
        resource.save(output/'provenance.json', json.dumps(report, indent=2)+'\n')
        return {'output': str(output.resolve()), **report['totals'], 'scene_sha256': resource.sha((output/'scene.gltf').read_bytes())}
    finally:
        connection.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--index', type=Path, required=True)
    parser.add_argument('--entry', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--collision', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(export(args.index, args.entry, args.output, args.collision), indent=2))
    except (ValueError, OSError, KeyError, struct.error) as error:
        print(json.dumps({'error': getattr(error, 'code', 'invalid_input'), 'message': str(error)}))
        raise SystemExit(2)


if __name__ == '__main__':
    main()
