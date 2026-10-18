#!/usr/bin/env python3
"""Bounded offline MH4U LMT67 probe and conservative glTF animation exporter.

No game execution. Format equations are independently implemented against local
bytes; pinned primary-source references and remaining unknowns are documented
in docs/animation-findings.md. Unsupported semantics fail explicitly on export.
"""
import argparse
from bisect import bisect_right
from dataclasses import dataclass
import copy
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
from urllib.parse import unquote, urlparse

from extract_model import save

MOTION_TYPE = 0x76820D81
MAX_BYTES = 64 * 1024 * 1024
MAX_TRACKS = 4096  # Local probe policies, not file-format maxima.
MAX_KEYS = 250000
REFERENCE_COMMIT = 'e17f9718ea571f040a7e1f331c28a4d45a6066b5'
DECODER_PROFILE = 'mh4u-lmt67-arm-sampler-v2'
# Exact IEEE-754 literals in shipped MH4U ARM samplers 0xae9910/0xae92e4.
# Arithmetic below remains Python double followed by glTF float32 conversion;
# it is not an emulation of VFP rounding, interpolation or FP environment.
VECTOR_QUANT_FACTORS = {4: struct.unpack('<f', bytes.fromhex('01088037'))[0],
                        5: struct.unpack('<f', bytes.fromhex('8988883b'))[0]}
QUATERNION_QUANT_FACTORS = {6: struct.unpack('<f', bytes.fromhex('00008038'))[0],
                            7: struct.unpack('<f', bytes.fromhex('2549123c'))[0],
                            11: struct.unpack('<f', bytes.fromhex('08208038'))[0]}
QUATERNION_CODECS = (6, 7, 11, 12, 13)
ARM_EVIDENCE = {
    'codec4': 'ev_9f30f4f3d29cfe67b5e7931b86ab1bea6a5424568cac03c2dda1d9a5692c214c',
    'codec5': 'ev_dfbc336ac239e12306c696c4a0f7022e2d6d64553b55f1ae440b0fdbe70f84a2',
    'sampler': 'ev_02441f313eda39bd06c1494f9ebfa8bec85876427ca64be0dde2515738c9d660',
    'cursor_init': 'ev_dd2d5709fb192acce83e6f7116cdeb0e0105d7ed2b39752aa6b21c562e2e8159',
    'frame_max': 'ev_536563d3f719d8414b35f84687804ac09d3f3d0ced61bd6608dd9e918e4a0bda'}
CODECS = {1: ('vector3-float', 12), 2: ('quaternion-xyz-float', 12),
          3: ('vector3-float-delta32', 16), 4: ('vector3-biased16-delta16', 8),
          5: ('vector3-biased8-delta8', 4), 6: ('quaternion-signed14-delta8', 8),
          7: ('quaternion-unorm7-delta4', 4), 11: ('quaternion-xw-unorm14-delta4', 4),
          12: ('quaternion-yw-unorm14-delta4', 4), 13: ('quaternion-zw-unorm14-delta4', 4)}
PATHS = {0: 'rotation', 1: 'translation', 2: 'scale', 3: 'absolute_rotation', 4: 'absolute_translation'}


class ProbeError(ValueError):
    def __init__(self, code, message, offset=None):
        super().__init__(message)
        self.code, self.offset = code, offset


def require(ok, code, message, offset=None):
    if not ok:
        raise ProbeError(code, message, offset)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read(path):
    with path.open('rb') as handle:
        data = handle.read(MAX_BYTES + 1)
    require(len(data) <= MAX_BYTES, 'resource_limit', 'Input exceeds local 64 MiB policy')
    return data


def region(data, offset, size, label):
    require(type(offset) is int and type(size) is int and offset >= 0 and size >= 0
            and offset <= len(data) and size <= len(data) - offset,
            'invalid_bounds', label + ' lies outside input', offset)
    return data[offset:offset + size]


def unpack(fmt, data, offset, label):
    return struct.unpack(fmt, region(data, offset, struct.calcsize(fmt), label))


def finite(values, label):
    require(all(math.isfinite(x) for x in values), 'nonfinite_value', label + ' contains nonfinite values')


def normalize(q):
    finite(q, 'quaternion')
    length = math.sqrt(sum(x*x for x in q))
    require(length > 1e-12, 'zero_quaternion', 'Cannot normalize a zero quaternion')
    return [x / length for x in q]


def float32(value):
    try:
        return struct.unpack('<f', struct.pack('<f', value))[0]
    except (OverflowError, struct.error) as error:
        raise ProbeError('float32_overflow', 'Value cannot be represented as float32') from error


def slerp(a, b, fraction):
    a, b = normalize(a), normalize(b)
    dot = sum(x*y for x, y in zip(a, b))
    if dot < 0:
        b, dot = [-x for x in b], -dot
    dot = min(1.0, max(-1.0, dot))
    if dot > 0.9995:
        return normalize([x + fraction*(y-x) for x, y in zip(a, b)])
    angle = math.acos(dot)
    return normalize([(x*math.sin((1-fraction)*angle) + y*math.sin(fraction*angle))/math.sin(angle)
                      for x, y in zip(a, b)])


def decode_buffer(codec, payload, extremes):
    require(codec in CODECS, 'unknown_codec', f'Unsupported LMT67 compression {codec}')
    stride = CODECS[codec][1]
    require(len(payload) % stride == 0, 'invalid_codec_size', 'Key buffer length is not a multiple of codec stride')
    if codec in (4, 5, 7, 11, 12, 13):
        require(extremes is not None, 'missing_extremes', 'Quantized codec requires scale/bias block')
    else:
        require(extremes is None, 'unexpected_extremes', 'Scale/bias on this codec is not validated')
    keys, current_frame = [], 0
    for offset in range(0, len(payload), stride):
        row = payload[offset:offset + stride]
        if codec in (1, 2, 3):
            value = list(struct.unpack_from('<3f', row))
            delta = struct.unpack_from('<I', row, 12)[0] if codec == 3 else 1
            if codec == 2:
                squared = sum(x*x for x in value)
                require(squared <= 1.00001, 'invalid_quaternion_xyz', 'Quaternion XYZ lies outside unit sphere')
                value += [math.sqrt(max(0.0, 1-squared))]
        elif codec in (4, 5):
            values = struct.unpack('<4H' if codec == 4 else '<4B', row)
            value = [(x-8)*VECTOR_QUANT_FACTORS[codec] for x in values[:3]]
            delta = values[3]
        else:
            word = int.from_bytes(row, 'little')
            if codec == 6:
                fields = [(word >> shift) & 16383 for shift in (42, 28, 14, 0)]
                # SXTH(field << 2), then multiply by the exact VFP literal.
                value = [((x if x < 8192 else x-16384)*4)*QUATERNION_QUANT_FACTORS[6] for x in fields]
                delta = word >> 56
            elif codec == 7:
                # SUB wraps uint32; VCVT.F32.U32 rounds before multiplication.
                # Values below 8 are not in the sampled corpus but remain exact
                # at this conversion boundary (approximately 2^32, not negative).
                value = [float32((((word >> shift) & 127)-8) & 0xffffffff)*QUATERNION_QUANT_FACTORS[7]
                         for shift in (21, 14, 7, 0)]
                delta = word >> 28
            else:
                value = [0.0, 0.0, 0.0, (((word >> 14) & 16383)-8)*QUATERNION_QUANT_FACTORS[11]]
                value[codec-11] = ((word & 16383)-8)*QUATERNION_QUANT_FACTORS[11]
                delta = word >> 28
        if extremes is not None:
            value = [extremes[4+i] + extremes[i]*component for i, component in enumerate(value)]
        finite(value, 'decoded key')
        keys.append({'frame': current_frame, 'next_frame_delta': delta, 'buffer_offset': offset, 'value': value})
        require(delta > 0 or offset + stride == len(payload), 'nonincreasing_frames', 'Zero frame delta before last key')
        current_frame += delta
        require(current_frame <= 1000000, 'resource_limit', 'Decoded frame span exceeds local million-frame policy')
    return keys


def parse_lmt(data):
    magic, version, count = unpack('<4sHH', data, 0, 'LMT header')
    require(magic == b'LMT\0' and version == 67, 'unsupported_format', 'Only observed little-endian LMT version 67 is supported')
    offsets = unpack('<' + str(count) + 'I', data, 8, 'action offset table')
    actions, total_tracks, total_keys = [], 0, 0
    for slot, offset in enumerate(offsets):
        if offset == 0:
            actions.append({'slot': slot, 'offset': 0, 'status': 'empty'})
            continue
        require(offset >= 8 + 4*count, 'overlapping_header', 'Action overlaps offset table', offset)
        track_offset, tracks, frames, loop = unpack('<IIIi', data, offset, 'action header')
        require(0 < frames <= 1000000 and -1 <= loop < frames, 'invalid_frame_range', 'Invalid action frame/loop range', offset+8)
        scene_position = unpack('<4f', data, offset+16, 'scene position')
        scene_rotation = unpack('<4f', data, offset+32, 'scene rotation')
        finite(scene_position + scene_rotation, 'action scene reference')
        flags, events, floats = unpack('<3I', data, offset+48, 'action flags/pointers')
        region(data, track_offset, tracks*36, 'track table')
        total_tracks += tracks
        require(total_tracks <= MAX_TRACKS, 'resource_limit', 'Total track count exceeds local 4096-track policy')
        for pointer, label in ((events, 'events'), (floats, 'float tracks')):
            if pointer:
                region(data, pointer, 1, label)
        action = {'slot': slot, 'offset': offset, 'status': 'parsed', 'frame_count': frames,
                  'loop_frame': loop, 'track_table_offset': track_offset, 'flags_raw': flags,
                  'events_offset_unparsed': events, 'float_tracks_offset_unparsed': floats,
                  'scene_end_position_raw': scene_position, 'scene_end_rotation_raw': scene_rotation,
                  'header_raw_hex': region(data, offset, 60, 'action raw header').hex(), 'tracks': []}
        for ordinal in range(tracks):
            position = track_offset + 36*ordinal
            codec, kind, bone_type, bone_id, weight, size, buffer, *tail = unpack('<4BfII4fI', data, position, 'track')
            reference, extremes_offset = tail[:4], tail[4]
            finite([weight] + reference, 'track')
            require((size == 0) == (buffer == 0), 'invalid_buffer_pointer', 'Buffer pointer and size disagree', position+8)
            payload = region(data, buffer, size, 'track payload') if size else b''
            extremes = list(unpack('<8f', data, extremes_offset, 'scale/bias')) if extremes_offset else None
            if extremes:
                finite(extremes, 'scale/bias')
            track = {'ordinal': ordinal, 'record_offset': position, 'record_raw_hex': region(data, position, 36, 'raw track').hex(),
                     'codec': codec, 'codec_name': CODECS.get(codec, ('unknown',))[0], 'track_type': kind,
                     'path': PATHS.get(kind), 'bone_type_raw': bone_type, 'bone_id': bone_id, 'weight_raw': weight,
                     'buffer_offset': buffer, 'buffer_size': size, 'buffer_sha256': sha(payload),
                     'reference_xyzw_or_xyz': reference, 'extremes_offset': extremes_offset,
                     'extremes_scale_bias': extremes, 'decode_status': 'decoded'}
            if codec not in CODECS:
                track.update(decode_status='unknown_codec', keys=None)
            else:
                total_keys += size // CODECS[codec][1]
                require(total_keys <= MAX_KEYS, 'resource_limit', 'Total decoded keys exceed local 250000-key policy')
                track['keys'] = decode_buffer(codec, payload, extremes)
                if track['keys']:
                    require(track['keys'][-1]['frame'] <= frames, 'key_outside_action', 'Key exceeds action frame count', buffer)
            action['tracks'].append(track)
        actions.append(action)
    return {'format': 'LMT', 'version': version, 'pointer_width': 32, 'sha256': sha(data), 'size': len(data),
            'action_slot_count': count, 'populated_action_count': sum(a['status'] != 'empty' for a in actions),
            'actions': actions,
            'decoder_profile': DECODER_PROFILE, 'arm_evidence': ARM_EVIDENCE,
            'interpretation_source': f'https://github.com/PredatorCZ/RevilLib/tree/{REFERENCE_COMMIT}/src/mtf_lmt',
            'unknowns': ['Game-specific action names and playback rate', 'Event/float-track payload semantics',
                         'Runtime interpolation equivalence and controller/root-motion behavior']}


def raw_channel_keys(track, action, fps):
    """Own sign transport and endpoint clipping before quaternion normalization."""
    require(math.isfinite(fps) and fps > 0, 'invalid_fps', 'Explicit export FPS must be positive')
    require(track['decode_status'] == 'decoded', 'unknown_codec', 'Cannot export undecoded track')
    require(track['bone_type_raw'] == 0 and track['weight_raw'] == 1.0, 'unsupported_track_semantics', 'Only plain bone tracks with weight 1 are validated')
    require(track['path'] is not None, 'unknown_track_type', 'Unknown track transform type')
    dimension = 4 if track['path'].endswith('rotation') else 3
    keys = track['keys']
    end_frame = action['frame_count']-1
    require(end_frame >= 0, 'invalid_frame_range', 'Frame count must be positive')
    if track['codec'] in (1, 2):
        # 0xaea0e4 returns the track reference even when a payload exists.
        frames_values = [(0, list(track['reference_xyzw_or_xyz'][:dimension]))]
    else:
        require(bool(keys), 'missing_key_buffer', 'Non-reference codec requires a stored key buffer')
        require(all(len(key['value']) == dimension for key in keys), 'codec_channel_mismatch', 'Codec dimensions disagree with channel')
        require(keys[0]['frame'] == 0 and all(a['frame'] < b['frame'] for a,b in zip(keys,keys[1:])),
                'nonincreasing_frames', 'Stored key timeline must begin at zero and increase')
        require(keys[-1]['next_frame_delta'] == 0, 'unterminated_keys', 'Last key must have zero delta; a missing next key cannot be sampled')
        frames_values = []
        sign, previous_raw = 1, None
        for key in keys:
            raw = key['value']
            finite(raw, 'key')
            if dimension == 4:
                # Native dot is on original raw endpoints. Carry prior sign;
                # testing only the already-signed pair is wrong when dot == 0.
                if previous_raw is not None and sum(a*b for a,b in zip(previous_raw,raw)) < 0:
                    sign = -sign
                previous_raw = raw
                value = [sign*x for x in raw]
            else:
                value = list(raw)
            if key['frame'] > end_frame:
                prior_frame, prior_value = frames_values[-1]
                fraction = (end_frame-prior_frame)/(key['frame']-prior_frame)
                if prior_frame < end_frame:
                    frames_values.append((end_frame, [(1-fraction)*a+fraction*b for a,b in zip(prior_value,value)]))
                break
            frames_values.append((key['frame'], value))
    # 0x95aa90 bounds time to N-1. Zero-delta terminal keys hold. Retain a
    # clipped quaternion's raw magnitude until CUBICSPLINE is constructed.
    if frames_values[-1][0] < end_frame:
        frames_values.append((end_frame, list(frames_values[-1][1])))
    times = [float32(frame/fps) for frame,_ in frames_values]
    require(all(math.isfinite(t) for t in times) and all(a < b for a,b in zip(times,times[1:])),
            'nonincreasing_frames', 'Stored float32 output times must be finite and increase')
    return times, [value for _,value in frames_values]


def channel_keys(track, action, fps):
    """Key directions for inspection; interpolation belongs to channel_sampler."""
    times, raw = raw_channel_keys(track, action, fps)
    return times, [normalize(value) for value in raw] if track['path'].endswith('rotation') else raw


def sample_model_channel(track, action, frames):
    """Sample the observed model-channel subset in native frame coordinates.

    The owner of LMT interpretation also owns this point-sampling boundary.
    Codecs 1/2 preserve the raw reference, including a nonunit quaternion;
    codecs 4/5 use the existing clipped linear vector curve. No FPS, model
    transform, loop advancement, default component or joint binding is added.
    Other model rotation codecs require a separate reviewed sampler contract.
    """
    kind, codec = track['track_type'], track['codec']
    require((kind == 4 and codec in (1, 4, 5)) or (kind == 3 and codec == 2),
            'unsupported_model_sampler', 'Only observed model translation 1/4/5 and reference rotation 2 are supported')
    require(track['path'] == PATHS[kind], 'model_channel_mismatch', 'Track type and parsed path disagree')
    require(len(frames) <= MAX_KEYS, 'resource_limit', 'Too many requested model samples')
    end = action['frame_count'] - 1
    require(all(type(f) in (int, float) and math.isfinite(f) and 0 <= f <= end for f in frames),
            'invalid_sample_frame', 'Model samples must be finite and inside [0, frame_count-1]')
    # All source key frames are bounded integers <=1e6, exactly representable
    # by raw_channel_keys' float32 time boundary when the divisor is one.
    times, values = raw_channel_keys(track, action, 1)
    sampled = []
    for frame in frames:
        left = bisect_right(times, frame) - 1
        if left == len(times)-1 or times[left] == frame:
            sampled.append(list(values[left]))
        else:
            fraction = (frame-times[left])/(times[left+1]-times[left])
            sampled.append([(1-fraction)*a+fraction*b for a,b in zip(values[left], values[left+1])])
    return sampled


def channel_sampler(track, action, fps):
    times, raw = raw_channel_keys(track, action, fps)
    if not track['path'].endswith('rotation'):
        return times, raw, 'LINEAR'
    values = [normalize(value) for value in raw]
    if track['codec'] in (1, 2) or len(times) == 1:
        return times, values, 'LINEAR'
    require(track['codec'] in QUATERNION_CODECS, 'unsupported_rotation_sampler', 'Rotation codec has no validated interpolation contract')
    # Hermite represents P(u)=lerp(1/|A|,1/|B|,u)*lerp(A,B,u).
    # Its positive scalar factor disappears on glTF quaternion normalization.
    # This preserves raw nlerp, with unit key values required by glTF. Tangents
    # use seconds and the actual stored float32 times, not frame deltas.
    incoming = [[0.0]*4 for _ in values]
    outgoing = [[0.0]*4 for _ in values]
    for i in range(len(times)-1):
        a,b = raw[i],raw[i+1]
        ka,kb = (1/math.sqrt(sum(x*x for x in q)) for q in (a,b))
        d, duration = kb-ka, times[i+1]-times[i]
        outgoing[i] = [(d*x+ka*(y-x))/duration for x,y in zip(a,b)]
        incoming[i+1] = [(d*y+kb*(y-x))/duration for x,y in zip(a,b)]
        finite(outgoing[i]+incoming[i+1], 'cubic tangents')
    rows = [row for i in range(len(values)) for row in (incoming[i],values[i],outgoing[i])]
    return times, rows, 'CUBICSPLINE'


@dataclass(frozen=True)
class ModelCurveBinding:
    """Explicit sidecar identity supplied by the paired-package producer.

    This is not a request to discard model channels. The producer must verify
    the sidecar through its owning format, then bind every model track ordinal.
    """
    source_lmt_sha256: str
    source_slot: int
    entry_id: int
    curve_index: int
    uri: str
    sha256: str
    track_ordinals: tuple


def _prepare_action(parsed, slot, binding, fps, model_curve=None):
    """Own joint/model classification, retaining the legacy export profile."""
    require(0 <= slot < len(parsed['actions']), 'invalid_slot', 'Slot does not exist')
    action = parsed['actions'][slot]
    require(action['status'] == 'parsed', 'empty_slot', 'Selected action is empty')
    require(action['flags_raw'] == 0x800000 and action['float_tracks_offset_unparsed'] == 0,
            'unsupported_action_semantics', 'Only observed event-bearing actions without float tracks are exportable')
    if model_curve is not None:
        require(isinstance(model_curve, ModelCurveBinding), 'invalid_model_curve_binding', 'Expected an explicit ModelCurveBinding')
        require(model_curve.source_lmt_sha256 == parsed['sha256'] and model_curve.source_slot == slot,
                'model_curve_identity_mismatch', 'External model curve belongs to another LMT or action')
        require(type(model_curve.entry_id) is int and model_curve.entry_id > 0 and
                type(model_curve.curve_index) is int and model_curve.curve_index >= 0 and
                isinstance(model_curve.uri, str) and bool(model_curve.uri) and
                isinstance(model_curve.sha256, str) and len(model_curve.sha256) == 64 and
                all(c in '0123456789abcdef' for c in model_curve.sha256),
                'invalid_model_curve_binding', 'External model curve identity is incomplete')
        expected = tuple(t['ordinal'] for t in action['tracks'] if t['track_type'] in (3, 4))
        require(bool(expected) and model_curve.track_ordinals == expected,
                'model_curve_track_mismatch', 'Every type 3/4 track must be bound exactly once in source order')
    prepared, omitted, used, separated = [], [], set(), []
    model_types = set()
    for track in action['tracks']:
        if model_curve is not None and track['track_type'] in (3, 4):
            require(track['track_type'] not in model_types, 'duplicate_model_channel', 'Repeated model component has no paired blending contract')
            model_types.add(track['track_type'])
            sample_model_channel(track, action, [0, action['frame_count']-1])
            separated.append({'track': track['ordinal'], 'track_type': track['track_type'],
                              'bone_id_raw': track['bone_id'], 'codec': track['codec'],
                              'component': 'rotation' if track['track_type'] == 3 else 'translation'})
            continue
        times, values, interpolation = channel_sampler(track, action, fps)
        if model_curve is None and track['bone_id'] == 255:
            require(track['path'] == 'absolute_translation' and all(x == 0.0 for row in values for x in row),
                    'unsupported_scene_motion', 'Nonzero/unrecognized scene-root track requires an explicit scene binding')
            omitted.append({'track': track['ordinal'], 'reason': 'scene-root translation is identically zero'})
            continue
        require(track['path'] in ('translation', 'rotation', 'scale'), 'absolute_track', 'Absolute tracks require conversion, not direct local glTF binding')
        require(track['bone_id'] in binding, 'missing_bone', f"Bone ID {track['bone_id']} is absent from model")
        target = binding[track['bone_id']]
        pair = (target, track['path'])
        require(pair not in used, 'duplicate_channel', 'Repeated target component needs game-specific blending')
        used.add(pair)
        prepared.append((track, target, times, values, interpolation))
    return action, prepared, omitted, separated


def prepare_action(parsed, slot, binding, fps):
    """Legacy joint-only export: nonzero/unrecognized scene tracks still fail."""
    action, prepared, omitted, _ = _prepare_action(parsed, slot, binding, fps)
    return action, prepared, omitted


def export_gltf(lmt_path, parsed, slot, model_path, fps, output, selections=None, *, model_curve_bindings=None):
    require(math.isfinite(fps) and fps > 0, 'invalid_fps', 'Explicit export FPS must be positive')
    many = selections is not None
    selections = selections if many else [(lmt_path, parsed, slot)]
    if model_curve_bindings is not None:
        require(len(model_curve_bindings) == len(selections), 'model_curve_count_mismatch', 'Each selected action needs a model-curve binding')
    model = json.loads(read(model_path))
    require(model.get('asset', {}).get('version') == '2.0', 'invalid_model', 'Expected glTF 2.0')
    gltf = copy.deepcopy(model)
    require(not gltf.get('animations'), 'existing_animations', 'Select a rest-pose glTF for this probe')
    binding = {}
    for i, node in enumerate(gltf.get('nodes', [])):
        bone_id = node.get('extras', {}).get('sourceFunctionIdCandidate')
        if bone_id is not None:
            require(type(bone_id) is int and bone_id not in binding, 'ambiguous_bone', 'Duplicate or invalid sourceFunctionIdCandidate')
            require('matrix' not in node, 'matrix_node', 'Animated joints must use decomposed TRS')
            binding[bone_id] = i
    require(bool(binding), 'missing_bone_mapping', 'Model nodes must expose extras.sourceFunctionIdCandidate')
    prepared_actions, rejected = [], []
    for selection_index, (selected_path, selected_parsed, selected_slot) in enumerate(selections):
        try:
            model_curve = model_curve_bindings[selection_index] if model_curve_bindings is not None else None
            if model_curve_bindings is not None:
                require(model_curve is not None, 'invalid_model_curve_binding', 'Paired actions cannot fall back to legacy omission')
            action, prepared, omitted, separated = _prepare_action(selected_parsed, selected_slot, binding, fps, model_curve)
            prepared_actions.append((selected_path, selected_parsed, action, prepared, omitted, separated, model_curve))
        except ProbeError as error:
            if not many or model_curve_bindings is not None:
                raise
            rejected.append({'source_lmt': str(selected_path.resolve()), 'sha256': selected_parsed['sha256'],
                             'slot': selected_slot, 'code': error.code, 'reason': str(error)})
    require(bool(prepared_actions), 'no_supported_actions', 'No selected action can be exported safely')
    output.mkdir(parents=True, exist_ok=True)
    dependency_records = []
    # Copy local external dependencies under deterministic names. No URL fetch.
    for category in ('buffers', 'images'):
        for index, item in enumerate(gltf.get(category, [])):
            uri = item.get('uri')
            if not uri or uri.startswith('data:'):
                continue
            parsed_uri = urlparse(uri)
            require(not parsed_uri.scheme and not parsed_uri.netloc and not parsed_uri.query and not parsed_uri.fragment,
                    'external_dependency', 'Only local glTF dependency paths are supported')
            dependency = (model_path.parent / unquote(uri)).resolve()
            data = read(dependency)
            name = f'model-{category}-{index}{dependency.suffix}'
            dependency_records.append(save(output/name, data))
            item['uri'] = name
    binary = bytearray()
    buffer_index = len(gltf.setdefault('buffers', []))
    views, accessors = gltf.setdefault('bufferViews', []), gltf.setdefault('accessors', [])

    def accessor(rows, kind):
        while len(binary) % 4:
            binary.append(0)
        offset = len(binary)
        flat = [value for row in rows for value in row]
        packed = struct.pack('<' + str(len(flat)) + 'f', *flat)
        binary.extend(packed)
        view_index = len(views)
        views.append({'buffer': buffer_index, 'byteOffset': offset, 'byteLength': len(binary)-offset})
        result = {'bufferView': view_index, 'componentType': 5126, 'count': len(rows), 'type': kind}
        if kind == 'SCALAR':
            stored = struct.unpack('<' + str(len(flat)) + 'f', packed)
            result.update(min=[min(stored)], max=[max(stored)])
        accessors.append(result)
        return len(accessors)-1

    animations, action_reports = [], []
    for selected_path, selected_parsed, action, prepared, omitted, separated, model_curve in prepared_actions:
        selected_slot = action['slot']
        animation = {'name': f'{selected_path.stem} / ação {selected_slot:03d}', 'samplers': [], 'channels': [],
                     'extras': {'source_lmt_sha256': selected_parsed['sha256'], 'source_slot': selected_slot,
                                'source_frame_count': action['frame_count'], 'source_loop_frame': action['loop_frame'],
                                'source_frame_max': action['frame_count']-1, 'decoder_profile': DECODER_PROFILE,
                                'export_fps': fps, 'fps_status': 'explicit conversion convention; game rate unverified',
                                'events_imported': False, 'game_runtime_validated': False}}
        channel_sources = []
        for track, target, times, values, interpolation in prepared:
            source_accessor = accessor([[time] for time in times], 'SCALAR')
            value_accessor = accessor(values, 'VEC4' if track['path'] == 'rotation' else 'VEC3')
            sampler = len(animation['samplers'])
            animation['samplers'].append({'input': source_accessor, 'output': value_accessor, 'interpolation': interpolation})
            animation['channels'].append({'sampler': sampler, 'target': {'node': target, 'path': track['path']}})
            channel_sources.append({'track': track['ordinal'], 'bone_id': track['bone_id'], 'node': target,
                                    'path': track['path'], 'codec': track['codec'], 'keys': len(times),
                                    'source_buffer_keys': len(track['keys']), 'interpolation': interpolation,
                                    'endpoint_policy': 'Sample/hold frame_count-1; raw tail retained in decoded source',
                                    'first_seconds': times[0], 'last_seconds': times[-1]})
        animation['extras']['omitted_identity_tracks'] = omitted
        if model_curve is not None:
            animation['extras']['model_channels_external'] = {
                'uri': model_curve.uri, 'sha256': model_curve.sha256, 'curve_index': model_curve.curve_index,
                'entry_id': model_curve.entry_id, 'source_lmt_sha256': model_curve.source_lmt_sha256,
                'source_slot': model_curve.source_slot, 'tracks': separated,
                'applied_to_gltf': False, 'runtime_defaults_applied': False}
        animations.append(animation)
        action_reports.append({'name': animation['name'], 'source_lmt': str(selected_path.resolve()),
                               'sha256': selected_parsed['sha256'], 'slot': selected_slot,
                               'frame_count': action['frame_count'], 'loop_frame': action['loop_frame'],
                               'channels': channel_sources, 'omitted': omitted})
        if model_curve is not None:
            action_reports[-1]['model_channels_external'] = animation['extras']['model_channels_external']
    gltf['buffers'].append({'uri': 'animation.bin', 'byteLength': len(binary)})
    gltf['animations'] = animations
    animation_buffer = save(output/'animation.bin', binary)
    artifact = save(output/'animated.gltf', json.dumps(gltf, ensure_ascii=False, indent=2)+'\n')
    report = {'producer': 'ordinary local parser/exporter; not REA Evidence', 'artifact': artifact,
              'animation_buffer': animation_buffer,
              'decoder_profile': DECODER_PROFILE, 'producer_sha256': sha(Path(__file__).read_bytes()),
              'arm_evidence': ARM_EVIDENCE,
              'quaternion_static_audit': {'document': 'docs/quaternion-codecs.md',
                'source_manifest': 'research/private/quaternion-codecs/source-manifest.json',
                'source_manifest_sha256': '5381a2f545dafaca890b54392e23af3c24fe69d810ca9be1a0ab6f1452ad0627'},
              'source_lmt': {'path': str(lmt_path.resolve()), 'sha256': parsed['sha256']},
              'source_gltf': {'path': str(model_path.resolve()), 'sha256': sha(read(model_path))},
              'dependencies': dependency_records, 'channels': channel_sources, 'omitted': omitted,
              'joint_binding': {'source_field': 'nodes[].extras.sourceFunctionIdCandidate',
                                'evidence': 'Each non-scene LMT bone ID resolves uniquely to the MOD-derived function-ID field'},
              'frame_count': action['frame_count'], 'export_fps': fps, 'runtime_validated': False,
              'limitations': ['60 FPS or other supplied rate is a conversion convention',
                              'Action name, original timing and gameplay events unverified',
                              'Quaternion codecs 6/7/11/12/13 and raw nlerp derived from shipped ARM; target not executed',
                              'CUBICSPLINE preserves the derived mathematical rotation curve; float32 serialization/runtime round differently',
                              'Exact float32 quantization literals with Python double arithmetic; VFP rounding/environment not emulated']}
    if many:
        del report['source_lmt'], report['channels'], report['omitted'], report['frame_count']
        report.update(requested_actions=len(selections), exported_actions=len(animations),
                      actions=action_reports, rejected_actions=rejected)
    if model_curve_bindings is not None:
        report['export_profile'] = 'paired-joints-with-external-model-curves-v1'
        report['limitations'].append('Joint clips require their paired model curves; this glTF does not apply model motion or select runtime defaults')
    save(output/'animation-provenance.json', json.dumps(report, indent=2)+'\n')
    return report


def inspect_index(index, entries, output):
    from index_resources import open_index, extract
    connection, meta = open_index(index)
    output.mkdir(parents=True, exist_ok=True)
    try:
        rows = [dict(row) for row in connection.execute('SELECT e.id,e.name,e.type_id,e.uncompressed_size,a.path archive FROM entries e JOIN archives a ON a.id=e.archive_id WHERE e.type_id=? ORDER BY e.id', (MOTION_TYPE,))]
        save(output/'motion-catalog.json', json.dumps({'source': meta['source'], 'type_id': MOTION_TYPE,
             'matching_members': len(rows), 'coverage': 'complete indexed ARC directory, payloads decoded only for selected IDs', 'entries': rows}, indent=2)+'\n')
        summaries = []
        for entry in entries:
            require(any(row['id'] == entry for row in rows), 'not_motion_entry', 'Selected ID is not a motion-list resource')
            path = output/f'entry-{entry}.lmt'
            provenance = extract(connection, meta, entry, path)
            parsed = parse_lmt(read(path))
            save(output/f'entry-{entry}.provenance.json', json.dumps(provenance, indent=2)+'\n')
            save(output/f'entry-{entry}.decoded.json', json.dumps(parsed, indent=2)+'\n')
            summaries.append({'id': entry, 'sha256': parsed['sha256'], 'version': parsed['version'],
                              'actions': parsed['populated_action_count'], 'file': str(path.resolve())})
        return {'motion_members_catalogued': len(rows), 'selected': summaries}
    finally:
        connection.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    inspect = commands.add_parser('inspect')
    inspect.add_argument('--index', type=Path, required=True)
    inspect.add_argument('--entry', type=int, action='append', required=True)
    inspect.add_argument('--output', type=Path, required=True)
    export = commands.add_parser('export')
    export.add_argument('--lmt', type=Path, required=True)
    export.add_argument('--model', type=Path, required=True)
    export.add_argument('--slot', type=int, required=True)
    export.add_argument('--fps', type=float, required=True)
    export.add_argument('--output', type=Path, required=True)
    many = commands.add_parser('export-many')
    many.add_argument('--lmt', type=Path, action='append', required=True)
    many.add_argument('--model', type=Path, required=True)
    many.add_argument('--fps', type=float, required=True)
    many.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == 'inspect':
            result = inspect_index(args.index, args.entry, args.output)
        elif args.command == 'export':
            report = export_gltf(args.lmt, parse_lmt(read(args.lmt)), args.slot, args.model, args.fps, args.output)
            result = {'artifact': report['artifact'], 'channel_count': len(report['channels']), 'frame_count': report['frame_count']}
        else:
            selections = []
            for path in args.lmt:
                parsed = parse_lmt(read(path))
                selections.extend((path, parsed, action['slot']) for action in parsed['actions'] if action['status'] == 'parsed')
            require(bool(selections), 'no_actions', 'Inputs contain no populated action slots')
            path, parsed, slot = selections[0]
            report = export_gltf(path, parsed, slot, args.model, args.fps, args.output, selections)
            result = {key: report[key] for key in ('artifact', 'requested_actions', 'exported_actions', 'rejected_actions')}
        print(json.dumps({'ok': True, **result}, ensure_ascii=False, indent=2))
        return 0
    except (ProbeError, ValueError, OSError) as error:
        print(json.dumps({'ok': False, 'error': {'code': getattr(error, 'code', 'input_error'),
              'message': str(error), 'offset': getattr(error, 'offset', None)}}))
        return 2


if __name__ == '__main__':
    sys.exit(main())
