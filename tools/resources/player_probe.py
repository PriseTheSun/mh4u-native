#!/usr/bin/env python3
"""Inspect player resource associations and export one unassembled body piece.

Source decoding belongs to index_resources, model_scene, stage_scene and
animation_probe. This module selects resources, preserves association evidence
and supplies a complete material bundle without assembling equipment.
"""
import argparse
from collections import Counter
import json
from pathlib import Path

import animation_probe as motion
import extract_model as resource
import index_resources as indexer
import model_scene as model
import stage_scene as material


def save_json(path, value):
    return resource.save(path, json.dumps(value, indent=2)+'\n')


def inventory(index, model_ids, motion_ids, output):
    output.mkdir(parents=True, exist_ok=True)
    source = output/'source'; source.mkdir(exist_ok=True)
    connection, metadata = indexer.open_index(index)
    try:
        query = '''SELECT e.id, a.path AS archive, e.name, e.type_id, e.uncompressed_size,
                   e.compressed_sha256 FROM entries e JOIN archives a ON e.archive_id=a.id
                   WHERE e.name LIKE 'player\\%' ORDER BY a.path,e.ordinal'''
        catalog = [dict(r) for r in connection.execute(query)]
        save_json(output/'player-resource-catalog.json', {'source': metadata['source'], 'entries': catalog})
        models = []
        for identifier in model_ids:
            provenance = indexer.extract(connection, metadata, identifier, source/f'entry-{identifier}.bin')
            raw = (source/f'entry-{identifier}.bin').read_bytes()
            h = resource.unpack('<4s4H13I', raw, 0, 'MOD header')
            material.require(h[0] == b'MOD\0' and h[1] == 230, 'unsupported_mod', 'Selected player model must be MOD230')
            row = {'entry_id': identifier, 'extraction': provenance, 'joints': h[2], 'primitives': h[3],
                   'materials': h[4], 'layouts': sorted(set((raw[h[15]+48*i+10], raw[h[15]+48*i+11]&63) for i in range(h[3])))}
            try:
                skeleton = model.parse_skeleton(raw, h)
                row.update(skeleton_status='parsed', bone_ids=[j['source_function_id_candidate'] for j in skeleton['joints']],
                           skeleton=skeleton)
            except ValueError as error:
                row.update(skeleton_status='unsupported', reason=str(error))
            models.append(row)
        motions = []
        for identifier in motion_ids:
            provenance = indexer.extract(connection, metadata, identifier, source/f'entry-{identifier}.bin')
            parsed = motion.parse_lmt((source/f'entry-{identifier}.bin').read_bytes())
            actions = [a for a in parsed['actions'] if a['status'] != 'empty']
            tracks = [t for a in actions for t in a['tracks']]
            save_json(output/f'motion-{identifier}.json', {'extraction': provenance, 'decoded': parsed})
            row = {'entry_id': identifier, 'extraction': provenance, 'populated_actions': len(actions),
                   'slots_including_empty': len(parsed['actions']), 'tracks': len(tracks),
                   'codec_counts': dict(sorted(Counter(t['codec'] for t in tracks).items())),
                   'keys': sum(len(t['keys'] or []) for t in tracks),
                   'bone_ids': sorted(set(t['bone_id'] for t in tracks)),
                   'unknown_track_count': sum(t['decode_status'] != 'decoded' for t in tracks),
                   'frame_range': [min(a['frame_count'] for a in actions), max(a['frame_count'] for a in actions)],
                   'loop_counts': dict(sorted(Counter(a['loop_frame'] for a in actions).items())), 'model_associations': []}
            for candidate in models:
                if candidate['skeleton_status'] != 'parsed':
                    continue
                ids = candidate['bone_ids']
                missing = sorted(set(row['bone_ids'])-set(ids)-{255})
                association = {'model_entry': candidate['entry_id'], 'missing_non_scene_motion_ids': missing,
                               'ids_unique_in_model': len(ids) == len(set(ids))}
                if not missing and association['ids_unique_in_model']:
                    binding = {b: i for i, b in enumerate(ids)}
                    accepted, rejected = [], []
                    for action in actions:
                        try:
                            motion.prepare_action(parsed, action['slot'], binding, 60)
                            accepted.append(action['slot'])
                        except motion.ProbeError as error:
                            rejected.append({'slot': action['slot'], 'code': error.code, 'message': str(error)})
                    association.update(exportable_slots=accepted, rejected_slots=rejected,
                                       export_convention='60fps preview; scene motion must be exactly zero')
                row['model_associations'].append(association)
            motions.append(row)
        result = {'schema': 'mh4u-player-resource-associations-v1', 'source': metadata['source'],
                  'catalog_count': len(catalog), 'catalog_type_counts': dict(sorted(Counter(r['type_id'] for r in catalog).items())),
                  'models': models, 'motions': motions,
                  'inference': 'player paths and exact bone-ID sets corroborate body/motion association; action names and assembly are not established',
                  'producer_sha256': resource.sha(Path(__file__).read_bytes())}
        save_json(output/'associations.json', result)
        return {'output': str(output.resolve()), 'catalog_entries': len(catalog),
                'models_inspected': len(models), 'motions_inspected': len(motions)}
    finally:
        connection.close()


def load_piece(connection, metadata, identifier, shared_archive, source, *,
               allow_two_influence_vertices=False, allow_single_joint=False, max_vertices=1000000):
    """Load one explicitly selected player MOD and its exact material dependencies.

    This owns extraction, expansion limits, same-archive MRL matching and explicit
    shared-archive texture lookup. It does not choose visible primitives or assemble rigs.
    """
    source.mkdir(parents=True, exist_ok=True)
    members = []
    def extract(entry):
        path = source/f'entry-{entry}.bin'
        provenance = indexer.extract(connection, metadata, entry, path)
        members.append(provenance)
        return path.read_bytes(), provenance['entry']
    raw, entry = extract(identifier)
    material.require(entry['type_id'] == resource.MODEL and entry['name'].startswith('player\\'),
                     'wrong_resource', 'Select a model under the source player namespace')
    h = resource.unpack('<4s4H13I', raw, 0, 'model header')
    material.require(1 <= max_vertices <= 1000000, 'resource_limit', 'Vertex budget outside supported bounds')
    material.require(0 < h[3] <= 4096 and 0 < h[5] <= max_vertices, 'resource_limit', 'Selected model exceeds probe count limits')
    resource.region(raw, h[15], h[3]*48, 'primitive preflight')
    records = [resource.unpack('<HHIHBB9I', raw, h[15]+48*i, 'primitive') for i in range(h[3])]
    material.require(sum(r[1] for r in records) == h[5] and sum(r[10]+1 for r in records) <= 6000000,
                     'resource_limit', 'Cumulative model expansion inconsistent or excessive')
    actor = model.parse_actor(raw, list(range(h[3])), allow_multiple_materials=True,
                                  allow_two_influence_vertices=allow_two_influence_vertices,
                                  allow_single_joint=allow_single_joint)

    def resolve(name, kind, permit_shared):
        rows = connection.execute('SELECT id FROM entries WHERE archive_id=? AND name=? AND type_id=?',
                                  (entry['archive_id'], name, kind)).fetchall()
        if not rows and permit_shared:
            rows = connection.execute('''SELECT e.id FROM entries e JOIN archives a ON e.archive_id=a.id
                WHERE a.path=? AND e.name=? AND e.type_id=?''', (shared_archive, name, kind)).fetchall()
        material.require(len(rows) == 1, 'ambiguous_resource', 'Resource absent or ambiguous in explicit archives: '+name)
        return rows[0][0]

    mrl, _ = extract(resolve(entry['name'], resource.MATERIAL, False))
    materials = material.parse_materials(mrl, actor['material_names'])
    bundle = {'images': [], 'textures': [], 'materials': []}
    texture_indices, textures, files = {}, [], []
    for name in sorted(set(b['texture_name'] for b in materials['bindings'])):
        texture_id = resolve(name, resource.TEXTURE, True)
        tex, _ = extract(texture_id)
        info, rgb, alpha = resource.parse_tex(tex)
        uri = f'texture-{texture_id}.png'
        texture_indices[name] = len(bundle['textures'])
        bundle['images'].append({'uri': uri}); bundle['textures'].append({'source': len(bundle['images'])-1, 'sampler': 0})
        files += [(uri, resource.png_rgb(info['width'], info['height'], rgb)),
                  (f'texture-{texture_id}-alpha.pgm', f'P5\n{info["width"]} {info["height"]}\n255\n'.encode()+alpha)]
        textures.append({'entry_id': texture_id, 'name': name, 'info': info,
                         'alpha_histogram': dict(sorted(Counter(alpha).items()))})
    for binding in materials['bindings']:
        bundle['materials'].append({'name': binding['name'], 'alphaMode': 'OPAQUE',
            'pbrMetallicRoughness': {'baseColorTexture': {'index': texture_indices[binding['texture_name']]},
                                   'metallicFactor': 0, 'roughnessFactor': 1},
            'extras': {'sourceMrlMaterialIndex': binding['mrl_material_index'], 'originalShaderReimplemented': False}})
    return {'actor': actor, 'entry': entry, 'members': members, 'materials': materials,
            'material_bundle': bundle, 'textures': textures, 'files': files}


def export_body(index, identifier, shared_archive, output):
    output.mkdir(parents=True, exist_ok=True)
    source = output/'source'; source.mkdir(exist_ok=True)
    connection, metadata = indexer.open_index(index)
    members = []
    try:
        piece = load_piece(connection, metadata, identifier, shared_archive, source)
        actor, entry, members = (piece[key] for key in ('actor', 'entry', 'members'))
        materials, textures, files = (piece[key] for key in ('materials', 'textures', 'files'))
        bundle = piece['material_bundle']
        document, binary = model.make_gltf(actor, material_bundle=bundle)
        document['extras'] = {'scope': 'One source body piece; no head/arms/legs/equipment assembly',
                              'material': 'Neutral opaque RGB preview; original color customization/shader unknown',
                              'sourceEntryId': identifier, 'sourceName': entry['name'], 'sourceSharedArchive': shared_archive}
        validation = model.validate_scene(document, binary)
        files += [('scene.gltf', json.dumps(document, indent=2)+'\n'), ('scene.bin', binary)]
        outputs = [resource.save(output/name, data) for name, data in files]
        report = {'schema': 'mh4u-player-body-v1', 'source': metadata['source'], 'members': members,
                  'explicit_shared_archive': shared_archive, 'model': {k: v for k, v in actor.items() if k != 'meshes'},
                  'materials': materials, 'textures': textures, 'outputs': outputs, 'validation': validation,
                  'producer_sha256': resource.sha(Path(__file__).read_bytes()),
                  'decoder_sha256': {name: resource.sha(Path(module.__file__).read_bytes()) for name, module in
                                    [('model_scene', model), ('stage_scene', material), ('extract_model', resource)]},
                  'limitations': ['One body resource, not a complete hunter', 'No assembly, visibility or equipment rules recovered',
                                  'No skin-color customization, original shader or alpha semantics',
                                  'No action/gameplay labels, original playback rate or scene-root policy recovered']}
        save_json(output/'provenance.json', report)
        return {'output': str(output.resolve()), 'source_name': entry['name'], 'joints': len(actor['skeleton']['joints']),
                'primitives': len(actor['meshes']), 'materials': len(materials['bindings']), 'textures': len(textures),
                'validation': validation}
    finally:
        connection.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    scan = commands.add_parser('inventory')
    scan.add_argument('--model-entry', type=int, action='append', required=True)
    scan.add_argument('--motion-entry', type=int, action='append', required=True)
    export = commands.add_parser('export-body')
    export.add_argument('--entry', type=int, required=True)
    export.add_argument('--shared-archive', required=True)
    for command in (scan, export):
        command.add_argument('--index', type=Path, required=True)
        command.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        result = (inventory(args.index, args.model_entry, args.motion_entry, args.output) if args.command == 'inventory'
                  else export_body(args.index, args.entry, args.shared_archive, args.output))
        print(json.dumps(result, indent=2))
    except (ValueError, OSError, KeyError) as error:
        print(json.dumps({'error': getattr(error, 'code', 'invalid_input'), 'message': str(error)}))
        raise SystemExit(2)


if __name__ == '__main__':
    main()
