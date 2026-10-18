#!/usr/bin/env python3
"""Public viewer boundary checks; optional short-lived native render captures."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--viewer', type=Path, required=True)
    parser.add_argument('--action-cli', type=Path, required=True)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--paired-root', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--render', action='store_true')
    args = parser.parse_args()
    if args.output is None:
        with tempfile.TemporaryDirectory(prefix='action-viewer-smoke-') as temporary:
            args.output = Path(temporary) / 'validation'
            execute(args)
    else:
        execute(args)


def execute(args):
    for name in ('viewer', 'action_cli', 'package', 'paired_root', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    args.output.mkdir(parents=True, exist_ok=False)
    report = {'schema': 'mh4u-action-viewer-smoke-v1', 'cases': [], 'negative': [], 'captures': [],
              'viewer_sha256': hashlib.sha256(args.viewer.read_bytes()).hexdigest(),
              'scope': 'Public composition against validated action CLI and standalone glTF evaluator; not original gameplay.'}
    number = 0

    def run(label, command, success=True):
        nonlocal number
        number += 1
        result = subprocess.run([str(x) for x in command], capture_output=True, text=True, timeout=30)
        (args.output / f'{number:03d}-{label}.log').write_text(result.stdout + result.stderr)
        if success:
            assert result.returncode == 0, (label, result.stderr)
        else:
            assert result.returncode != 0, (label, 'invalid command succeeded')
            assert 'OPENGL ' not in result.stdout and 'MOTION_STATE' not in result.stdout, label
            report['negative'].append({'case': label, 'exit': result.returncode, 'reason': result.stderr.strip()})
        return result.stdout

    def line(text, prefix):
        rows = [x for x in text.splitlines() if x.startswith(prefix)]
        assert rows, prefix
        return rows[-1]

    def base(index=0, package=None, root=None):
        return [args.viewer, '--motion-package', package or args.package, '--paired-root', root or args.paired_root,
                '--motion-action', str(index)]

    manifest = json.loads((args.paired_root / 'manifest.json').read_text())
    gltf = args.paired_root / manifest['joint_gltf']
    assert len(manifest['pairs']) == 7
    different_clock = 0
    movement = 0
    for index, pair in enumerate(manifest['pairs']):
        for ticks, repeat in [(0, False), (2, False), (pair['source_frame_count'] + 3, False), (pair['source_frame_count'] + 3, True)]:
            label = f'action{index}-ticks{ticks}-repeat{int(repeat)}'
            reference = run(label + '-numeric', [args.action_cli, '--package', args.package, '--paired-root', args.paired_root,
                            '--action', str(index), '--ticks', str(ticks)] + (['--repeat'] if repeat else []))
            inspected = run(label, base(index) + ['--motion-ticks', str(ticks), '--inspect-only'] + (['--motion-repeat'] if repeat else []))
            state = line(inspected, 'MOTION_STATE\t').split('\t')[1:]
            expected = line(reference, 'TICK\t').split('\t')[1:]
            assert state == expected, (label, state, expected)
            source_frame = float(state[1])
            different_clock += state[1] != state[2]
            position = [float(x) for x in state[6:9]]
            q = [float(x) for x in state[9:13]]
            movement += any(abs(x) > 1e-6 for x in position)
            # Check the presentation matrix directly from API state, without
            # normalizing its raw quaternion or feeding it back into Player.
            x, y, z, w = q
            expected_matrix = [1-2*(y*y+z*z), 2*(x*y+z*w), 2*(x*z-y*w), 0,
                               2*(x*y-z*w), 1-2*(x*x+z*z), 2*(y*z+x*w), 0,
                               2*(x*z+y*w), 2*(y*z-x*w), 1-2*(x*x+y*y), 0, *position, 1]
            matrix = [float(x) for x in line(inspected, 'MOTION_MODEL_MATRIX\t').split('\t')[1:]]
            assert len(matrix) == 16 and all(math.isclose(a, b, abs_tol=2e-6, rel_tol=2e-6) for a, b in zip(matrix, expected_matrix)), label
            plain = run(label + '-joints', [args.viewer, '--model', gltf, '--animation', str(pair['joint_clip_index']),
                        '--time', str(source_frame), '--no-loop', '--inspect-only'])
            assert line(inspected, 'BOUNDS ') == line(plain, 'BOUNDS '), (label, 'joint frame mismatch')
            report['cases'].append({'action': index, 'ticks': ticks, 'repeat': repeat, 'joint_source_frame': source_frame,
                                    'advanced_clock': float(state[2]), 'position': position, 'raw_rotation': q,
                                    'bounds': line(inspected, 'BOUNDS '), 'render_bounds': line(inspected, 'MOTION_RENDER_BOUNDS\t')})
    assert different_clock >= 14 and movement >= 14, (different_clock, movement)

    invalid = [
        ('missing-root', [args.viewer, '--motion-package', args.package, '--motion-action', '0']),
        ('missing-action', [args.viewer, '--motion-package', args.package, '--paired-root', args.paired_root]),
        ('missing-package', [args.viewer, '--paired-root', args.paired_root, '--motion-action', '0']),
        ('action-range', base(7)), ('action-negative', base(-1)),
        ('ticks-negative', base() + ['--motion-ticks', '-1']),
        ('ticks-budget', base() + ['--motion-ticks', '100001']),
        ('ticks-text', base() + ['--motion-ticks', 'NaN']),
        ('mixed-world', base() + ['--world', 'unread-world.tsv']),
        ('mixed-network', base() + ['--connect-port', '8799']),
        ('mixed-model', base() + ['--model', gltf]),
        ('mixed-catalog', base() + ['--catalog', 'unread.tsv']),
        ('mixed-animation', base() + ['--animation', '0']),
        ('mixed-time', base() + ['--time', '0']),
        ('mixed-loop', base() + ['--no-loop']),
        ('capture-without-ticks', base() + ['--frames', '1']),
        ('duplicate-action', base() + ['--motion-action', '0']),
        ('duplicate-ticks', base() + ['--motion-ticks', '0', '--motion-ticks', '0']),
        ('duplicate-root', base() + ['--paired-root', args.paired_root]),
    ]
    for label, command in invalid:
        run(label, command, False)
    # Mutate only newly owned temporary copies; original package/assets stay intact.
    with tempfile.TemporaryDirectory(prefix='action-viewer-', dir=args.output) as temporary:
        temporary = Path(temporary)
        bad_package = temporary / 'changed.bin'
        payload = bytearray(args.package.read_bytes()); payload[-1] ^= 1; bad_package.write_bytes(payload)
        run('changed-package', base(package=bad_package) + ['--inspect-only'], False)
        for target in ['joints/animated.gltf', 'joints/model-buffers-0.bin', 'joints/model-images-0.png']:
            copied = temporary / 'paired'; shutil.copytree(args.paired_root, copied)
            file = copied / target
            changed = bytearray(file.read_bytes()); changed[-1] ^= 1; file.write_bytes(changed)
            run('changed-' + file.suffix[1:], base(root=copied) + ['--inspect-only'], False)
            shutil.rmtree(copied)

    if args.render:
        # Distinct native processes own/close only their own bounded windows.
        for index, ticks, repeat in [(0, 0, False), (0, 16, False), (3, 24, False), (1, 280, True)]:
            label = f'render-action{index}-ticks{ticks}'
            image = args.output / f'{label}.ppm'
            output = run(label, base(index) + ['--motion-ticks', str(ticks), '--frames', '2', '--hide-primitive', '3',
                         '--screenshot', image] + (['--motion-repeat'] if repeat else []))
            assert image.is_file() and image.stat().st_size > 10000 and 'FRAMES 2' in output
            states = [x for x in output.splitlines() if x.startswith('MOTION_STATE\t')]
            assert len(states) == 2 and states[0] == states[1], 'capture advanced frozen state'
            report['captures'].append({'action': index, 'ticks': ticks, 'repeat': repeat, 'file': image.name,
                                       'sha256': hashlib.sha256(image.read_bytes()).hexdigest(), 'hidden_primitives': [3]})
    report['passed'] = True
    report['different_joint_and_advanced_clock_cases'] = different_clock
    report['nonzero_model_translation_cases'] = movement
    report['processes'] = number
    (args.output / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'passed': True, 'cases': len(report['cases']), 'negative': len(report['negative']),
                      'captures': len(report['captures']), 'processes': number}))


if __name__ == '__main__':
    main()
