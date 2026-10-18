#!/usr/bin/env python3
"""Pair local joint clips with verified, separate LMT67 model-motion curves.

No model motion is applied and no runtime fallback is selected. Existing curve
bytes are checked through their owner and copied verbatim, never regenerated.
"""
import argparse
import json
from pathlib import Path
import sqlite3
import sys

import animation_probe as motion
import root_motion_curves as curves
from index_resources import open_index, extract

SCHEMA = 'mh4u-paired-actions-v1'


def record(path, root):
    data = motion.read(path)
    return {'uri': path.relative_to(root).as_posix(), 'sha256': motion.sha(data), 'size': len(data)}


def verify_curve_action(packaged, parsed):
    """Compare parsed facts through the curve owner, including samples/nulls."""
    motion.require(isinstance(packaged, dict) and type(packaged.get('slot')) is int,
                   'invalid_curve_action', 'Packaged action lacks an integer slot')
    reproduced = {'entry_id': packaged.get('entry_id'), **curves.action_curve(parsed, packaged['slot'])}
    motion.require(packaged == json.loads(curves.encoded(reproduced)),
                   'model_curve_content_mismatch', 'Packaged model curve differs from its current owning parser/sampler')
    return reproduced


def build(index, curve_path, expected_sha256, model, output, research):
    raw = motion.read(curve_path)
    motion.require(motion.sha(raw) == expected_sha256, 'model_curve_hash_mismatch', 'Selected curve package differs from the caller-pinned SHA-256')
    package = json.loads(raw)
    motion.require(isinstance(package, dict) and package.get('schema') == curves.SCHEMA and
                   package.get('decoder_profile') == motion.DECODER_PROFILE,
                   'unsupported_curve_package', 'Expected the current source-frame model curve schema/profile')
    sources, actions = package.get('sources'), package.get('actions')
    motion.require(isinstance(sources, list) and 0 < len(sources) <= curves.MAX_SOURCES and
                   isinstance(actions, list) and 0 < len(actions) <= curves.MAX_SELECTIONS,
                   'invalid_curve_package', 'Curve package sources/actions exceed the supported shape or limits')
    motion.require(all(isinstance(s, dict) and type(s.get('entry_id')) is int and s['entry_id'] > 0 for s in sources),
                   'invalid_curve_source', 'Curve sources need positive indexed entry IDs')
    entries = [s['entry_id'] for s in sources]
    motion.require(len(entries) == len(set(entries)), 'duplicate_curve_source', 'Curve source identities must be unique')
    motion.require(all(isinstance(a, dict) and a.get('entry_id') in entries and type(a.get('slot')) is int for a in actions),
                   'invalid_curve_action', 'Curve actions must reference declared sources and integer slots')
    identities = [(a['entry_id'], a['slot']) for a in actions]
    motion.require(len(identities) == len(set(identities)), 'duplicate_curve_action', 'Curve action identities must be unique')
    output, research = output.resolve(), research.resolve()
    motion.require(not output.exists() and not research.exists(), 'existing_output', 'Choose new artifact and research directories')
    motion.require(output != research and output not in research.parents and research not in output.parents,
                   'overlapping_output', 'Artifact and research directories must be separate')
    connection, meta = open_index(index)
    parsed, paths, extraction_records = {}, {}, []
    try:
        # Preflight indexed source names/types without creating output paths.
        for source in sources:
            row = connection.execute('SELECT e.type_id,e.name,a.path archive FROM entries e JOIN archives a ON a.id=e.archive_id WHERE e.id=?',
                                     (source['entry_id'],)).fetchone()
            motion.require(row is not None and row['type_id'] == motion.MOTION_TYPE and
                           row['name'] == source.get('name') and row['archive'] == source.get('archive'),
                           'indexed_source_mismatch', 'Curve source name/archive/type does not match the selected index')
        research.mkdir(parents=True)
        for source in sources:
            entry = source['entry_id'];path = research/f'entry-{entry}.lmt'
            provenance = extract(connection, meta, entry, path)
            motion.require(provenance['sha256'] == source.get('sha256') and provenance['size'] == source.get('size'),
                           'lmt_identity_mismatch', 'Fresh extraction differs from the curve package LMT identity')
            parsed[entry] = motion.parse_lmt(motion.read(path))
            paths[entry] = path
            extraction_records.append(provenance)
        bindings, selections = [], []
        for i, action in enumerate(actions):
            entry, slot = action['entry_id'], action['slot']
            verify_curve_action(action, parsed[entry])
            source_action = parsed[entry]['actions'][slot]
            ordinals = tuple(t['ordinal'] for t in source_action['tracks'] if t['track_type'] in (3, 4))
            bindings.append(motion.ModelCurveBinding(parsed[entry]['sha256'], slot, entry, i,
                '../model-curves.json', expected_sha256, ordinals))
            selections.append((paths[entry], parsed[entry], slot))
    finally:
        connection.close()
    output.mkdir(parents=True)
    # Preserve all original metadata and absent/null markers byte for byte.
    with (output/'model-curves.json').open('xb') as handle:
        handle.write(raw)
    first_path, first_parsed, first_slot = selections[0]
    report = motion.export_gltf(first_path, first_parsed, first_slot, model, 1.0, output/'joints',
                               selections, model_curve_bindings=bindings)
    motion.require(report['exported_actions'] == len(actions) and not report['rejected_actions'],
                   'partial_pair_export', 'Paired export must contain every selected action')
    pairs = []
    for i, (action, exported, (_, source, slot)) in enumerate(zip(actions, report['actions'], selections)):
        joint_tracks = [c['track'] for c in exported['channels']]
        external_tracks = [c['track'] for c in exported['model_channels_external']['tracks']]
        all_tracks = [t['ordinal'] for t in source['actions'][slot]['tracks']]
        motion.require(not exported['omitted'] and sorted(joint_tracks+external_tracks) == all_tracks and
                       len(set(joint_tracks+external_tracks)) == len(all_tracks),
                       'incomplete_track_partition', 'Every source track must have exactly one explicit destination')
        pairs.append({'pair_index': i, 'entry_id': action['entry_id'], 'source_lmt_sha256': source['sha256'],
            'source_slot': slot, 'joint_clip_index': i, 'model_curve_index': i,
            'source_frame_count': action['source_frame_count'], 'source_frame_end': action['frame_end'],
            'source_loop_frame_raw': action['source_loop_frame_raw'],
            'joint_time_mapping': {'gltf_time_unit': 'seconds', 'source_frames_per_gltf_second': 1,
                                  'original_game_rate_verified': False},
            'joint_track_ordinals': joint_tracks, 'external_model_track_ordinals': external_tracks,
            'source_track_count': len(all_tracks), 'complete_track_partition': True,
            'absent_model_components': [component for component, channel in action['channels'].items() if channel['status']=='absent'],
            'runtime_default_binding': None})
    files = [record(p, output) for p in sorted(output.rglob('*')) if p.is_file()]
    manifest = {'schema': SCHEMA, 'decoder_profile': motion.DECODER_PROFILE,
        'joint_gltf': 'joints/animated.gltf', 'model_curves': 'model-curves.json', 'files': files,
        'pairs': pairs, 'source_model_sha256': motion.sha(motion.read(model)),
        'sources': sources, 'model_motion_applied': False, 'runtime_defaults_applied': False,
        'controller_policy': None, 'gameplay_names': None, 'enabled_for_playback': False,
        'limits': ['Joint glTF and model curves must be consumed as a pair; ordinary glTF playback omits model motion.',
                   'glTF seconds have one unit per source frame solely for explicit domain mapping, not original speed.',
                   'No original controller, runtime attributes, missing-component defaults, events or collision policy is supplied.',
                   'Model geometry/materials retain the selected rest-pose asset limitations; equipment assembly is not newly inferred.']}
    manifest_record = curves.write_new(output/'manifest.json', manifest)
    provenance = {'schema': SCHEMA, 'manifest': manifest_record, 'curve_input': {'path': str(curve_path.resolve()), 'sha256': expected_sha256},
        'source_model': {'path': str(model.resolve()), 'sha256': motion.sha(motion.read(model))},
        'source_extractions': extraction_records, 'rom_identity_from_index': meta['source'],
        'whole_rom_rehashed': False, 'producer_sha256': motion.sha(Path(__file__).read_bytes()),
        'animation_owner_sha256': motion.sha(Path(motion.__file__).read_bytes()),
        'curve_owner_sha256': motion.sha(Path(curves.__file__).read_bytes()),
        'all_curve_actions_revalidated_with_owner': True, 'curve_bytes_preserved': True,
        'joint_animation_provenance': record(output/'joints/animation-provenance.json', output),
        'original_game_executed': False, 'native_application_executed': False}
    provenance_record = curves.write_new(research/'provenance.json', provenance)
    return {'ok': True, 'manifest': manifest_record, 'provenance': provenance_record,
            'paired_actions': len(pairs), 'joint_channels': sum(len(p['joint_track_ordinals']) for p in pairs),
            'external_model_channels': sum(len(p['external_model_track_ordinals']) for p in pairs)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--index', type=Path, required=True)
    parser.add_argument('--curves', type=Path, required=True)
    parser.add_argument('--curves-sha256', required=True)
    parser.add_argument('--model', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--research', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(build(args.index, args.curves, args.curves_sha256, args.model, args.output, args.research)))
        return 0
    except (motion.ProbeError, ValueError, OSError, sqlite3.Error) as error:
        print(json.dumps({'ok': False, 'error': {'code': getattr(error, 'code', 'input_error'), 'message': str(error)}}))
        return 2


if __name__ == '__main__':
    sys.exit(main())
