#!/usr/bin/env python3
"""Inventory selected MH4U ARC v19 resources; no target execution or ROM writes.

Reference: https://github.com/svanheulen/mhff/wiki/MH4U-rArchive-Format
File names and flags remain source facts; no extension/type inference is made.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from inspect_rom import require, within


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inspection', type=Path, required=True, help='Directory produced by inspect_rom.py')
    parser.add_argument('--output', type=Path, required=True, help='New JSON catalog; must not exist')
    parser.add_argument('--romfs-path', dest='romfs_paths', action='append', required=True,
                        help='Exact RomFS archive path; repeat to select more archives')
    args = parser.parse_args()
    manifest = json.loads((args.inspection/'manifest.json').read_text())
    inventory = json.loads((args.inspection/'romfs-inventory.json').read_text())
    selected = []
    with Path(manifest['source']['path']).open('rb') as f:
        for name in args.romfs_paths:
            matches = [x for x in inventory if x['path'] == name]
            require(len(matches) == 1, f'Path missing or ambiguous: {name}')
            member = matches[0]
            require(member['size'] <= 64*1024*1024, 'Archive exceeds local inspection budget')
            f.seek(member['source_offset'])
            raw = f.read(member['size'])
            require(len(raw) == member['size'], 'Short ARC read')
            require(len(raw) >= 12, 'Truncated ARC header')
            magic, version, count, unknown = struct.unpack_from('<4sHHI', raw)
            require(magic == b'ARC\0' and version == 19, 'Only plaintext ARC version 19 is handled')
            within(12, count*80, len(raw))
            entries = []
            for i in range(count):
                path, kind, compressed, size_flags, off = struct.unpack_from('<64sIIII', raw, 12+i*80)
                within(off, compressed, len(raw))
                require(off >= 12+count*80, 'ARC payload overlaps directory')
                entries.append({'ordinal': i, 'path': path.split(b'\0')[0].decode('ascii'),
                                'raw_name_hex': path.hex(), 'type_id_hex': f'{kind:08X}',
                                'compressed_size': compressed, 'uncompressed_size': size_flags & 0x0fffffff,
                                'raw_size_flags_hex': f'{size_flags:08X}', 'archive_offset': off,
                                'rom_source_offset': member['source_offset'] + off,
                                'compressed_sha256': hashlib.sha256(raw[off:off+compressed]).hexdigest()})
            ranges = sorted((x['archive_offset'], x['compressed_size']) for x in entries)
            require(all(a+n <= b for (a,n),(b,_) in zip(ranges,ranges[1:])), 'Overlapping ARC payloads')
            selected.append({'romfs_path': name, 'source_offset': member['source_offset'],
                             'sha256': hashlib.sha256(raw).hexdigest(), 'version': version,
                             'unknown_header_word': unknown, 'entries': entries})
    result = {'producer': 'inspect_arc.py (ordinary local tooling, not REA Evidence)',
              'parent_rom_identity': manifest['source'],
              'coverage': 'Only explicitly selected ARC archives; every directory entry read; payloads not decompressed',
              'archives': selected}
    with args.output.open('x') as out:
        json.dump(result, out, ensure_ascii=True, indent=2)
        out.write('\n')
    print(json.dumps([{'archive': a['romfs_path'], 'entry_count': len(a['entries']),
                       'message_name_candidates': [x['path'] for x in a['entries'] if 'msg' in x['path'].split('\\')]}
                      for a in selected], indent=2))


if __name__ == '__main__':
    main()
