#!/usr/bin/env python3
"""Export explicit LMT67 model curves, without applying motion or choosing gameplay.

LMT parsing, codec equations and sampling belong to animation_probe.py. This
adapter selects type 3/4 channels and packages their source identity and values.
It neither executes game instructions nor changes glTF/installed assets.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import sqlite3
import sys

import animation_probe as motion
from index_resources import open_index, extract

SCHEMA = 'mh4u-model-motion-curves-v1'
MAX_SELECTIONS = 16
MAX_SOURCES = 8
MAX_SAMPLES = 100000  # Export policy, not a format limit.
BIND_EVIDENCE = 'ev_0efaabf914e4e2147baee4244d36d37940dfa9f09102fe65c14cb6348d918fcd'
LIMITS = [
    'Frames are source coordinates; no seconds, FPS or gameplay clock is assigned.',
    'XYZ and XYZW are unchanged sampler outputs; distance units and model-parent/world mapping are unknown.',
    'A sampled position is not a per-update displacement; previous state and runtime attributes remain required.',
    'No runtime attributes, reset templates, default rotation, event behavior or controller policy are inferred.',
    'The normal binder loop clamp is recorded separately from the raw action field; it is not a playback-mode decision.',
    'Python double arithmetic with shipped float32 literals; VFP instruction rounding is not emulated.',
    'No original game, isolated ARM routine, native application or viewer playback is executed by this export.'
]


def encoded(value):
    return (json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False)+'\n').encode('utf-8')


def write_new(path, value):
    data = encoded(value)
    with path.open('xb') as handle:
        handle.write(data)
    return {'path': str(path.resolve()), 'size': len(data), 'sha256': motion.sha(data)}


def model_tracks(action):
    """The shipped binder selects by type; bone_id=255 is not the selector."""
    selected = {}
    for track in action['tracks']:
        if track['track_type'] not in (3, 4):
            continue
        component = 'rotation' if track['track_type'] == 3 else 'translation'
        motion.require(component not in selected, 'duplicate_model_channel',
                       'Repeated model component needs a separately validated overwrite/blending rule')
        selected[component] = track
    return selected


def describe_channel(track, action):
    # Fail closed at the point-sampler boundary as well as the key decoder.
    motion.sample_model_channel(track, action, [0, action['frame_count']-1])
    times, values = motion.raw_channel_keys(track, action, 1)
    constant = all(row == values[0] for row in values)
    zero = all(value == 0 for row in values for value in row)
    return {
        'track_ordinal': track['ordinal'], 'track_type': track['track_type'],
        'bone_id_raw': track['bone_id'], 'bone_type_raw': track['bone_type_raw'],
        'weight_raw': track['weight_raw'], 'codec': track['codec'], 'codec_name': track['codec_name'],
        'record_offset': track['record_offset'], 'record_raw_hex': track['record_raw_hex'],
        'buffer_offset': track['buffer_offset'], 'buffer_size': track['buffer_size'],
        'buffer_sha256': track['buffer_sha256'], 'extremes_offset': track['extremes_offset'],
        'extremes_scale_bias': track['extremes_scale_bias'],
        'reference_raw': track['reference_xyzw_or_xyz'],
        'source_key_count': len(track['keys']),
        'source_last_key_frame': track['keys'][-1]['frame'] if track['keys'] else None,
        'constant': constant, 'all_components_exactly_zero': zero,
        'interpolation': 'constant_reference' if track['codec'] in (1, 2) else 'linear_vector',
        'key_frames': times, 'key_values': values,
        'minimum': [min(row[i] for row in values) for i in range(len(values[0]))],
        'maximum': [max(row[i] for row in values) for i in range(len(values[0]))]
    }


def action_curve(parsed, slot):
    motion.require(type(slot) is int and 0 <= slot < len(parsed['actions']), 'invalid_slot', 'Slot does not exist')
    action = parsed['actions'][slot]
    motion.require(action['status'] == 'parsed', 'empty_slot', 'Selected action is empty')
    selected = model_tracks(action)
    motion.require(bool(selected), 'missing_model_channels', 'No type 3/4 model channel is present')
    end, loop = action['frame_count']-1, max(action['loop_frame'], 0)
    count = 2*end+1
    motion.require(count <= MAX_SAMPLES, 'resource_limit', 'Half-frame sampling exceeds local point limit')
    frames = [i/2 for i in range(count)]
    channels = {}
    samples = {component: None for component in ('translation', 'rotation')}
    for component in samples:
        track = selected.get(component)
        if track is None:
            channels[component] = {'status': 'absent', 'track': None, 'default': None,
                                   'reason': 'No track of this model component; runtime fallback not inferred'}
        else:
            description = describe_channel(track, action)
            samples[component] = motion.sample_model_channel(track, action, frames)
            description.update(status='present', samples=samples[component])
            channels[component] = description
    translation = selected.get('translation')
    base = None
    if translation is not None:
        at_loop, at_end = motion.sample_model_channel(translation, action, [loop, end])
        base = {'profile': 'normal binder branch without runtime attr 8', 'loop_sample': at_loop,
                'end_sample': at_end, 'end_minus_loop': [b-a for a,b in zip(at_loop, at_end)],
                'application_selected': False}
    return {
        'source_lmt_sha256': parsed['sha256'], 'slot': slot, 'action_offset': action['offset'],
        'action_header_raw_hex': action['header_raw_hex'], 'source_frame_count': action['frame_count'],
        'frame_end': end, 'source_loop_frame_raw': action['loop_frame'],
        'normal_binder_loop_frame_derived': loop, 'action_flags_raw': action['flags_raw'],
        'runtime_attributes': None, 'source_end_position_raw': action['scene_end_position_raw'],
        'source_end_rotation_raw': action['scene_end_rotation_raw'],
        'events_offset_unparsed': action['events_offset_unparsed'],
        'float_tracks_offset_unparsed': action['float_tracks_offset_unparsed'],
        'sample_frames': frames, 'channels': channels,
        'normal_binder_translation_base_derived': base,
        'joint_channels_exported': False, 'enabled_for_playback': False
    }


def inventory(parsed, binding):
    rows, codecs, statuses = [], Counter(), Counter()
    for action in parsed['actions']:
        if action['status'] == 'empty':
            continue
        row = {'slot': action['slot'], 'frame_count': action['frame_count'],
               'loop_frame_raw': action['loop_frame'], 'model_channels': []}
        try:
            selected = model_tracks(action)
            for component, track in selected.items():
                description = describe_channel(track, action)
                codecs[f"type{track['track_type']}/codec{track['codec']}"] += 1
                row['model_channels'].append({key: description[key] for key in
                    ('track_ordinal', 'track_type', 'bone_id_raw', 'codec', 'constant',
                     'all_components_exactly_zero', 'minimum', 'maximum')})
            row['model_sampler_status'] = 'supported' if selected else 'absent'
        except motion.ProbeError as error:
            row['model_sampler_status'] = 'rejected'
            row['model_sampler_error'] = {'code': error.code, 'message': str(error)}
        statuses[row['model_sampler_status']] += 1
        try:
            motion.prepare_action(parsed, action['slot'], binding, 1)
            row['existing_joint_export'] = {'status': 'accepted'}
        except motion.ProbeError as error:
            row['existing_joint_export'] = {'status': 'rejected', 'code': error.code, 'reason': str(error)}
        rows.append(row)
    return {'slot_count': parsed['action_slot_count'], 'populated_actions': len(rows),
            'empty_slots': parsed['action_slot_count']-len(rows), 'model_channel_codecs': dict(codecs),
            'model_sampler_statuses': dict(statuses),
            'existing_joint_export_accepted': sum(r['existing_joint_export']['status']=='accepted' for r in rows),
            'existing_joint_export_rejected': sum(r['existing_joint_export']['status']=='rejected' for r in rows),
            'actions': rows}


def build(index, entries, selections, model, output, research):
    motion.require(0 < len(entries) <= MAX_SOURCES and len(set(entries)) == len(entries),
                   'invalid_selection', 'Choose unique source entries within the local eight-source limit')
    motion.require(0 < len(selections) <= MAX_SELECTIONS and len(set(selections)) == len(selections),
                   'invalid_selection', 'Choose unique entry:slot pairs within the local sixteen-action limit')
    motion.require(all(entry in entries for entry, slot in selections), 'invalid_selection', 'Every selected slot needs its source entry')
    motion.require(not output.exists() and not research.exists(), 'existing_output', 'Choose new artifact and research directories')
    output, research = output.resolve(), research.resolve()
    motion.require(output != research and output not in research.parents and research not in output.parents,
                   'overlapping_output', 'Artifact and research directories must be separate')
    model_bytes = motion.read(model)
    doc = json.loads(model_bytes)
    binding = {}
    for i, node in enumerate(doc.get('nodes', [])):
        bone = node.get('extras', {}).get('sourceFunctionIdCandidate')
        if bone is not None:
            motion.require(type(bone) is int and bone not in binding, 'ambiguous_bone', 'Model mapping is invalid/duplicated')
            binding[bone] = i
    motion.require(bool(binding), 'missing_bone_mapping', 'Model lacks source joint identities')
    connection, meta = open_index(index)
    try:
        # Validate all source types before creating directories or extracting.
        for entry in entries:
            row = connection.execute('SELECT type_id FROM entries WHERE id=?', (entry,)).fetchone()
            motion.require(row is not None and row['type_id'] == motion.MOTION_TYPE,
                           'not_motion_entry', 'Selected entry is not an indexed rMotionList')
        research.mkdir(parents=True)
        sources, inventories, parsed_sources = [], [], {}
        for entry in entries:
            source_path = research/f'entry-{entry}.lmt'
            extraction = extract(connection, meta, entry, source_path)
            parsed = motion.parse_lmt(motion.read(source_path))
            motion.require(parsed['sha256'] == extraction['sha256'], 'source_identity_mismatch', 'Extracted resource changed during parse')
            parsed_sources[entry] = parsed
            sources.append({'entry_id': entry, 'archive': extraction['entry']['archive'],
                            'name': extraction['entry']['name'], 'sha256': parsed['sha256'],
                            'size': parsed['size'], 'extraction': extraction})
            inventories.append({'entry_id': entry, 'sha256': parsed['sha256'], **inventory(parsed, binding)})
        actions = []
        total_samples = 0
        for entry, slot in selections:
            action = action_curve(parsed_sources[entry], slot)
            total_samples += len(action['sample_frames'])
            motion.require(total_samples <= MAX_SAMPLES, 'resource_limit', 'Combined sample timeline exceeds local point limit')
            actions.append({'entry_id': entry, **action})
    finally:
        connection.close()
    adapter_bytes = Path(__file__).read_bytes()
    owner_bytes = Path(motion.__file__).read_bytes()
    package = {'schema': SCHEMA, 'decoder_profile': motion.DECODER_PROFILE,
               'sampler_owner_sha256': motion.sha(owner_bytes), 'adapter_sha256': motion.sha(adapter_bytes),
               'time_domain': 'source_frames', 'sample_policy': 'Every integer and half-integer inside [0,N-1]',
               'coordinate_policy': 'Raw model-layer sampler XYZ/XYZW; no axis conversion or model/world transform',
               'sources': [{k: s[k] for k in ('entry_id', 'archive', 'name', 'sha256', 'size')} for s in sources],
               'actions': actions, 'limits': LIMITS, 'runtime_validated': False}
    output.mkdir(parents=True)
    artifact = write_new(output/'curves.json', package)
    inv = write_new(research/'inventory.json', {'decoder_profile': motion.DECODER_PROFILE,
                    'sampler_owner_sha256': motion.sha(owner_bytes), 'sources': inventories})
    provenance = {'schema': SCHEMA, 'artifact': artifact, 'inventory': inv, 'sources': sources,
        'index': {'path': str(index.resolve()), 'sha256': motion.sha(index.read_bytes()),
                  'rom_identity': meta['source'], 'whole_rom_rehashed_in_this_export': False},
        'model_for_existing_export_comparison': {'path': str(model.resolve()), 'sha256': motion.sha(model_bytes)},
        'producer': {'adapter': str(Path(__file__).resolve()), 'sha256': motion.sha(adapter_bytes),
                     'sampler_owner': str(Path(motion.__file__).resolve()), 'sampler_sha256': motion.sha(owner_bytes)},
        'binding_evidence': BIND_EVIDENCE, 'sampler_evidence': motion.ARM_EVIDENCE,
        'limits': LIMITS, 'selected_actions': len(actions), 'sample_timeline_points': total_samples,
        'coverage': 'All populated actions in the explicitly selected resources; exported curves only for selected slots'}
    report = write_new(research/'provenance.json', provenance)
    return {'ok': True, 'artifact': artifact, 'provenance': report,
            'actions': len(actions), 'sample_timeline_points': total_samples,
            'inventoried_actions': sum(i['populated_actions'] for i in inventories),
            'existing_joint_export_rejected': sum(i['existing_joint_export_rejected'] for i in inventories)}


def selection(value):
    try:
        entry, slot = value.split(':')
        entry, slot = int(entry), int(slot)
        if entry <= 0 or slot < 0:
            raise ValueError()
        return entry, slot
    except ValueError as error:
        raise argparse.ArgumentTypeError('Use positive-entry:nonnegative-slot') from error


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--index', type=Path, required=True)
    ap.add_argument('--entry', type=int, action='append', required=True)
    ap.add_argument('--select', type=selection, action='append', required=True)
    ap.add_argument('--model', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--research', type=Path, required=True)
    args = ap.parse_args()
    try:
        print(json.dumps(build(args.index, args.entry, args.select, args.model, args.output, args.research)))
        return 0
    except (motion.ProbeError, ValueError, OSError, sqlite3.Error) as error:
        print(json.dumps({'ok': False, 'error': {'code': getattr(error, 'code', 'input_error'),
                         'message': str(error)}}))
        return 2


if __name__ == '__main__':
    sys.exit(main())
