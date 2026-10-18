#!/usr/bin/env python3
"""Independent Python inventory for the native diagnostic world identity.

This verifies content bytes, not movement/recipe semantics; WorldConfig and the
native geometry loader remain their owners. No files are modified by default.
"""
import argparse
import hashlib
import json
from pathlib import Path
from network_manifest import build_document

PROFILE = 'disk-floor-v1;protocol=2;initial-selection=0;tick-ms=50;hold-ms=250;axis=1000;turn=pi;skin=0.001;max-step=4096;contacts=8'
ROLES = ('stage', 'actor', 'floor', 'walls')


def native_inventory(path: Path, root: Path):
    """Independent test inventory, not an alternative native format loader.

    The C++ Actor loader validates resource formats and reference semantics.
    This bounded audit checks the recipe's declared bytes and ordered labels.
    """
    if path.stat().st_size > 65536:
        raise ValueError('native recipe exceeds 64 KiB')
    with path.open('rb') as stream:
        raw = stream.read(65537)
    if len(raw) > 65536 or not raw.startswith(b'MH4U-NATIVE-ACTOR 1\n') or not raw.endswith(b'\n') or b'\0' in raw or b'\r' in raw:
        raise ValueError('invalid native recipe text')
    files = {path.relative_to(root).as_posix(): (hashlib.sha256(raw).hexdigest(), len(raw))}
    clips = []
    for line in raw.decode('utf-8').splitlines()[1:]:
        row = line.split('\t')
        if row[0] in ('model', 'motion', 'texture', 'commands'):
            if len(row) != 3 or not row[1] or ':' in row[1] or '\\' in row[1]:
                raise ValueError('invalid native resource row')
            relative = Path(row[1])
            if relative.is_absolute() or '..' in relative.parts or '.' in row[1].split('/'):
                raise ValueError('invalid native relative resource')
            source = (path.parent/relative).resolve()
            source.relative_to(path.parent)
            size = source.stat().st_size
            if size > 512*1024*1024-sum(value[1] for value in files.values()):
                raise ValueError('native inventory exceeds byte budget')
            hasher = hashlib.sha256()
            consumed = 0
            with source.open('rb') as stream:
                while block := stream.read(65536):
                    consumed += len(block)
                    if consumed > size:
                        raise ValueError('native resource grew while hashing')
                    hasher.update(block)
            if consumed != size:
                raise ValueError('native resource changed while hashing')
            digest = hasher.hexdigest()
            if digest != row[2]:
                raise ValueError('native resource SHA256 mismatch')
            name = source.relative_to(root).as_posix()
            if name in files and files[name] != (digest, size):
                raise ValueError('native resource changed while identifying world')
            files[name] = (digest, size)
        elif row[0] == 'action' and len(row) == 4:
            clips.append(row[3])
        elif not ((row[0] == 'primitive' and len(row) == 4) or (row[0] == 'rate' and len(row) == 2)):
            raise ValueError('unknown native recipe row')
    return files, clips


def identify(recipe: Path):
    recipe = recipe.resolve()
    root = recipe.parent
    data = recipe.read_bytes()
    if len(data) > 65536 or not data.startswith(b'MH4U-DIAGNOSTIC-WORLD 1\n') or not data.endswith(b'\n') or b'\0' in data or b'\r' in data:
        raise ValueError('invalid bounded recipe text')
    fields = {}
    for line in data.decode('utf-8').splitlines()[1:]:
        pair = line.split('\t')
        if len(pair) != 2 or not all(pair) or pair[0] in fields:
            raise ValueError('invalid or duplicated recipe field')
        fields[pair[0]] = pair[1]
    files = {recipe.name: (hashlib.sha256(data).hexdigest(), len(data))}
    lines = ['MH4U-NATIVE-WORLD 1', 'profile\t'+PROFILE, 'recipe\t'+recipe.name]
    clips = []
    for role in ROLES:
        path = (root / fields[role]).resolve()
        name = path.relative_to(root).as_posix()
        lines.append(f'role\t{role}\t{name}')
        if role == 'actor' and path.suffix == '.mhnative':
            native_files, clips = native_inventory(path, root)
            for name, value in native_files.items():
                if name in files and files[name] != value:
                    raise ValueError('native resource changed while identifying world')
                files[name] = value
            continue
        document = build_document(path, root).decode('utf-8').splitlines()
        for line in document:
            parts = line.split('\t')
            if parts[0] == 'file':
                value = (parts[1], int(parts[2]))
                if parts[3] in files and files[parts[3]] != value:
                    raise ValueError('resource changed while identifying world')
                files[parts[3]] = value
            if role == 'actor' and parts[0] == 'clip':
                clips.append(parts[2])
    if len(files) > 256 or sum(size for _, size in files.values()) > 512*1024*1024:
        raise ValueError('world content budget exceeded')
    lines.append(f'clips\t{len(clips)}')
    lines.extend(f'clip\t{i}\t{name}' for i, name in enumerate(clips))
    lines.extend(f'file\t{digest}\t{size}\t{name}' for name, (digest, size) in sorted(files.items()))
    encoded = ('\n'.join(lines)+'\n').encode('utf-8')
    if len(encoded) > 1024*1024:
        raise ValueError('identity text budget exceeded')
    return {'digest': hashlib.sha256(encoded).hexdigest(), 'files': files, 'clips': clips, 'document': encoded}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('recipe', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = identify(args.recipe)
    if args.output:
        with args.output.open('xb') as file:
            file.write(result['document'])
    print(json.dumps({'digest': result['digest'], 'files': len(result['files']), 'bytes': sum(size for _, size in result['files'].values()), 'clips': len(result['clips'])}, indent=2))


if __name__ == '__main__':
    main()
