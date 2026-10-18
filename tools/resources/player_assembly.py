#!/usr/bin/env python3
"""Compose explicitly selected, bind-compatible player pieces without equipment/visibility guesses."""
import argparse
import copy
import json
import math
from pathlib import Path

import extract_model as resource
import index_resources as indexer
import model_scene as model
import player_probe as player

MAX_PARTS = 16
MAX_PART_VERTICES = 25000
MAX_TOTAL_VERTICES = 100000
BIND_TOLERANCE = 1e-4


class AssemblyError(ValueError):
    def __init__(self, code, message, evidence=None):
        super().__init__(message)
        self.code, self.evidence = code, evidence


def require(condition, code, message, evidence=None):
    if not condition:
        raise AssemblyError(code, message, evidence)


def joint_map(base, piece):
    """Map unique source function IDs only when global bind and internal parent IDs agree."""
    def identities(skeleton):
        ids = [j['source_function_id_candidate'] for j in skeleton['joints']]
        require(len(set(ids)) == len(ids), 'duplicate_joint_id', 'Source joint IDs are not unique', ids)
        return ids
    target_ids, source_ids = identities(base), identities(piece)
    target = {identifier: index for index, identifier in enumerate(target_ids)}
    missing = sorted(set(source_ids)-set(target))
    require(not missing, 'missing_joint_id', 'Piece has IDs absent from the selected base skeleton', missing)
    mapping, rows = [], []
    for i, identifier in enumerate(source_ids):
        j = target[identifier];parent = piece['parents'][i];target_parent = base['parents'][j]
        parent_id = source_ids[parent] if parent != 255 else None
        target_parent_id = target_ids[target_parent] if target_parent != 255 else None
        matrix_error = model.difference(piece['global_matrices'][i], base['global_matrices'][j])
        inverse_error = model.difference(piece['inverse_bind_matrices'][i], base['inverse_bind_matrices'][j])
        row = {'source_joint_index': i, 'source_function_id': identifier, 'target_joint_index': j,
               'source_parent_id': parent_id, 'target_parent_id': target_parent_id,
               'global_bind_max_component_error': matrix_error, 'inverse_bind_max_component_error': inverse_error,
               'source_root_maps_to_base_internal_joint': parent == 255 and target_parent != 255}
        rows.append(row)
        require(parent == 255 or parent_id == target_parent_id, 'parent_id_mismatch',
                'Internal parent relation differs from the selected base', rows)
        require(matrix_error <= BIND_TOLERANCE and inverse_error <= BIND_TOLERANCE,
                'bind_mismatch', 'Matching IDs have incompatible bind matrices; no fitting or retargeting is performed', rows)
        mapping.append(j)
    return mapping, {'basis': 'Unique byte IDs, global bind matrices, inverse binds and internal parent relations',
                     'tolerance_per_matrix_component': BIND_TOLERANCE, 'joints': rows,
                     'max_global_bind_error': max(r['global_bind_max_component_error'] for r in rows),
                     'max_inverse_bind_error': max(r['inverse_bind_max_component_error'] for r in rows),
                     'coordinate_transform': 'identity in decoded source model coordinates; no fit, scale or axis swap',
                     'scope': 'Bind-compatible candidate; does not establish runtime equipment selection or visibility'}


def remap_piece(actor, mapping, target_skeleton, entry_id, material_base, mesh_base):
    """Preserve source geometry; change only joint/material indices for the common tables."""
    meshes = []
    before_error = after_error = change = 0.0
    source_skin = actor['skeleton']
    old_matrices = [model.multiply(w, b) for w, b in zip(source_skin['global_matrices'], source_skin['inverse_bind_matrices'])]
    new_matrices = [model.multiply(w, b) for w, b in zip(target_skeleton['global_matrices'], target_skeleton['inverse_bind_matrices'])]
    for mesh in actor['meshes']:
        result = copy.deepcopy(mesh)
        result['source_entry_id'], result['source_primitive_index'] = entry_id, mesh['primitive_index']
        result['primitive_index'] = mesh_base+len(meshes)
        result['material_index'] += material_base
        # Zero-weight lanes carry no source influence; keep canonical glTF padding
        # at joint zero instead of remapping its placeholder to a real joint.
        result['joints'] = [[mapping[index] if weight else 0 for index, weight in zip(row, weights)]
                            for row, weights in zip(mesh['joints'], mesh['weights'])]
        for position, old, new, weights in zip(mesh['positions'], mesh['joints'], result['joints'], mesh['weights']):
            old_point = [sum(weight*model.transform(old_matrices[joint], position)[axis] for joint, weight in zip(old, weights)) for axis in range(3)]
            new_point = [sum(weight*model.transform(new_matrices[joint], position)[axis] for joint, weight in zip(new, weights)) for axis in range(3)]
            before_error = max(before_error, math.dist(position, old_point))
            after_error = max(after_error, math.dist(position, new_point))
            change = max(change, math.dist(old_point, new_point))
        meshes.append(result)
    require(change < 1e-3 and after_error < 1e-3, 'remap_deformation', 'Remapping changes the reference geometry beyond the explicit tolerance')
    return meshes, {'max_original_bind_position_error': before_error, 'max_shared_bind_position_error': after_error,
                    'max_reference_deformation_change': change, 'coordinates_and_normals_copied_unchanged': True,
                    'zero_weight_joint_lanes': 'Canonical zero padding; no active influence removed or changed'}


def merge_bundle(destination, source):
    """One owner for glTF table remapping; deduplicate identical image URIs from explicit entries."""
    mapping = {}
    for index, texture in enumerate(source['textures']):
        image = source['images'][texture['source']]
        matches = [i for i, existing in enumerate(destination['images']) if existing == image]
        if matches:
            image_index = matches[0]
        else:
            image_index = len(destination['images']);destination['images'].append(copy.deepcopy(image))
        candidate = {**texture, 'source': image_index}
        if candidate not in destination['textures']:
            destination['textures'].append(candidate)
        mapping[index] = destination['textures'].index(candidate)
    offset = len(destination['materials'])
    for material in source['materials']:
        material = copy.deepcopy(material)
        reference = material['pbrMetallicRoughness']['baseColorTexture']
        reference['index'] = mapping[reference['index']]
        destination['materials'].append(material)
    return offset


def inspect_geometry(connection, metadata, identifier, source):
    source.mkdir(parents=True, exist_ok=True)
    path = source/f'entry-{identifier}.bin'
    extraction = indexer.extract(connection, metadata, identifier, path)
    require(extraction['entry']['type_id'] == resource.MODEL, 'wrong_resource', 'Select an rModel entry')
    raw = path.read_bytes();header = resource.unpack('<4s4H13I', raw, 0, 'MOD header')
    require(0 < header[3] <= 4096 and 0 < header[5] <= MAX_PART_VERTICES, 'resource_limit', 'Piece exceeds assembly vertex budget')
    records = [resource.unpack('<HHIHBB9I', raw, header[15]+48*i, 'primitive preflight') for i in range(header[3])]
    require(sum(row[1] for row in records) == header[5] and sum(row[10]+1 for row in records) <= 6000000,
            'resource_limit', 'Cumulative source mesh extent inconsistent or excessive')
    actor = model.parse_actor(raw, list(range(header[3])), allow_multiple_materials=True,
                              allow_two_influence_vertices=True, allow_single_joint=True)
    return actor, extraction


def byte_evidence(actor):
    # header_words omits only the magic; source header indices are therefore one higher.
    joint_offset = actor['header_words'][11]
    count = len(actor['skeleton']['joints'])
    return {'joint_table_offset': joint_offset, 'joint_record_size': 24,
            'local_matrix_table_offset': joint_offset+24*count,
            'source_skin_matrix_table_offset': joint_offset+88*count,
            'identity_byte_offset_within_joint': 0, 'parent_index_byte_offset_within_joint': 1,
            'skin_layouts': sorted(set((p['type'], p['stride']) for p in actor['primitives'])),
            'skin_vertex_offsets': {'position_float32x3': 0, 'normal_snorm8x3': 12, 'uv_float32x2': 16,
                                    'joint_indices_0_1_u8': 24, 'weights_0_1_unorm8': 26,
                                    'stride36_additional_joint_indices': 32, 'stride36_additional_weights': 34},
            'source_joint_records': [j['record_raw_hex'] for j in actor['skeleton']['joints']],
            'source_primitive_records': [p['raw_record_hex'] for p in actor['primitives']]}


def export(index, base_entry, piece_entries, inspect_entries, shared_archive, output):
    identifiers = [base_entry, *piece_entries, *inspect_entries]
    require(len(identifiers) <= MAX_PARTS and len(set(identifiers)) == len(identifiers),
            'selection_limit', 'Select at most16 distinct explicit entries')
    output.mkdir(parents=True, exist_ok=True)
    connection, metadata = indexer.open_index(index)
    accepted, rejected, inspections, members, files = [], [], [], [], {}
    bundle = {'images': [], 'textures': [], 'materials': []}
    combined, total_vertices = None, 0
    try:
        for identifier in [base_entry, *piece_entries]:
            try:
                piece = player.load_piece(connection, metadata, identifier, shared_archive, output/'source'/str(identifier),
                                          allow_two_influence_vertices=True, allow_single_joint=True,
                                          max_vertices=MAX_PART_VERTICES)
                actor = piece['actor'];members.extend(piece['members'])
                if combined is None:
                    combined = {'skeleton': copy.deepcopy(actor['skeleton']), 'meshes': [], 'material_names': []}
                mapping, association = joint_map(combined['skeleton'], actor['skeleton'])
                vertices = sum(len(m['positions']) for m in actor['meshes'])
                require(total_vertices+vertices <= MAX_TOTAL_VERTICES, 'resource_limit', 'Assembly exceeds100000 vertices')
                # Validate all geometry before committing any shared tables.
                mapped, remap = remap_piece(actor, mapping, combined['skeleton'], identifier,
                                           len(bundle['materials']), len(combined['meshes']))
                for name, data in piece['files']:
                    require(name not in files or files[name] == data, 'resource_identity_collision', 'Conflicting bytes for one texture filename')
                merge_bundle(bundle, piece['material_bundle'])
                files.update(piece['files']);combined['meshes'].extend(mapped)
                combined['material_names'].extend(actor['material_names']);total_vertices += vertices
                accepted.append({'entry_id': identifier, 'source_name': piece['entry']['name'],
                                 'source_sha256': piece['members'][0]['sha256'],
                                 'association': association, 'remap_validation': remap, 'byte_evidence': byte_evidence(actor),
                                 'model': {k: v for k, v in actor.items() if k != 'meshes'},
                                 'materials': piece['materials'], 'textures': piece['textures'],
                                 'visibility': 'All primitives retained; no original runtime visibility rule inferred'})
            except (ValueError, OSError) as error:
                if identifier == base_entry:
                    raise
                rejected.append({'entry_id': identifier, 'code': getattr(error, 'code', 'unsupported_piece'),
                                 'message': str(error), 'evidence': getattr(error, 'evidence', None)})
        for identifier in inspect_entries:
            try:
                actor, extraction = inspect_geometry(connection, metadata, identifier, output/'source'/str(identifier))
                members.append(extraction)
                mapping, association = joint_map(combined['skeleton'], actor['skeleton'])
                inspections.append({'entry_id': identifier, 'extraction': extraction, 'byte_evidence': byte_evidence(actor),
                                    'model': {k: v for k, v in actor.items() if k != 'meshes'}, 'association': association,
                                    'exported': False, 'reason': 'Inspection-only selection; primitive/appearance visibility selection unresolved'})
            except (ValueError, OSError) as error:
                inspections.append({'entry_id': identifier, 'exported': False, 'code': getattr(error, 'code', 'unsupported_piece'),
                                    'reason': str(error), 'evidence': getattr(error, 'evidence', None)})
        document, binary = model.make_gltf(combined, material_bundle=bundle)
        document['asset']['generator'] = 'mh4u-native player_assembly.py'
        document['extras'] = {'scope': 'Explicit bind-compatible composition candidate; not original equipment selection or a complete playable hunter',
                             'baseEntryId': base_entry, 'acceptedEntryIds': [p['entry_id'] for p in accepted],
                             'visibilityRulesRecovered': False, 'originalShaderReimplemented': False,
                             'coordinates': 'Source model coordinates preserved without placement transforms'}
        for mesh, source in zip(document['meshes'], combined['meshes']):
            mesh['extras'] = {'sourceEntryId': source['source_entry_id'], 'sourcePrimitiveIndex': source['source_primitive_index']}
        validation = model.validate_scene(document, binary)
        files['scene.gltf'], files['scene.bin'] = json.dumps(document, indent=2)+'\n', binary
        outputs = [resource.save(output/name, data) for name, data in sorted(files.items())]
        report = {'schema': 'mh4u-player-assembly-v1', 'source': metadata['source'], 'base_entry_id': base_entry,
                  'explicit_piece_entries': piece_entries, 'explicit_inspect_entries': inspect_entries,
                  'explicit_shared_archive': shared_archive, 'accepted': accepted, 'rejected': rejected,
                  'inspections_not_exported': inspections, 'members': members, 'outputs': outputs, 'validation': validation,
                  'producer_sha256': resource.sha(Path(__file__).read_bytes()),
                  'decoder_sha256': {name: resource.sha(Path(module.__file__).read_bytes()) for name, module in
                                    [('player_probe', player), ('model_scene', model), ('extract_model', resource)]},
                  'references': [model.REFERENCE],
                  'limitations': ['No original equipment/loadout selection established',
                                  'No visibility, skin customization, shader or attachment runtime recovered',
                                  'Inspection-only face/hair alternatives are not overlaid',
                                  'No retargeting, fitted transforms, gameplay controller or scene placement'],
                  'target_executed': False}
        player.save_json(output/'provenance.json', report)
        return {'output': str(output.resolve()), 'accepted_entry_ids': [p['entry_id'] for p in accepted],
                'rejected': rejected, 'inspected_not_exported': inspect_entries,
                'materials': len(bundle['materials']), 'textures': len(bundle['textures']),
                'primitives': len(combined['meshes']), 'validation': validation}
    finally:
        connection.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--index', type=Path, required=True)
    parser.add_argument('--base-entry', type=int, required=True)
    parser.add_argument('--piece-entry', type=int, action='append', default=[])
    parser.add_argument('--inspect-entry', type=int, action='append', default=[])
    parser.add_argument('--shared-archive', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(export(args.index, args.base_entry, args.piece_entry, args.inspect_entry,
                                args.shared_archive, args.output), indent=2))
    except (ValueError, OSError) as error:
        print(json.dumps({'error': getattr(error, 'code', 'invalid_input'), 'message': str(error)}))
        raise SystemExit(2)


if __name__ == '__main__':
    main()
