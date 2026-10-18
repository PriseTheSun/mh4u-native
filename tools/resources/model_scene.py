#!/usr/bin/env python3
"""Bounded MH4U skeletal MOD probe and glTF 2.0 exporter; no game execution.

Supports observed MOD230/type4/stride36, one material and its first TEX resource.
Joint records, local matrices, source skin matrices, palettes and unknown bytes
remain in a separate provenance document. No animation or shader is invented.
"""
import argparse
import json
import math
import os
from pathlib import Path
import struct

import extract_model as resource

IDENTITY = [1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0]
REFERENCE = {
    'url': 'https://raw.githubusercontent.com/xZombieAlix/MT-Framework-Mobile---Blender-Model-Script/2bb9c35be1f0ccfb3899e8317da1b6e6cf011a8e/BlenderMTFrameworkMobile.py',
    'sha256': 'e60b0f91c327c9540aee795a8cce0c5e582a999a7d6ec9489de16e07f81ad61f',
    'license_from_header': 'GPL-3.0-or-later; Copyright 2015 Seth VanHeulen',
    'usage': 'Structure research only; not imported, copied into runtime, or executed',
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def multiply(a, b):
    """Column-major matrices acting on column vectors, matching glTF."""
    return [sum(a[k*4+r]*b[c*4+k] for k in range(4)) for c in range(4) for r in range(4)]


def transform(matrix, point):
    return [sum(matrix[k*4+r]*point[k] for k in range(3))+matrix[12+r] for r in range(3)]


def inverse(matrix):
    rows = [[matrix[c*4+r] for c in range(4)] + [float(r == c) for c in range(4)] for r in range(4)]
    for column in range(4):
        pivot = max(range(column, 4), key=lambda row: abs(rows[row][column]))
        require(abs(rows[pivot][column]) > 1e-12, 'Singular matrix')
        rows[column], rows[pivot] = rows[pivot], rows[column]
        divisor = rows[column][column]
        rows[column] = [x/divisor for x in rows[column]]
        for row in range(4):
            if row != column:
                factor = rows[row][column]
                rows[row] = [x-factor*y for x, y in zip(rows[row], rows[column])]
    return [rows[r][4+c] for c in range(4) for r in range(4)]


def difference(a, b):
    return max(abs(x-y) for x, y in zip(a, b))


def globals_from_locals(locals_, parents):
    require(len(locals_) == len(parents), 'Mismatched skeleton arrays')
    result, active = [None]*len(parents), set()

    def resolve(index):
        require(index not in active, 'Cyclic skeleton hierarchy')
        if result[index] is None:
            active.add(index)
            parent = parents[index]
            require(parent == 255 or 0 <= parent < len(parents), 'Parent index outside skeleton')
            result[index] = locals_[index] if parent == 255 else multiply(resolve(parent), locals_[index])
            active.remove(index)
        return result[index]

    for index in range(len(parents)):
        resolve(index)
    return result


def trs(matrix):
    require(all(math.isfinite(x) for x in matrix), 'Non-finite matrix')
    require(difference([matrix[i] for i in (3, 7, 11, 15)], [0, 0, 0, 1]) < 1e-6, 'Non-affine matrix')
    scale = [math.sqrt(sum(matrix[c*4+r]**2 for r in range(3))) for c in range(3)]
    require(min(scale) > 1e-9, 'Zero scale')
    rotation = [[matrix[c*4+r]/scale[c] for c in range(3)] for r in range(3)]
    require(max(abs(sum(rotation[r][a]*rotation[r][b] for r in range(3))-float(a == b))
                for a in range(3) for b in range(3)) < 1e-5, 'Sheared matrix unsupported')
    det = sum(rotation[0][c]*(rotation[1][(c+1)%3]*rotation[2][(c+2)%3]
                              - rotation[1][(c+2)%3]*rotation[2][(c+1)%3]) for c in range(3))
    require(det > 0.99999, 'Reflected matrix unsupported')
    r = rotation
    trace = sum(r[i][i] for i in range(3))
    if trace > 0:
        s = math.sqrt(trace+1.0)*2
        q = [(r[2][1]-r[1][2])/s, (r[0][2]-r[2][0])/s, (r[1][0]-r[0][1])/s, s/4]
    else:
        i = max(range(3), key=lambda n: r[n][n])
        j, k = (i+1)%3, (i+2)%3
        s = math.sqrt(1+r[i][i]-r[j][j]-r[k][k])*2
        q = [0.0]*4
        q[i], q[j], q[k], q[3] = s/4, (r[j][i]+r[i][j])/s, (r[k][i]+r[i][k])/s, (r[k][j]-r[j][k])/s
    length = math.sqrt(sum(x*x for x in q))
    q = [x/length for x in q]
    result = {'translation': list(matrix[12:15]), 'rotation': q, 'scale': scale}
    require(difference(compose(result), matrix) < 1e-5, 'TRS reconstruction error')
    return result


def compose(node):
    if 'matrix' in node:
        return node['matrix']
    x, y, z, w = node.get('rotation', [0, 0, 0, 1])
    sx, sy, sz = node.get('scale', [1, 1, 1])
    t = node.get('translation', [0, 0, 0])
    return [(1-2*y*y-2*z*z)*sx, (2*x*y+2*z*w)*sx, (2*x*z-2*y*w)*sx, 0,
            (2*x*y-2*z*w)*sy, (1-2*x*x-2*z*z)*sy, (2*y*z+2*x*w)*sy, 0,
            (2*x*z+2*y*w)*sz, (2*y*z-2*x*w)*sz, (1-2*x*x-2*y*y)*sz, 0,
            *t, 1]


def parse_skeleton(data, header, *, allow_single_joint=False):
    count, offset, palette_count = header[2], header[12], header[11]
    require((1 if allow_single_joint else 2) <= count <= 256,
            'Probe requires 1..256 joints' if allow_single_joint else 'Probe requires 2..256 joints')
    resource.region(data, offset, count*152+256+palette_count*28, 'complete joint section')
    joints, locals_, binds, parents = [], [], [], []
    for i in range(count):
        record = resource.unpack('<4B5f', data, offset+24*i, 'joint record')
        local = list(resource.unpack('<16f', data, offset+24*count+64*i, 'local matrix'))
        bind = list(resource.unpack('<16f', data, offset+88*count+64*i, 'source skin matrix'))
        require(all(math.isfinite(x) for x in record[4:]+tuple(bind)), 'Non-finite joint data')
        node_trs = trs(local)
        joints.append({'index': i, 'source_function_id_candidate': record[0], 'parent': record[1],
                       'record': list(record), 'local_matrix': local, 'source_skin_matrix': bind,
                       'record_raw_hex': resource.region(data, offset+24*i, 24, 'joint bytes').hex(),
                       'gltf_trs': node_trs})
        parents.append(record[1]); locals_.append(local); binds.append(bind)
    world = globals_from_locals(locals_, parents)
    root = next((i for i, parent in enumerate(parents) if parent == 255), None)
    require(root is not None, 'No root joint')
    quantization = multiply(world[root], binds[root])
    uniform = IDENTITY.copy()
    uniform[0] = uniform[5] = uniform[10] = quantization[0]
    uniform[12:15] = quantization[12:15]
    require(quantization[0] > 0 and difference(quantization, uniform) < 1e-4,
            'Unsupported source coordinate transform: expected positive uniform scale and translation')
    residual = max(difference(multiply(w, b), quantization) for w, b in zip(world, binds))
    require(residual < 1e-3, 'Joint local/source skin matrix relationship is inconsistent')
    q_inverse = inverse(quantization)
    inverse_binds = [multiply(bind, q_inverse) for bind in binds]
    palettes = []
    palette_offset = offset+count*152+256
    for i in range(palette_count):
        p = palette_offset+28*i
        n = resource.unpack('<I', data, p, 'palette count')[0]
        require(0 < n <= 24, 'Unsupported palette joint count')
        palette = list(resource.region(data, p+4, n, 'palette joints'))
        require(all(j < count for j in palette), 'Palette joint outside skeleton')
        palettes.append(palette)
    require(palettes, 'No joint palettes')
    return {'joints': joints, 'parents': parents, 'local_matrices': locals_, 'global_matrices': world,
            'inverse_bind_matrices': inverse_binds, 'source_to_model': quantization,
            'joint_matrix_relation_max_error': residual, 'palettes': palettes,
            'palette_offset': palette_offset,
            'joint_remap_raw_hex': resource.region(data, offset+count*152, 256, 'joint remap').hex()}


def parse_actor(data, selected, *, allow_multiple_materials=False,
                allow_two_influence_vertices=False, allow_single_joint=False):
    h = resource.unpack('<4s4H13I', data, 0, 'MOD header')
    require(h[0] == b'MOD\0' and h[1] == 230, 'Only MOD version230 is supported')
    require(0 < h[4] <= 4096 and (allow_multiple_materials or h[4] == 1),
            'Default material binding requires exactly one MOD material; explicit multi-material opt-in is required')
    require(selected and len(set(selected)) == len(selected) and all(0 <= i < h[3] for i in selected),
            'Invalid or duplicate primitive selection')
    skeleton = parse_skeleton(data, h, allow_single_joint=allow_single_joint)
    resource.region(data, h[16], h[8], 'declared vertex buffer')
    materials = [resource.region(data, h[14]+128*i, 128, 'material name').split(b'\0')[0].decode('ascii')
                 for i in range(h[4])]
    meshes, catalog = [], []
    for i in range(h[3]):
        info = resource.unpack('<HHIHBB9I', data, h[15]+48*i, 'primitive record')
        record = resource.region(data, h[15]+48*i, 48, 'primitive bytes')
        metadata = {'primitive_index': i, 'vertex_count': info[1], 'stride': info[4], 'type': info[5]&63,
                    'raw_record_hex': record.hex(), 'words': list(info), 'selected': i in selected}
        catalog.append(metadata)
        if i not in selected:
            continue
        stride = info[4]
        require(stride in ((28, 36) if allow_two_influence_vertices else (36,)) and info[5]&63 == 4,
                'Only type4/stride28 or36 skin vertices are supported' if allow_two_influence_vertices
                else 'Only type4/stride36 skin vertices are supported')
        material_index = (info[2] >> 12)&0xfff
        require(material_index < len(materials), 'Material index outside supported table')
        palette_id = record[0x25]
        require(palette_id < len(skeleton['palettes']), 'Primitive palette outside table')
        palette = skeleton['palettes'][palette_id]
        start = h[16]+info[6]*stride+info[7]
        raw = resource.region(data, start, info[1]*stride, 'vertex span')
        require(start >= h[16] and start+len(raw) <= h[16]+h[8], 'Vertices escape declared buffer')
        positions, normals, uvs, indices, weights = [], [], [], [], []
        weight_sums, merged = {}, 0
        for v in range(info[1]):
            row = raw[v*stride:(v+1)*stride]
            position = resource.unpack('<3f', row, 0, 'position')
            uv = resource.unpack('<2f', row, 16, 'UV')
            require(all(math.isfinite(x) for x in position+uv), 'Non-finite vertex data')
            normal = [max(-1.0, x/127) for x in resource.unpack('<3b', row, 12, 'normal')]
            length = math.sqrt(sum(x*x for x in normal))
            require(length > 1e-8, 'Zero-length normal')
            bone_bytes, weight_bytes = row[24:26]+row[32:34], row[26:28]+row[34:36]
            total = sum(weight_bytes)
            require(total == 255, 'Observed skin layout requires byte weights summing to255')
            weight_sums[str(total)] = weight_sums.get(str(total), 0)+1
            combined = {}
            for bone, weight in zip(bone_bytes, weight_bytes):
                if weight:
                    require(bone < len(palette), 'Active vertex joint outside palette')
                    joint = palette[bone]
                    merged += int(joint in combined)
                    combined[joint] = combined.get(joint, 0)+weight
            influences = list(combined.items())
            indices.append([j for j, _ in influences]+[0]*(4-len(influences)))
            weights.append([w/255 for _, w in influences]+[0.0]*(4-len(influences)))
            positions.append(transform(skeleton['source_to_model'], position))
            normals.append([x/length for x in normal])
            uvs.append([uv[0], 1-uv[1]])
        raw_indices = resource.region(data, h[17]+2*info[9], 2*(info[10]+1), 'strip indices')
        triangles, strip_stats = resource.triangles([i[0] for i in struct.iter_unpack('<H', raw_indices)], info[6], info[1])
        require(triangles, 'Empty selected primitive')
        meshes.append({'primitive_index': i, 'positions': positions, 'normals': normals, 'uvs': uvs,
                       'joints': indices, 'weights': weights, 'triangles': triangles})
        if allow_multiple_materials:
            meshes[-1]['material_index'] = material_index
            metadata['material_index'] = material_index
        metadata.update({'triangle_count': len(triangles), 'palette_index': palette_id,
                         'weight_byte_sum_histogram': weight_sums, 'duplicate_joint_influences_merged': merged,
                         'strip_statistics': strip_stats})
    material_field = {'material_names': materials} if allow_multiple_materials else {'material_name': materials[0]}
    return {'header_words': list(h[1:]), 'header_raw_hex': resource.region(data, 0, 128, 'header').hex(),
            **material_field, 'skeleton': skeleton, 'primitives': catalog, 'meshes': meshes}


def accessor_values(document, binary, index):
    accessor = document['accessors'][index]
    view = document['bufferViews'][accessor['bufferView']]
    fmt = {5123: 'H', 5125: 'I', 5126: 'f'}[accessor['componentType']]
    size = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}[accessor['type']]
    stride = struct.calcsize('<'+fmt*size)
    start = view.get('byteOffset', 0)+accessor.get('byteOffset', 0)
    raw = resource.region(binary, start, stride*accessor['count'], 'glTF accessor')
    return list(struct.iter_unpack('<'+fmt*size, raw))


def validate_scene(document, binary):
    nodes, skin = document['nodes'], document['skins'][0]
    joint_nodes = skin['joints']
    parents = [255]*len(joint_nodes)
    lookup = {node: i for i, node in enumerate(joint_nodes)}
    for node_id in joint_nodes:
        for child in nodes[node_id].get('children', []):
            if child in lookup:
                require(parents[lookup[child]] == 255, 'glTF joint has multiple parents')
                parents[lookup[child]] = lookup[node_id]
    world = globals_from_locals([compose(nodes[i]) for i in joint_nodes], parents)
    inverse_binds = accessor_values(document, binary, skin['inverseBindMatrices'])
    require(len(inverse_binds) == len(world), 'Inverse-bind count mismatch')
    matrices = [multiply(w, b) for w, b in zip(world, inverse_binds)]
    matrix_error = max(difference(m, IDENTITY) for m in matrices)
    max_error = sum_square = weight_error = 0.0
    vertex_count = triangle_count = 0
    for mesh in document['meshes']:
        for primitive in mesh['primitives']:
            attrs = primitive['attributes']
            positions, normals, joints, weights = [accessor_values(document, binary, attrs[x])
                                                  for x in ('POSITION', 'NORMAL', 'JOINTS_0', 'WEIGHTS_0')]
            require(len(positions) == len(joints) == len(weights) == len(normals), 'Attribute counts differ')
            for point, normal, js, ws in zip(positions, normals, joints, weights):
                require(all(0 <= j < len(world) for j in js), 'glTF joint index outside skin')
                require(all(0 <= w <= 1 for w in ws), 'Invalid glTF weight')
                weight_error = max(weight_error, abs(sum(ws)-1))
                require(abs(sum(n*n for n in normal)-1) < 1e-5, 'glTF normal is not unit length')
                deformed = [sum(w*transform(matrices[j], point)[axis] for j, w in zip(js, ws)) for axis in range(3)]
                error = math.sqrt(sum((a-b)**2 for a, b in zip(point, deformed)))
                max_error = max(max_error, error); sum_square += error*error; vertex_count += 1
            faces = accessor_values(document, binary, primitive['indices'])
            require(len(faces)%3 == 0 and all(0 <= face[0] < len(positions) for face in faces), 'Invalid glTF indices')
            triangle_count += len(faces)//3
    require(weight_error < 1e-6, 'glTF weights do not sum to1')
    require(matrix_error < 1e-3 and max_error < 1e-3, 'Bind pose deformation exceeds tolerance')
    return {'vertices_checked': vertex_count, 'triangles_checked': triangle_count,
            'max_weight_sum_error': weight_error, 'max_joint_bind_identity_error': matrix_error,
            'max_bind_position_error_source_units': max_error,
            'rms_bind_position_error_source_units': math.sqrt(sum_square/vertex_count),
            'accepted_position_tolerance_source_units': 1e-3,
            'basis': 'Serialized float32 glTF accessors, node TRS, hierarchy and skin matrices'}


def make_gltf(actor, texture_name=None, *, material_bundle=None):
    skeleton = actor['skeleton']
    binary = bytearray()
    doc = {'asset': {'version': '2.0', 'generator': 'mh4u-native model_scene.py'},
           'scene': 0, 'scenes': [], 'nodes': [], 'skins': [], 'meshes': [],
           'buffers': [], 'bufferViews': [], 'accessors': [],
           'images': [{'uri': texture_name}], 'textures': [{'source': 0, 'sampler': 0}],
           'samplers': [{'magFilter': 9729, 'minFilter': 9729, 'wrapS': 10497, 'wrapT': 10497}],
           'materials': [{'name': actor.get('material_name'), 'alphaMode': 'OPAQUE',
                          'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}, 'metallicFactor': 0, 'roughnessFactor': 1}}]}
    if material_bundle is not None:
        require('material_names' in actor and len(material_bundle['materials']) == len(actor['material_names']),
                'Material bundle must match explicit multi-material actor')
        for key in ('materials', 'images', 'textures'):
            doc[key] = material_bundle[key]
    else:
        require('material_name' in actor and isinstance(texture_name, str),
                'Multi-material actor needs explicit complete material bundle')

    def accessor(rows, shape, component=5126, target=None, bounds=False):
        while len(binary)%4:
            binary.append(0)
        offset = len(binary)
        fmt = {5123: 'H', 5125: 'I', 5126: 'f'}[component]
        for row in rows:
            binary.extend(struct.pack('<'+fmt*len(row), *row))
        view = {'buffer': 0, 'byteOffset': offset, 'byteLength': len(binary)-offset}
        if target:
            view['target'] = target
        item = {'bufferView': len(doc['bufferViews']), 'componentType': component, 'count': len(rows), 'type': shape}
        if bounds:
            item['min'] = [min(r[i] for r in rows) for i in range(len(rows[0]))]
            item['max'] = [max(r[i] for r in rows) for i in range(len(rows[0]))]
        doc['bufferViews'].append(view); doc['accessors'].append(item)
        return len(doc['accessors'])-1

    for joint in skeleton['joints']:
        doc['nodes'].append({'name': f'joint_{joint["index"]:03}', **joint['gltf_trs'],
                             'extras': {'sourceJointIndex': joint['index'],
                                        'sourceFunctionIdCandidate': joint['source_function_id_candidate']}})
    for i, parent in enumerate(skeleton['parents']):
        if parent != 255:
            doc['nodes'][parent].setdefault('children', []).append(i)
    roots = [i for i, parent in enumerate(skeleton['parents']) if parent == 255]
    require(len(roots) == 1, 'Current scene export requires one skeleton root')
    doc['skins'].append({'name': 'source_skeleton', 'joints': list(range(len(skeleton['joints']))),
                         'skeleton': roots[0], 'inverseBindMatrices': accessor(skeleton['inverse_bind_matrices'], 'MAT4')})
    scene_nodes = roots.copy()
    for mesh in actor['meshes']:
        attrs = {key: accessor(mesh[value], shape, kind, 34962, key == 'POSITION')
                 for key, value, shape, kind in [('POSITION', 'positions', 'VEC3', 5126),
                                                ('NORMAL', 'normals', 'VEC3', 5126),
                                                ('TEXCOORD_0', 'uvs', 'VEC2', 5126),
                                                ('JOINTS_0', 'joints', 'VEC4', 5123),
                                                ('WEIGHTS_0', 'weights', 'VEC4', 5126)]}
        index = accessor([(v,) for face in mesh['triangles'] for v in face], 'SCALAR', 5123, 34963)
        mesh_index = len(doc['meshes'])
        doc['meshes'].append({'name': f'primitive_{mesh["primitive_index"]}',
                              'primitives': [{'mode': 4, 'attributes': attrs, 'indices': index,
                                              'material': mesh.get('material_index', 0)}]})
        scene_nodes.append(len(doc['nodes']))
        doc['nodes'].append({'name': f'actor_primitive_{mesh["primitive_index"]}', 'mesh': mesh_index, 'skin': 0})
    doc['scenes'].append({'name': 'source_bind_pose', 'nodes': scene_nodes})
    doc['buffers'].append({'uri': 'scene.bin', 'byteLength': len(binary)})
    return doc, bytes(binary)


def read_inspection(path):
    manifest_bytes = (path/'manifest.json').read_bytes()
    inventory_bytes = (path/'romfs-inventory.json').read_bytes()
    manifest, inventory = json.loads(manifest_bytes), json.loads(inventory_bytes)
    return manifest, inventory, {'manifest_sha256': resource.sha(manifest_bytes),
                                 'inventory_sha256': resource.sha(inventory_bytes)}


def read_archive(manifest, entry):
    path = Path(manifest['source']['path'])
    with path.open('rb') as handle:
        before = os.fstat(handle.fileno())
        require(before.st_size == manifest['source']['size'], 'ROM size changed')
        require(0 < entry['size'] <= resource.MAX_ARC and 0 <= entry['source_offset'] <= before.st_size-entry['size'],
                'Archive outside ROM or memory budget')
        handle.seek(entry['source_offset']); data = handle.read(entry['size'])
        after = os.fstat(handle.fileno())
    require(len(data) == entry['size'] and (before.st_size, before.st_mtime_ns, before.st_ino) ==
            (after.st_size, after.st_mtime_ns, after.st_ino), 'ROM changed during selected read')
    return data


def export_scene(inspection, archive_path, model_name, selected, output):
    manifest, inventory, inspection_hashes = read_inspection(inspection)
    matches = [e for e in inventory if e['path'] == archive_path]
    require(len(matches) == 1, 'Missing or ambiguous ARC path')
    entry = matches[0]; arc = read_archive(manifest, entry)
    directory = resource.arc_members(arc)

    def member(name, kind):
        matches = [r for r in directory['entries'] if r['name'] == name and int(r['kind'], 16) == kind]
        require(len(matches) == 1, 'Missing or ambiguous resource: '+name)
        data = resource.inflate_member(arc, matches[0])
        return data, {**matches[0], 'sha256': resource.sha(data)}

    mod, mod_record = member(model_name, resource.MODEL)
    mrl, mrl_record = member(model_name, resource.MATERIAL)
    actor = parse_actor(mod, selected)
    mh = resource.unpack('<7I', mrl, 0, 'MRL header')
    require(mrl[:4] == b'MRL\0' and mh[1] == 32, 'Unsupported MRL')
    record = resource.region(mrl, mh[5], 76, 'first material resource')
    require(resource.unpack('<I', record, 0, 'resource kind')[0] == resource.TEXTURE, 'First MRL resource is not TEX')
    texture_name = record[12:76].split(b'\0')[0].decode('ascii')
    tex, tex_record = member(texture_name, resource.TEXTURE)
    texture_info, rgb, alpha = resource.parse_tex(tex)
    basename = model_name.split('\\')[-1]
    tex_basename = texture_name.split('\\')[-1]
    require(all(x and x not in ('.', '..') and not any(c in x for c in '/\\:') for x in (basename, tex_basename)),
            'Unsafe resource basename')
    doc, binary = make_gltf(actor, tex_basename+'.png')
    validation = validate_scene(doc, binary)
    output.mkdir(parents=True, exist_ok=True)
    products = []
    for name, data in [(basename+'.mod', mod), (basename+'.mrl', mrl), (tex_basename+'.tex', tex),
                       (tex_basename+'.png', resource.png_rgb(texture_info['width'], texture_info['height'], rgb)),
                       (tex_basename+'_alpha.pgm', f'P5\n{texture_info["width"]} {texture_info["height"]}\n255\n'.encode()+alpha),
                       ('scene.gltf', json.dumps(doc, indent=2)+'\n'), ('scene.bin', binary)]:
        products.append(resource.save(output/name, data))
    report = {'schema': 'mh4u-skeletal-probe-v1', 'source': manifest['source'], 'full_rom_hash_recomputed': False,
              'inspection': inspection_hashes, 'archive': {**entry, 'sha256': resource.sha(arc)},
              'members': [mod_record, mrl_record, tex_record], 'references': [REFERENCE],
              'selected_primitives': selected, 'model': {k: v for k, v in actor.items() if k != 'meshes'},
              'material': {'texture_resource': texture_name, 'texture_info': texture_info,
                           'binding_basis': 'Single MOD material and first MRL resource names the selected TEX',
                           'shader': 'glTF neutral PBR preview; original shader state unknown',
                           'alpha': 'Decoded to PGM only; semantic purpose unknown; preview opaque'},
              'coordinate_conversion': {'positions': 'source_to_model * source_position',
                                        'inverse_binds': 'source_skin_matrix * inverse(source_to_model)',
                                        'joint_locals': 'Source local matrices preserved via verified TRS decomposition',
                                        'normals': 'Signed normalized i8, renormalized to unit length',
                                        'uv': '(u,1-v) for top-left glTF image origin',
                                        'physical_units': 'Unknown; source model units retained'},
              'validation': validation, 'outputs': products,
              'unknowns': ['Other primitive purpose/LOD', 'Original material expressions and alpha semantics',
                           'Joint record fields beyond parent and candidate function ID', 'Animation and runtime pose fidelity']}
    resource.save(output/'provenance.json', json.dumps(report, indent=2)+'\n')
    resource.save(output/'validation.json', json.dumps(validation, indent=2)+'\n')
    return validation


def inventory_candidates(inspection, limit, output):
    require(1 <= limit <= 16, 'Bounded inventory limit must be1..16')
    manifest, inventory, hashes = read_inspection(inspection)
    candidates = sorted([e for e in inventory if e['path'].startswith('/data/em') and e['path'].endswith('.arc')],
                        key=lambda e: (e['size'], e['path']))[:limit]
    rows = []
    for entry in candidates:
        arc = read_archive(manifest, entry)
        models = []
        for record in resource.arc_members(arc)['entries']:
            if int(record['kind'], 16) != resource.MODEL:
                continue
            mod = resource.inflate_member(arc, record)
            h = resource.unpack('<4s4H13I', mod, 0, 'model header')
            layouts = sorted(set((resource.region(mod, h[15]+48*i, 48, 'primitive')[10],
                                  resource.region(mod, h[15]+48*i, 48, 'primitive')[11]&63) for i in range(h[3])))
            models.append({'name': record['name'], 'sha256': resource.sha(mod), 'version': h[1],
                           'joints': h[2], 'primitives': h[3], 'materials': h[4], 'stride_type_pairs': layouts})
        rows.append({**entry, 'sha256': resource.sha(arc), 'models': models})
    result = {'selection': 'Up to16 smallest /data/em*.arc entries; no claim of complete game coverage',
              'source': manifest['source'], 'inspection': hashes, 'full_rom_hash_recomputed': False, 'archives': rows}
    output.parent.mkdir(parents=True, exist_ok=True)
    resource.save(output, json.dumps(result, indent=2)+'\n')
    return {'archives_examined': len(rows), 'output': str(output)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    scan = commands.add_parser('inventory')
    scan.add_argument('--inspection', type=Path, required=True)
    scan.add_argument('--limit', type=int, default=12)
    scan.add_argument('--output', type=Path, required=True)
    export = commands.add_parser('export')
    export.add_argument('--inspection', type=Path, required=True)
    export.add_argument('--archive', required=True)
    export.add_argument('--model', required=True)
    export.add_argument('--primitive', type=int, action='append', required=True)
    export.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.command == 'inventory':
        result = inventory_candidates(args.inspection, args.limit, args.output)
    else:
        result = export_scene(args.inspection, args.archive, args.model, args.primitive, args.output)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
