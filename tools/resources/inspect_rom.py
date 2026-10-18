#!/usr/bin/env python3
"""Local, read-only NCSD/NCCH investigation. Not a REA provider or a game port.

Format references: Azahar ncch_container.{h,cpp} and layered_fs.{h,cpp}.
Supports plaintext NCCH and the common IVFC level-3 layout at +0x1000.
Does not decrypt, execute target code, or extract RomFS payloads.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import time

CAP = 64 * 1024 * 1024  # Explicit per-buffer memory budget for this investigation.
NIL = 0xffffffff


def require(condition, message):
    if not condition:
        raise ValueError(message)


def u32(b, offset):
    return struct.unpack_from('<I', b, offset)[0]


def digest(b):
    return hashlib.sha256(b).hexdigest()


def within(offset, size, limit):
    require(0 <= offset <= limit and 0 <= size <= limit - offset,
            f'Out of bounds: offset={offset}, size={size}, limit={limit}')


def blz_decode(data, expected):
    require(8 <= len(data) <= expected <= CAP, 'BLZ size exceeds declared image or memory budget')
    footer, extra = struct.unpack_from('<II', data, len(data) - 8)
    header_size, encoded_size = footer >> 24, footer & 0xffffff
    require(len(data) + extra == expected, 'BLZ expansion differs from declared segment pages')
    require(8 <= header_size <= encoded_size <= len(data), 'Invalid BLZ footer')
    stop, source, dest = len(data) - encoded_size, len(data) - header_size, expected
    out = bytearray(expected)
    out[:len(data)] = data
    while source > stop:
        source -= 1
        flags = data[source]
        for bit in range(7, -1, -1):
            if source <= stop:
                break
            if flags & (1 << bit):
                require(source - stop >= 2, 'Truncated BLZ backreference')
                source -= 2
                pair = struct.unpack_from('<H', data, source)[0]
                count, displacement = (pair >> 12) + 3, (pair & 0xfff) + 3
                require(dest - count >= stop, 'BLZ writes before prefix')
                for _ in range(count):
                    dest -= 1
                    require(dest + displacement < expected, 'BLZ reference outside output')
                    out[dest] = out[dest + displacement]
            else:
                require(source > stop and dest > stop, 'Truncated BLZ literal')
                source -= 1
                dest -= 1
                out[dest] = data[source]
    require(source == stop and dest == stop, 'BLZ did not exactly fill declared output')
    return out


def inspect(source, destination):
    start = time.monotonic()
    destination.mkdir(parents=True, exist_ok=False)
    with source.open('rb') as f:
        before = os.fstat(f.fileno())
        size = before.st_size

        def read(offset, length):
            within(offset, length, size)
            require(length <= CAP, 'Read exceeds local per-buffer budget')
            f.seek(offset)
            data = f.read(length)
            require(len(data) == length, 'Short read')
            return data

        def hash_range(offset, length):
            within(offset, length, size)
            f.seek(offset)
            h = hashlib.sha256()
            remaining = length
            while remaining:
                block = f.read(min(4 * 1024 * 1024, remaining))
                require(bool(block), 'Short read during hashing')
                h.update(block)
                remaining -= len(block)
            return h.hexdigest()

        ncsd = read(0, 0x200)
        require(ncsd[0x100:0x104] == b'NCSD', 'Expected NCSD container')
        partitions = []
        for i in range(8):
            off, length = struct.unpack_from('<II', ncsd, 0x120 + i * 8)
            if not length:
                continue
            off, length = off * 512, length * 512
            within(off, length, size)
            h = read(off, 512)
            require(h[0x100:0x104] == b'NCCH', 'Expected NCCH partition')
            unit = 512 << h[0x18e]
            require(unit * u32(h, 0x104) == length, 'NCCH/NCSD size mismatch')
            partitions.append({'index': i, 'offset': off, 'size': length,
                               'program_id': f'{struct.unpack_from("<Q", h, 0x118)[0]:016X}',
                               'product_code': h[0x150:0x160].split(b'\0')[0].decode('ascii'),
                               'no_crypto_flag': bool(h[0x18f] & 4),
                               'header_sha256': digest(h)})
        ordered = sorted(partitions, key=lambda item: item['offset'])
        require(all(a['offset'] + a['size'] <= b['offset'] for a, b in zip(ordered, ordered[1:])),
                'Overlapping NCSD partitions')
        p = next(p for p in partitions if p['index'] == 0)
        require(p['no_crypto_flag'], 'Partition 0 is encrypted; this script does not decrypt')
        base = p['offset']
        h = read(base, 512)
        unit = 512 << h[0x18e]

        def part_read(relative, length):
            within(relative, length, p['size'])
            return read(base + relative, length)

        ext = part_read(512, u32(h, 0x180))
        require(len(ext) == 1024, 'Unsupported extended-header layout')
        require(digest(ext) == h[0x160:0x180].hex(), 'Extended-header hash mismatch')
        segments, code_offset = [], 0
        for label, offset in [('text', 0x10), ('rodata', 0x20), ('data', 0x30)]:
            addr, pages, used = struct.unpack_from('<III', ext, offset)
            require(used <= pages * 4096, 'Segment size exceeds declared pages')
            segments.append({'name': label, 'virtual_address': addr, 'code_bin_offset': code_offset,
                             'initialized_size': used, 'page_aligned_size': pages * 4096})
            code_offset += pages * 4096
        exoff, exsize = u32(h, 0x1a0) * unit, u32(h, 0x1a4) * unit
        within(exoff, exsize, p['size'])
        exhashsize = u32(h, 0x1a8) * unit
        within(0, exhashsize, exsize)
        require(digest(part_read(exoff, exhashsize)) == h[0x1c0:0x1e0].hex(), 'ExeFS superblock hash mismatch')
        table = part_read(exoff, 512)
        files = []
        code = None
        for i in range(10):
            name, off, length = struct.unpack_from('<8sII', table, i * 16)
            if not name.strip(b'\0'):
                require(length == 0, 'Unnamed nonempty ExeFS entry')
                continue
            within(512 + off, length, exsize)
            name = name.split(b'\0')[0].decode('ascii')
            data = part_read(exoff + 512 + off, length)
            sha = digest(data)
            require(sha == table[0x1e0 - 32*i:0x200 - 32*i].hex(), f'ExeFS hash mismatch: {name}')
            files.append({'name': name, 'source_offset': base + exoff + 512 + off,
                          'size': length, 'sha256': sha, 'declared_hash_matches': True})
            if name == '.code':
                require(code is None, 'Duplicate .code entry')
                (destination/'code.exefs.bin').write_bytes(data)
                code = blz_decode(data, code_offset) if ext[0xd] & 1 else data
        require(code is not None and len(code) == code_offset, 'Missing or unexpected code image')
        (destination/'code.bin').write_bytes(code)
        services = [ext[o:o+8].split(b'\0')[0].decode('ascii') for o in range(0x250, 0x360, 8)
                    if ext[o:o+8].strip(b'\0')]
        mapping = {'process_name': ext[:8].split(b'\0')[0].decode('ascii'),
                   'segments': segments, 'bss_size': u32(ext, 0x3c),
                   'stack_size': u32(ext, 0x1c), 'remaster_version': struct.unpack_from('<H', ext, 0xe)[0],
                   'entry_point_derived_from_text_address': segments[0]['virtual_address'],
                   'raw_contiguous_mapping': all(b['virtual_address'] == a['virtual_address'] + a['page_aligned_size']
                                                 for a, b in zip(segments, segments[1:])),
                   'declared_services': services, 'code_compressed': bool(ext[0xd] & 1),
                   'code_bin_sha256': digest(code), 'code_bin_size': len(code)}

        roff, rsize = u32(h, 0x1b0) * unit, u32(h, 0x1b4) * unit
        within(roff, rsize, p['size'])
        ivfc = part_read(roff, 0x60)
        require(ivfc[:4] == b'IVFC' and u32(ivfc, 4) == 0x10000, 'Unsupported IVFC layout')
        rhsize = u32(h, 0x1b8) * unit
        within(0, rhsize, rsize)
        rhash_match = digest(part_read(roff, rhsize)) == h[0x1e0:0x200].hex()
        require(rhash_match, 'RomFS superblock hash mismatch')
        # Common retail layout used by the referenced loader; validate before interpreting it.
        level3, lsize = roff + 0x1000, rsize - 0x1000
        rh = struct.unpack('<10I', part_read(level3, 40))
        require(rh[0] == 40, 'Unsupported level-3 location or header')
        for off, length in [(rh[1],rh[2]),(rh[3],rh[4]),(rh[5],rh[6]),(rh[7],rh[8])]:
            within(off, length, lsize)
            require(off + length <= rh[9], 'Metadata overlaps RomFS file data')
        dirs, entries = part_read(level3 + rh[3], rh[4]), part_read(level3 + rh[7], rh[8])
        pending, seen_dirs, seen_files, inventory = [(0, '', 0)], set(), set(), []
        directory_bytes = file_bytes = 0

        def name_at(blob, offset, length):
            within(offset, length, len(blob))
            require(length % 2 == 0, 'Odd UTF-16 name length')
            raw = blob[offset:offset+length]
            return raw.decode('utf-16-le'), raw.hex()

        while pending:
            off, parent_path, expected_parent = pending.pop()
            require(off not in seen_dirs, 'Repeated or cyclic directory')
            seen_dirs.add(off)
            within(off, 24, len(dirs))
            parent, sibling, child, first_file, bucket, n = struct.unpack_from('<6I', dirs, off)
            require(parent == expected_parent, 'Inconsistent directory parent')
            name, raw_name = name_at(dirs, off+24, n)
            path = parent_path + name + '/' if off else '/'
            directory_bytes += (24+n+3) & ~3
            current = first_file
            while current != NIL:
                require(current not in seen_files, 'Repeated or cyclic file')
                seen_files.add(current)
                within(current, 32, len(entries))
                parent, nxt, dataoff, length, bucket, n = struct.unpack_from('<IIQQII', entries, current)
                require(parent == off, 'Inconsistent file parent')
                name, raw_name = name_at(entries, current+32, n)
                within(rh[9] + dataoff, length, lsize)
                inventory.append({'path': path+name, 'name_utf16le_hex': raw_name,
                                  'metadata_offset': current, 'source_offset': base+level3+rh[9]+dataoff,
                                  'size': length})
                file_bytes += (32+n+3) & ~3
                current = nxt
            siblings = set()
            while child != NIL:
                require(child not in siblings, 'Directory sibling cycle')
                siblings.add(child)
                within(child, 24, len(dirs))
                pending.append((child, path, off))
                child = u32(dirs, child+4)
        rom_sha = hash_range(0, size)
        after = os.fstat(f.fileno())
        require((before.st_dev,before.st_ino,before.st_size,before.st_mtime_ns) ==
                (after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns), 'Source changed during inspection')
        manifest = {'schema': 'mh4u-local-inspection-v1', 'producer': 'inspect_rom.py (ordinary local tooling, not REA Evidence)',
                    'source': {'path': str(source), 'size': size, 'sha256': rom_sha},
                    'partitions': partitions, 'exefs': files, 'memory_map': mapping,
                    'checks': {'exheader_hash_matches': True, 'exefs_superblock_hash_matches': True,
                               'romfs_superblock_hash_matches': rhash_match, 'source_stat_stable': True},
                    'romfs': {'directory_count': len(seen_dirs), 'file_count': len(inventory),
                              'directory_table_bytes_visited': directory_bytes, 'directory_table_size': len(dirs),
                              'file_table_bytes_visited': file_bytes, 'file_table_size': len(entries),
                              'all_metadata_record_bytes_visited': directory_bytes == len(dirs) and file_bytes == len(entries),
                              'payloads_extracted': False, 'full_ivfc_tree_verified': False},
                    'limitations': ['No signature authentication', 'No execution or function discovery',
                                    'RomFS file contents and internal archives not decoded',
                                    'Header service declarations do not prove runtime usage',
                                    'Only partition 0 payload metadata inspected; no separate game update supplied'],
                    'elapsed_seconds': time.monotonic()-start}
        for filename, value in [('manifest.json', manifest), ('romfs-inventory.json', inventory), ('memory-map.json', mapping)]:
            (destination/filename).write_text(json.dumps(value, ensure_ascii=True, indent=2)+'\n')
        print(json.dumps({'source_sha256': rom_sha, 'program_id': p['program_id'],
                          'product_code': p['product_code'], 'code_bin_size': len(code),
                          'romfs': manifest['romfs'], 'output': str(destination.resolve())}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True, help='Plaintext local .3ds container; opened read-only')
    parser.add_argument('--output', type=Path, required=True, help='New inspection directory; must not exist')
    args = parser.parse_args()
    inspect(args.rom, args.output)
