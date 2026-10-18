#!/usr/bin/env python3
"""Bounded offline event inventory for the observed MH4U LMT67 layout.

animation_probe owns LMT/action/track parsing. This module owns the separate
event-block interpretation; it does not modify that owner's frozen outputs.
Event masks and numeric remaps are data, not executable gameplay commands.
"""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import struct
import sys

import animation_probe as motion
from index_resources import open_index, extract
import root_motion_curves as curves

SCHEMA = 'mh4u-lmt67-event-inventory-v1'
PROFILE = 'mh4u-lmt67-four-mask-groups-v1'
GROUPS = 4
GROUP_SIZE = 0x48
MAX_RECORDS = 250000  # Aggregate per source, local resource policy.
MAX_SOURCES = 8
MAX_SLOTS = 4096
EVIDENCE = {
    'loader': 'ev_2341ca4a57100d3cb50d1e37279681b559e3e148d294bc09ad4206286775a533',
    'binder': 'ev_0efaabf914e4e2147baee4244d36d37940dfa9f09102fe65c14cb6348d918fcd',
    'advance': 'ev_4c3dbbd0c7ef787f9b9a28cb4e41842c1e5c74561232066caf921b1cfdcb5dca',
    'advance_assembly': 'ev_e2259ed88779fc4762b5b6172fb6e28dc12984979d10585b7ab717c839131cc8'}
LIMITATIONS = [
    'Mask bits and remap values have no assigned gameplay meanings.',
    'The 32 uint16 prefix entries are remaps in pinned RevilLib; shipped remap dispatch remains unverified.',
    'Source frames are not seconds; no playback rate is inferred.',
    'Stored intervals cover N frames; advance clamps ranges using frameMax=N-1, separately from this inventory.',
    'Range accumulation is the inner integer kernel only; no clock, loop splitting, dispatch or gameplay is selected.',
    'Action flags with 0x02000000 bypass nested relocation and are rejected by this profile.',
    'Zero duration, cumulative signed overflow and empty event pointers remain explicit; they do not imply no gameplay.',
    'No game process, controller or event handler is executed by the inventory producer.']


def byte_range(data, offset, size):
    raw = motion.region(data, offset, size, 'event region')
    return {'offset': offset, 'size': size, 'end_exclusive': offset+size,
            'sha256': motion.sha(raw)}


def event_groups(data, action, budget):
    """Own the 4*72-byte event block; no action-header reinterpretation here."""
    base = action['events_offset_unparsed']
    if base == 0:
        return {'status': 'absent_pointer', 'groups': [], 'block': None}
    motion.require(not action['flags_raw'] & 0x02000000,
                   'unsupported_event_relocation', 'Action bypasses normal nested event relocation', action['offset']+48)
    block = byte_range(data, base, GROUPS*GROUP_SIZE)
    descriptors = []
    remaining = budget
    for index in range(GROUPS):
        offset = base + index*GROUP_SIZE
        fields = motion.unpack('<32HII', data, offset, 'event group')
        remaps, count, pointer = list(fields[:32]), fields[32], fields[33]
        motion.require(count <= remaining, 'resource_limit', 'Aggregate event record policy exceeded', offset+0x40)
        remaining -= count
        motion.require(not count or pointer != 0, 'invalid_event_pointer', 'Nonempty group has a null record pointer', offset+0x44)
        payload = motion.region(data, pointer, count*8, 'event records') if count else b''
        if not count and pointer:
            motion.region(data, pointer, 0, 'empty event pointer')
        descriptors.append((index, offset, remaps, count, pointer, payload))
    groups = []
    for index, offset, remaps, count, pointer, payload in descriptors:
        records, cumulative, union = [], 0, 0
        for ordinal, (mask, duration) in enumerate(struct.iter_unpack('<II', payload)):
            end = cumulative + duration
            bits = [bit for bit in range(32) if mask & (1 << bit)]
            records.append({'ordinal': ordinal, 'offset': pointer+8*ordinal,
                            'raw_hex': payload[8*ordinal:8*ordinal+8].hex(),
                            'mask_raw': mask, 'duration_frames_raw': duration,
                            'start_frame_derived': cumulative, 'end_frame_exclusive_derived': end,
                            'set_bits': bits, 'prefix_u16_for_set_bits': [remaps[bit] for bit in bits]})
            cumulative, union = end, union | mask
        warnings = []
        if cumulative != action['frame_count']:
            warnings.append('duration_sum_differs_from_action_frame_count')
        if any(r['duration_frames_raw'] == 0 for r in records):
            warnings.append('zero_duration_record_not_an_ordinary_half_open_interval')
        if cumulative > 0x7fffffff:
            warnings.append('signed_cumulative_overflow_not_interpreted')
        groups.append({'group_index': index, 'header': byte_range(data, offset, GROUP_SIZE),
                       'header_raw_hex': motion.region(data, offset, GROUP_SIZE, 'event group').hex(),
                       'prefix_u16_raw': remaps, 'prefix_interpretation': 'remap_per_bit_primary_reference_only',
                       'record_count': count, 'records_pointer_raw': pointer,
                       'records_region': byte_range(data, pointer, count*8) if count else None,
                       'content_sha256_without_pointer': motion.sha(struct.pack('<32H', *remaps)+payload),
                       'duration_sum': cumulative, 'action_frame_count': action['frame_count'],
                       'duration_sum_equals_frame_count': cumulative == action['frame_count'],
                       'mask_union': union, 'nonzero_mask_records': sum(r['mask_raw'] != 0 for r in records),
                       'warnings': warnings, 'integer_kernel_supported': cumulative <= 0x7fffffff,
                       'records': records})
    return {'status': 'parsed', 'block': block, 'groups': groups}


def kernel_mask(group, begin, end):
    """Inner shipped signed integer kernel, without range normalization.

    The loop tests cumulative end > begin, ORs the mask, and only continues if
    cumulative end <= end. This deliberately preserves zero-duration behavior.
    No prefix lookup or default/fallback is performed.
    """
    motion.require(group['integer_kernel_supported'], 'unsupported_event_accumulator',
                   'Signed cumulative overflow needs a separately validated profile')
    motion.require(type(begin) is int and type(end) is int and
                   -0x80000000 <= begin <= 0x7fffffff and -0x80000000 <= end <= 0x7fffffff,
                   'invalid_event_kernel_query', 'Kernel inputs must be signed 32-bit integers')
    result = 0
    for record in group['records']:
        cumulative = record['end_frame_exclusive_derived']
        if begin < cumulative:
            result |= record['mask_raw']
        if cumulative > end:
            break
    return result


def range_mask(group, begin, end):
    """Strict inclusive source-frame range; use kernel_mask for raw clock inputs."""
    motion.require(type(begin) is int and type(end) is int and
                   0 <= begin <= end < group['action_frame_count'],
                   'invalid_event_interval', 'Query needs ordered integer source frames in [0,N-1]')
    return kernel_mask(group, begin, end)


def verify_raw_roundtrip(data, result):
    """Reconstruct interpreted fields only, compare each source region exactly."""
    checked = 0
    for action in result['actions']:
        for group in action.get('events', {}).get('groups', []):
            header = struct.pack('<32HII', *group['prefix_u16_raw'], group['record_count'], group['records_pointer_raw'])
            payload = b''.join(struct.pack('<II', r['mask_raw'], r['duration_frames_raw']) for r in group['records'])
            for offset, raw in ((group['header']['offset'], header), (group['records_pointer_raw'], payload)):
                motion.require(raw == motion.region(data, offset, len(raw), 'roundtrip region'),
                               'event_roundtrip_mismatch', 'Reconstructed fields differ from source bytes', offset)
                checked += len(raw)
    return checked


def parse_events(data, parsed, max_records=MAX_RECORDS):
    motion.require(parsed['sha256'] == motion.sha(data) and parsed['size'] == len(data),
                   'source_identity_mismatch', 'Event input differs from the owning LMT parser input')
    motion.require(parsed['format'] == 'LMT' and parsed['version'] == 67,
                   'unsupported_format', 'Only LMT67 events are covered')
    motion.require(len(parsed['actions']) <= MAX_SLOTS and type(max_records) is int and 0 <= max_records <= MAX_RECORDS,
                   'resource_limit', 'Event inventory policy exceeded')
    budget, actions = max_records, []
    for action in parsed['actions']:
        row = {key: action[key] for key in ('slot', 'offset', 'status')}
        if action['status'] != 'empty':
            row.update(source_frame_count=action['frame_count'], source_loop_frame_raw=action['loop_frame'],
                       action_flags_raw=action['flags_raw'], events_pointer_raw=action['events_offset_unparsed'])
            try:
                row['events'] = event_groups(data, action, budget)
                budget -= sum(g['record_count'] for g in row['events']['groups'])
            except motion.ProbeError as error:
                row['events'] = {'status': 'rejected', 'groups': [], 'error': {
                    'code': error.code, 'message': str(error), 'offset': error.offset}}
        actions.append(row)
    result = {'source_lmt_sha256': parsed['sha256'], 'source_size': len(data),
              'slot_count': parsed['action_slot_count'], 'actions': actions}
    result['roundtrip_region_bytes_checked'] = verify_raw_roundtrip(data, result)
    return result


def summarize(sources):
    totals = Counter(); groups = defaultdict(Counter); aliases = defaultdict(list)
    for source in sources:
        for action in source['actions']:
            totals['slots'] += 1
            if action['status'] == 'empty':
                totals['empty_slots'] += 1
                continue
            totals['populated_actions'] += 1
            events = action['events']; totals['event_status_'+events['status']] += 1
            totals['actions_with_nonzero_mask'] += any(g['mask_union'] for g in events['groups'])
            for g in events['groups']:
                totals['groups'] += 1; totals['records'] += g['record_count']
                totals['nonzero_mask_records'] += g['nonzero_mask_records']
                totals['groups_duration_equals_N'] += g['duration_sum_equals_frame_count']
                totals['groups_with_warnings'] += bool(g['warnings'])
                groups[g['group_index']]['groups_with_nonzero_mask'] += bool(g['mask_union'])
                groups[g['group_index']]['records'] += g['record_count']
                groups[g['group_index']]['nonzero_mask_records'] += g['nonzero_mask_records']
                aliases[g['content_sha256_without_pointer']].append([source['entry_id'], action['slot'], g['group_index']])
    return {'totals': dict(totals), 'group_counts': dict(groups),
            'unique_group_content_excluding_pointer': len(aliases),
            'duplicate_group_instances_excluding_pointer': sum(len(v)-1 for v in aliases.values()),
            'duplicate_groups': [{'sha256': key, 'instances': value} for key,value in sorted(aliases.items()) if len(value)>1]}


def build(index, manifest_path, expected_sha, output):
    raw = motion.read(manifest_path)
    motion.require(motion.sha(raw) == expected_sha, 'paired_manifest_identity_mismatch', 'Paired manifest hash differs')
    manifest = json.loads(raw)
    motion.require(isinstance(manifest, dict) and manifest.get('schema') == 'mh4u-paired-actions-v1',
                   'unsupported_manifest', 'Expected paired actions v1')
    sources = manifest.get('sources', [])
    motion.require(isinstance(sources, list) and 0 < len(sources) <= MAX_SOURCES and
                   all(isinstance(s, dict) and type(s.get('entry_id')) is int for s in sources),
                   'invalid_sources', 'Invalid source list')
    motion.require(len({s['entry_id'] for s in sources}) == len(sources), 'duplicate_source', 'Sources repeat an entry')
    pairs = manifest.get('pairs', [])
    motion.require(isinstance(pairs, list) and 0 < len(pairs) <= 64 and all(isinstance(p, dict) for p in pairs),
                   'invalid_pairs', 'Invalid paired selections')
    motion.require(not output.exists(), 'existing_output', 'Choose a new output directory')
    connection, metadata = open_index(index)
    inventories, provenance = [], []
    try:
        for source in sources:
            row = connection.execute('SELECT e.type_id,e.name,a.path archive FROM entries e JOIN archives a ON a.id=e.archive_id WHERE e.id=?', (source['entry_id'],)).fetchone()
            motion.require(row is not None and row['type_id'] == motion.MOTION_TYPE and
                           row['name'] == source.get('name') and row['archive'] == source.get('archive'),
                           'indexed_source_mismatch', 'Indexed source identity differs from paired manifest')
        output.mkdir(parents=True); (output/'sources').mkdir()
        for source in sources:
            path = output/'sources'/f"entry-{source['entry_id']}.lmt"
            extraction = extract(connection, metadata, source['entry_id'], path)
            data = motion.read(path)
            motion.require(motion.sha(data) == source['sha256'] and len(data) == source['size'],
                           'source_identity_mismatch', 'Extracted LMT differs from paired source')
            parsed = motion.parse_lmt(data)
            inventories.append({'entry_id': source['entry_id'], 'archive': source['archive'], 'name': source['name'],
                                **parse_events(data, parsed)})
            provenance.append(extraction)
    finally:
        connection.close()
    lookup = {s['entry_id']: s for s in inventories}; selected = []; seen = set()
    for pair in pairs:
        entry, slot = pair.get('entry_id'), pair.get('source_slot')
        motion.require(type(entry) is int and entry in lookup and type(slot) is int and
                       0 <= slot < len(lookup[entry]['actions']) and (entry,slot) not in seen,
                       'invalid_pair', 'Pair identity is invalid or duplicate')
        seen.add((entry,slot)); source = lookup[entry]; action = source['actions'][slot]
        motion.require(source['source_lmt_sha256'] == pair.get('source_lmt_sha256') and
                       action.get('source_frame_count') == pair.get('source_frame_count'),
                       'pair_source_mismatch', 'Pair source hash/frame count differs')
        selected.append({'entry_id': entry, 'source_lmt_sha256': source['source_lmt_sha256'],
                         'pair_index': pair['pair_index'], 'joint_clip_index': pair['joint_clip_index'], **action})
    report = {'schema': SCHEMA, 'profile': PROFILE, 'lmt_owner_profile': motion.DECODER_PROFILE,
              'paired_manifest': {'path': str(manifest_path.resolve()), 'sha256': expected_sha},
              'sources': inventories, 'selected_actions': selected,
              'summary': summarize(inventories), 'selected_summary': summarize([
                  {'entry_id': a['entry_id'], 'actions': [a]} for a in selected]),
              'evidence': EVIDENCE, 'limitations': LIMITATIONS, 'runtime_enabled': False}
    result = curves.write_new(output/'inventory.json', report)
    curves.write_new(output/'provenance.json', {'schema': SCHEMA, 'inventory': result, 'extractions': provenance,
        'producers': [{'path': str(Path(p).resolve()), 'sha256': motion.sha(Path(p).read_bytes())}
                      for p in (__file__, motion.__file__)], 'source_rebuilt': False,
        'roundtrip_scope': 'All decoded group headers and record payloads; untouched LMT extracted verbatim'})
    return {'inventory': result, 'summary': report['summary']['totals'], 'selected': report['selected_summary']['totals']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--index', type=Path, required=True)
    parser.add_argument('--paired-manifest', type=Path, required=True)
    parser.add_argument('--expected-sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(build(args.index, args.paired_manifest, args.expected_sha256, args.output), indent=2))
    except (motion.ProbeError, OSError, ValueError, KeyError) as error:
        print(json.dumps({'error': getattr(error, 'code', 'invalid_input'), 'message': str(error)}), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
