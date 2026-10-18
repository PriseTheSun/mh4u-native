#!/usr/bin/env python3
"""Exercise the collision CLI as a process using a synthetic glTF and optional local SBC export."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile
import time


def require(value, reason):
    if not value:
        raise RuntimeError(reason)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--scene', type=Path)
    parser.add_argument('--geometry', type=Path, help='Matching collision_probe geometry.json for independent source coordinates')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if bool(args.scene) != bool(args.geometry):
        parser.error('--scene and --geometry must be supplied together')
    binary = args.binary.resolve()
    records = []

    def query(scene, options, success=True):
        command = [str(binary), '--scene', str(scene), *map(str, options)]
        before = time.perf_counter()
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        elapsed = time.perf_counter()-before
        require(result.returncode == (0 if success else 1), f'Unexpected exit {result.returncode}: {result.stderr}')
        require(not (result.stderr if success else result.stdout), 'Unexpected mixed output streams')
        decoded = json.loads(result.stdout if success else result.stderr)
        require(success or isinstance(decoded.get('error'), str), 'Missing structured CLI error')
        records.append({'options': options, 'success': success, 'elapsed_seconds': elapsed, 'result': decoded})
        return decoded

    with tempfile.TemporaryDirectory(prefix='mh4u-collision-cli-') as directory:
        root = Path(directory)
        values = (0,0,0, 1,0,0, 0,1,0, 0,0,1, 0,0,1, 0,0,1)
        (root/'mesh.bin').write_bytes(struct.pack('<18f', *values))
        document = {'asset': {'version': '2.0'},
                    'buffers': [{'uri': 'mesh.bin', 'byteLength': 72}],
                    'bufferViews': [{'buffer': 0, 'byteLength': 36}, {'buffer': 0, 'byteOffset': 36, 'byteLength': 36}],
                    'accessors': [{'bufferView': i, 'componentType': 5126, 'type': 'VEC3', 'count': 3} for i in range(2)],
                    'meshes': [{'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1}}]}],
                    'nodes': [{'mesh': 0}], 'scenes': [{'nodes': [0]}], 'scene': 0}
        scene = root/'scene.gltf'; scene.write_text(json.dumps(document))
        ray = ['--origin', .25, .25, 2, '--direction', 0, 0, -10, '--max-distance', 3]
        result = query(scene, ray)
        require(result['hit']['distance'] == 2 and result['hit']['triangle_id'] == 0, 'CLI ray result differs from analytic plane')
        require(result['hit']['normal'] == [0,0,1], 'CLI winding normal')
        result = query(scene, ['--origin', .25, .25, 2, '--end', .25, .25, 0])
        require(result['mode'] == 'segment' and result['hit']['distance'] == 2, 'CLI segment endpoint')
        result = query(scene, ['--origin', 2, 2, 2, '--direction', 0, 0, -1, '--max-distance', 3])
        require(result['hit'] is None, 'CLI miss must be null and exit successfully')
        result = query(scene, ['--origin', .25, .25, -2, '--direction', 0, 0, 1, '--max-distance', 3, '--backfaces', 'cull'])
        require(result['hit'] is None, 'CLI backface policy lost')
        for options in (
                ['--origin', 0, 0, 1, '--direction', 0, 0, 0, '--max-distance', 1],
                ['--origin', 'nan', 0, 1, '--direction', 0, 0, 1, '--max-distance', 1],
                ['--origin', 0, 0, 1, '--direction', 0, 0, 1, '--max-distance', -1],
                [*ray, '--max-distance', 1], [*ray, '--end', 0, 0, 1], [*ray, '--backfaces', 'unknown']):
            query(scene, options, False)
        # Empty animation still constitutes animation data: must not silently freeze it.
        document['animations'] = [{'channels': [], 'samplers': []}]
        scene.write_text(json.dumps(document))
        rejected = query(scene, ray, False)
        require('animation/skin' in rejected['error'], 'Animated scene rejection occurred at unexpected boundary')
    if args.scene:
        core = json.loads(args.geometry.read_text())
        face = core['faces'][0]
        points = [core['vertices_xyzw'][i][:3] for i in face['vertices']]
        center = [sum(v[k] for v in points)/3 for k in range(3)]
        result = query(args.scene.resolve(), ['--origin', center[0], center[1]+10, center[2], '--direction', 0, -1, 0, '--max-distance', 20])
        require(result['hit']['triangle_id'] == 0 and math.isclose(result['hit']['distance'], 10, abs_tol=1e-8), 'Real CLI hit disagrees with exported source face')
    report = {'schema': 'mh4u-collision-cli-smoke-v1', 'binary': str(binary),
              'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(), 'queries': records,
              'timing_scope': 'Process startup, file loading, BVH build and one query per process; no isolated-kernel benchmark'}
    if args.output:
        with args.output.open('x') as handle:
            json.dump(report, handle, indent=2); handle.write('\n')
    print(json.dumps({'queries': len(records), 'passed': True, 'real_asset_checked': bool(args.scene)}))


if __name__ == '__main__':
    main()
