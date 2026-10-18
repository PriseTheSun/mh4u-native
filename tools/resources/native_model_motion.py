#!/usr/bin/env python3
"""Serialize owner-decoded model keys as bounded LE binary64, paired by identity."""
import argparse
import json
import math
from pathlib import Path, PurePosixPath
import sqlite3
import struct
import sys

import animation_probe as motion
import root_motion_curves as curves
import paired_actions as paired
from index_resources import open_index, extract

MAGIC = b'MH4UMC02'
SCHEMA = 'mh4u-native-model-motion-v2'


def local_file(root, uri):
    motion.require(isinstance(uri, str) and uri and len(uri) <= 1024 and
                   all(c.isascii() and (c.isalnum() or c in '/._-') for c in uri) and
                   not uri.startswith('/') and all(p not in ('', '.', '..') for p in uri.split('/')),
                   'invalid_binding_path', 'Binding path must be canonical relative ASCII')
    path = (root/uri).resolve()
    motion.require(path.is_relative_to(root.resolve()), 'invalid_binding_path', 'Binding escapes selected package root')
    return path


def serialized_action(action, pair):
    channels = [(1, action['channels']['translation']), (2, action['channels']['rotation'])]
    channels = [(component, channel) for component, channel in channels if channel['status']=='present']
    mask = sum(component for component, _ in channels)
    data = bytearray(struct.pack('<II', action['entry_id'], action['slot']))
    data.extend(bytes.fromhex(action['source_lmt_sha256']))
    data.extend(struct.pack('<IiIIII', action['source_frame_count'], action['source_loop_frame_raw'],
                            mask, len(channels), pair['joint_clip_index'], 0))
    for component, channel in channels:
        mode = 0 if channel['interpolation']=='constant_reference' else 1
        motion.require((component==1 and channel['interpolation'] in ('constant_reference','linear_vector')) or
                       (component==2 and channel['interpolation']=='constant_reference'),
                       'unsupported_sampler', 'Native model package supports observed vector and reference rotation profiles')
        frames, values = channel['key_frames'], channel['key_values']
        dimension = 3 if component==1 else 4
        motion.require(0 < len(frames) == len(values) <= motion.MAX_KEYS and
                       all(len(value)==dimension for value in values), 'invalid_keys', 'Invalid owner key dimensions/count')
        data.extend(struct.pack('<4I', component, mode, channel['track_ordinal'], len(frames)))
        for frame, value in zip(frames, values):
            motion.finite([frame]+value, 'native model key')
            data.extend(struct.pack('<'+'d'*(dimension+1), frame, *value))
    info = {'entry_id': action['entry_id'], 'slot': action['slot'], 'lmt_sha256': action['source_lmt_sha256'],
            'frame_count': action['source_frame_count'], 'loop_frame_raw': action['source_loop_frame_raw'],
            'presence_mask': mask, 'gltf_clip_index': pair['joint_clip_index']}
    return bytes(data), info


def identity_header(expected):
    def binding(value):
        return '{'+json.dumps(value['relative_path'])+','+json.dumps(value['sha256'])+'}'
    lines = ['#pragma once', '#include "model_motion.hpp"',
             '// Generated local identities only; no game key values are embedded.',
             'namespace mh4u::model_motion::generated {',
             'inline ExpectedPackage expected_package() {',
             '    return {'+json.dumps(expected['package_sha256'])+',',
             '        '+binding(expected['gltf'])+','+binding(expected['curves'])+',{']
    for a in expected['actions']:
        lines.append('        {{'+str(a['entry_id'])+','+str(a['slot'])+','+json.dumps(a['lmt_sha256'])+'},'+
            ','.join(str(a[k]) for k in ('frame_count','loop_frame_raw','presence_mask','gltf_clip_index'))+'},')
    lines += ['    }, {']
    lines += ['        '+binding(value)+',' for value in expected['dependencies']]
    lines += ['    }};', '}', '} // namespace mh4u::model_motion::generated', '']
    return '\n'.join(lines)


def build(manifest_path, expected_manifest_sha256, index, output, research):
    manifest_raw = motion.read(manifest_path)
    motion.require(motion.sha(manifest_raw)==expected_manifest_sha256, 'paired_hash_mismatch', 'Paired manifest differs from caller-pinned identity')
    manifest = json.loads(manifest_raw);root = manifest_path.parent.resolve()
    motion.require(manifest.get('schema')==paired.SCHEMA, 'unsupported_package', 'Expected paired-action v1 manifest')
    files = {}
    for item in manifest['files']:
        motion.require(item['uri'] not in files, 'duplicate_binding', 'Paired file occurs twice')
        data = motion.read(local_file(root, item['uri']))
        motion.require(len(data)==item['size'] and motion.sha(data)==item['sha256'], 'paired_file_mismatch', 'Paired file identity mismatch')
        files[item['uri']] = data
    gltf_uri, curves_uri = manifest['joint_gltf'], manifest['model_curves']
    motion.require(gltf_uri in files and curves_uri in files and gltf_uri != curves_uri, 'missing_binding', 'Missing/distinct joint glTF and model curves are required')
    gltf, package = json.loads(files[gltf_uri]), json.loads(files[curves_uri])
    motion.require(package.get('schema')==curves.SCHEMA and package.get('decoder_profile')==motion.DECODER_PROFILE,
                   'unsupported_package', 'Curve format/profile is unsupported')
    motion.require(0 < len(manifest['pairs']) == len(package['actions']) <= curves.MAX_SELECTIONS,
                   'invalid_pair_count', 'Paired action count is unsupported')
    motion.require(not output.exists() and not research.exists(), 'existing_output', 'Choose new output/research directories')
    output, research = output.resolve(), research.resolve()
    motion.require(output != research and output not in research.parents and research not in output.parents,
                   'overlapping_output', 'Output/research directories must be separate')
    research.mkdir(parents=True)
    connection, meta = open_index(index)
    parsed, extractions = {}, []
    try:
        for source in package['sources']:
            entry = source['entry_id'];path = research/f'entry-{entry}.lmt'
            extraction = extract(connection, meta, entry, path)
            motion.require(extraction['entry']['type_id']==motion.MOTION_TYPE and
                           extraction['entry']['name']==source['name'] and extraction['entry']['archive']==source['archive'] and
                           extraction['sha256']==source['sha256'], 'source_mismatch', 'Indexed source differs from paired identity')
            parsed[entry] = motion.parse_lmt(motion.read(path));extractions.append(extraction)
    finally:
        connection.close()
    expected = {'gltf': {'relative_path': gltf_uri, 'sha256': motion.sha(files[gltf_uri])},
                'curves': {'relative_path': curves_uri, 'sha256': motion.sha(files[curves_uri])}, 'actions': [],
                'dependencies': [{'relative_path':uri,'sha256':motion.sha(data)} for uri,data in sorted(files.items())
                                 if uri not in (gltf_uri,curves_uri,'joints/animation-provenance.json')]}
    body = bytearray()
    for binding in (expected['gltf'], expected['curves']):
        path = binding['relative_path'].encode('ascii')
        body.extend(struct.pack('<I', len(path))+path+bytes.fromhex(binding['sha256']))
    motion.require(len(expected['dependencies']) <= 62, 'resource_limit', 'Dependent bindings exceed native package limit')
    body.extend(struct.pack('<I',len(expected['dependencies'])))
    for binding in expected['dependencies']:
        path=binding['relative_path'].encode('ascii')
        body.extend(struct.pack('<I',len(path))+path+bytes.fromhex(binding['sha256']))
    oracle, seen = [], set()
    total_keys = 0
    for i, pair in enumerate(manifest['pairs']):
        motion.require(pair['pair_index']==i and pair['model_curve_index']==i,
                       'pair_mismatch', 'Paired curve ordering is inconsistent')
        action = package['actions'][i];source = parsed[action['entry_id']]
        paired.verify_curve_action(action, source)
        clip_index = pair['joint_clip_index']
        motion.require(type(clip_index) is int and 0 <= clip_index < len(gltf['animations']) and clip_index not in seen,
                       'pair_mismatch', 'Clip index is invalid/duplicated')
        seen.add(clip_index);clip = gltf['animations'][clip_index]
        motion.require(pair['source_lmt_sha256']==action['source_lmt_sha256']==clip['extras']['source_lmt_sha256'] and
                       pair['source_slot']==action['slot']==clip['extras']['source_slot'] and
                       pair['entry_id']==action['entry_id'] and pair['source_frame_count']==action['source_frame_count'],
                       'pair_mismatch', 'Clip/source action identity differs')
        data, info = serialized_action(action, pair);body.extend(data);expected['actions'].append(info)
        for channel in action['channels'].values():
            if channel['status']=='present':total_keys += len(channel['key_frames'])
        motion.require(total_keys <= motion.MAX_KEYS, 'resource_limit', 'Aggregate key count exceeds native producer limit')
        original = source['actions'][action['slot']]
        tracks = curves.model_tracks(original)
        frames = set(action['sample_frames'])
        for channel in action['channels'].values():
            if channel['status']=='present':
                for frame in channel['key_frames']:
                    for toward in (-math.inf, math.inf):
                        nearby = math.nextafter(frame, toward)
                        if 0 <= nearby <= action['frame_end']:frames.add(nearby)
        values = {component: motion.sample_model_channel(track, original, sorted(frames)) for component,track in tracks.items()}
        for n, frame in enumerate(sorted(frames)):
            fields=[str(i),format(frame,'.17g')]
            for component in ('translation','rotation'):
                if component in values:fields += ['1']+[format(v,'.17g') for v in values[component][n]]
                else:fields += ['0']
            oracle.append('\t'.join(fields))
    binary = MAGIC+struct.pack('<4I', 0x01020304, 24+len(body), len(expected['actions']), 0)+body
    motion.require(len(binary) <= 8*1024*1024, 'resource_limit', 'Native package exceeds byte limit')
    expected['package_sha256'] = motion.sha(binary)
    output.mkdir(parents=True)
    artifacts = {}
    for name, data in [('model-motion.bin',binary), ('model_motion_identity.hpp',identity_header(expected).encode()),
                       ('sample-oracle.tsv', ('\n'.join(oracle)+'\n').encode())]:
        with (output/name).open('xb') as handle:handle.write(data)
        artifacts[name] = {'sha256':motion.sha(data),'size':len(data)}
    report = {'schema':SCHEMA, 'byte_order':'little', 'numeric_encoding':'IEEE-754 binary64',
        'artifact_root':str(output), 'artifacts':artifacts, 'expected':expected,
        'paired_manifest':{'path':str(manifest_path.resolve()),'sha256':expected_manifest_sha256},
        'paired_asset_root':str(root), 'key_count':total_keys, 'oracle_queries':len(oracle),
        'original_half_frame_queries':sum(len(a['sample_frames']) for a in package['actions']),
        'binary64_serialization_rounding_error':0, 'double_to_float_conversion_performed':False,
        'source_extractions':extractions, 'producer_sha256':motion.sha(Path(__file__).read_bytes()),
        'sampler_owner_sha256':motion.sha(Path(motion.__file__).read_bytes()),
        'runtime_defaults_selected':False, 'model_application_performed':False}
    curves.write_new(output/'identity.json', expected)
    provenance = curves.write_new(research/'provenance.json', report)
    return {'ok':True,'binary':artifacts['model-motion.bin'],'header':str(output/'model_motion_identity.hpp'),
            'actions':len(expected['actions']),'keys':total_keys,'oracle_queries':len(oracle),'provenance':provenance}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--paired-manifest',type=Path,required=True);parser.add_argument('--paired-sha256',required=True)
    parser.add_argument('--index',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--research',type=Path,required=True);args=parser.parse_args()
    try:
        print(json.dumps(build(args.paired_manifest,args.paired_sha256,args.index,args.output,args.research)));return 0
    except (motion.ProbeError,ValueError,OSError,sqlite3.Error,KeyError,TypeError) as error:
        print(json.dumps({'ok':False,'error':{'code':getattr(error,'code','input_error'),'message':str(error)}}));return 2

if __name__=='__main__':sys.exit(main())
