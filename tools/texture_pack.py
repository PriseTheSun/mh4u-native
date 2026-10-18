#!/usr/bin/env python3
"""Build a local PNG replacement pack bound to original encoded image hashes."""
import argparse
import hashlib
import json
from pathlib import Path


def png_bytes(path):
    with path.open('rb') as stream:
        data = stream.read(64 * 1024 * 1024 + 1)
    if len(data) > 64 * 1024 * 1024 or not data.startswith(b'\x89PNG\r\n\x1a\n'):
        raise ValueError(f'Expected PNG within 64 MiB: {path}')
    return data


def create(name, pairs, output):
    if not name or len(name.encode('utf-8')) > 120 or any(ord(c) < 32 or ord(c) == 127 for c in name):
        raise ValueError('Pack name requires 1–120 UTF-8 bytes without controls')
    if not 1 <= len(pairs) <= 4096:
        raise ValueError('Expected 1–4096 replacement pairs')
    replacements, originals, records = {}, set(), []
    total = 0
    for original, replacement in pairs:
        source = png_bytes(original)
        target = png_bytes(replacement)
        source_hash = hashlib.sha256(source).hexdigest()
        target_hash = hashlib.sha256(target).hexdigest()
        if source_hash in originals:
            raise ValueError('An original image is replaced more than once')
        originals.add(source_hash)
        total += len(target)
        if total > 256 * 1024 * 1024:
            raise ValueError('Replacements exceed the 256 MiB encoded budget')
        filename = target_hash + '.png'
        replacements[filename] = target
        records.append({'original': str(original.resolve()), 'original_sha256': source_hash,
                        'replacement': str(replacement.resolve()), 'replacement_sha256': target_hash,
                        'size': len(target), 'file': filename})
    output.mkdir(parents=True, exist_ok=False)
    lines = ['MH4U-TEXTURE-PACK 1', 'name\t' + name]
    for record in sorted(records, key=lambda r: r['original_sha256']):
        lines.append('\t'.join(('replace', record['original_sha256'], record['replacement_sha256'],
                                str(record['size']), record['file'])))
    for filename, data in replacements.items():
        (output / filename).write_bytes(data)
    (output / 'pack.tsv').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    (output / 'provenance.json').write_text(json.dumps({'name': name, 'inputs': records,
        'limits': 'PNG header/size checked here; native loader validates decoded pixels and allocation budget'},
        ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return output / 'pack.tsv'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name', required=True)
    parser.add_argument('--replace', nargs=2, type=Path, action='append', required=True,
                        metavar=('ORIGINAL_PNG', 'REPLACEMENT_PNG'))
    parser.add_argument('--output', type=Path, required=True, help='New output directory')
    args = parser.parse_args()
    print(create(args.name, args.replace, args.output))


if __name__ == '__main__':
    main()
