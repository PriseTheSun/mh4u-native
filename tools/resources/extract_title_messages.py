#!/usr/bin/env python3
"""Decode explicitly selected ARC message resources for later format research.

This does not interpret message records, translate them, or patch the game.
Compressed source bytes must match the previously retained ARC catalog.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zlib

from inspect_rom import require


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, required=True, help='JSON catalog produced by inspect_arc.py')
    parser.add_argument('--output', type=Path, required=True, help='New sample directory; must not exist')
    parser.add_argument('--archive', required=True, help='Exact RomFS archive path in the catalog')
    parser.add_argument('--resource', action='append', required=True,
                        help='Exact internal resource name, preserving backslashes; repeat to select more')
    args = parser.parse_args()
    catalog = json.loads(args.catalog.read_text())
    archives = [a for a in catalog['archives'] if a['romfs_path'] == args.archive]
    require(len(archives) == 1, f'Missing or ambiguous archive: {args.archive}')
    archive = archives[0]
    names = args.resource
    output = args.output
    output.mkdir(exist_ok=False)
    records = []
    with Path(catalog['parent_rom_identity']['path']).open('rb') as f:
        for name in names:
            entries = [e for e in archive['entries'] if e['path'] == name]
            require(len(entries) == 1, f'Missing or ambiguous source: {name}')
            e = entries[0]
            require(e['uncompressed_size'] <= 16*1024*1024, 'Message exceeds local decode budget')
            f.seek(e['rom_source_offset'])
            compressed = f.read(e['compressed_size'])
            require(hashlib.sha256(compressed).hexdigest() == e['compressed_sha256'], 'Source changed')
            decoder = zlib.decompressobj()
            data = decoder.decompress(compressed, e['uncompressed_size']+1)
            require(decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail,
                    'Incomplete stream, trailing data, or expansion beyond declaration')
            require(len(data) == e['uncompressed_size'], 'Wrong expanded size')
            filename = f'message-{e["ordinal"]:03}.bin'
            (output/filename).write_bytes(data)
            records.append({'archive': archive['romfs_path'], 'entry': e, 'output': filename,
                            'sha256': hashlib.sha256(data).hexdigest(), 'size': len(data),
                            'prefix_32_hex': data[:32].hex()})
    (output/'manifest.json').write_text(json.dumps({'producer':'ordinary local zlib decoding, not REA Evidence',
        'parent_rom_identity':catalog['parent_rom_identity'], 'records':records,
        'limitations':['Message schema not interpreted', 'No translated patch produced', 'No in-game validation']},indent=2)+'\n')
    print(json.dumps([{'path':r['entry']['path'], 'size':r['size'], 'prefix':r['prefix_32_hex']} for r in records],indent=2))


if __name__ == '__main__':
    main()
