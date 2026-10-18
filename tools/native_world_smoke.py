#!/usr/bin/env python3
"""Verify native world identity through the public CLI, using isolated copies."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from network_world_identity import identify


def run(binary, recipe, output):
    source = identify(recipe)
    records = []
    with tempfile.TemporaryDirectory(prefix='mh4u-native-world-') as temporary:
        root = Path(temporary)
        for name in source['files']:
            target = root/name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(recipe.parent/name, target)
        world = root/recipe.name
        fields = dict(line.split('\t') for line in world.read_text().splitlines()[1:])
        actor = root/fields['actor']
        assert actor.suffix == '.mhnative'
        recipe_bytes = actor.read_bytes()
        rows = [line.split('\t') for line in recipe_bytes.decode().splitlines()[1:]]

        def verify(name, expected=None, error=None):
            result = subprocess.run([str(binary), 'verify', '--world', str(world)],
                                    capture_output=True, text=True, timeout=20)
            records.append({'name': name, 'exit_code': result.returncode,
                            'stdout': result.stdout, 'stderr': result.stderr})
            if error:
                assert result.returncode != 0 and error in result.stderr, (name, result)
            else:
                assert result.returncode == 0 and 'WORLD verified='+expected in result.stdout, (name, result)

        verify('native-inventory', source['digest'])
        for kind in ('model', 'motion', 'texture', 'commands'):
            row = next(row for row in rows if row[0] == kind)
            path = actor.parent/row[1]
            original = path.read_bytes()
            try:
                changed = bytearray(original)
                changed[-1] ^= 1
                path.write_bytes(changed)
                verify('tampered-'+kind, error='manifest: native actor:')
                path.unlink()
                verify('missing-'+kind, error='manifest: native actor:')
            finally:
                path.write_bytes(original)

        command = next(row for row in rows if row[0] == 'commands')
        command_path = actor.parent/command[1]
        original = command_path.read_bytes()
        try:
            changed = bytearray(original)
            changed[0] ^= 1
            command_path.write_bytes(changed)
            changed_hash = hashlib.sha256(changed).hexdigest()
            actor.write_bytes(recipe_bytes.replace(command[2].encode(), changed_hash.encode()))
            new_identity = identify(world)
            assert new_identity['digest'] != source['digest']
            verify('valid-different-command-snapshot', new_identity['digest'])
        finally:
            command_path.write_bytes(original)
            actor.write_bytes(recipe_bytes)

        model = next(row for row in rows if row[0] == 'model')
        variants = [
            ('path-escape', recipe_bytes.replace(('model\t'+model[1]).encode(),
                ('model\t../'+model[1]).encode(), 1)),
            ('duplicate-commands', recipe_bytes+('\t'.join(command)+'\n').encode()),
            ('unknown-native-record', recipe_bytes+b'extra\tvalue\n'),
        ]
        for name, changed in variants:
            try:
                actor.write_bytes(changed)
                verify(name, error='manifest: native actor:')
            finally:
                actor.write_bytes(recipe_bytes)
        verify('restored-identity', source['digest'])
    report = {'status': 'passed', 'cases': len(records), 'world_digest': source['digest'],
              'files': len(source['files']), 'records': records}
    if output:
        output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k: report[k] for k in ('status', 'cases', 'world_digest', 'files')}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--world', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    run(args.binary.resolve(), args.world.resolve(), args.output)
