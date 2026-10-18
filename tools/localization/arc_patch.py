#!/usr/bin/env python3
"""Build a separate ARC19 text override; never rewrite the supplied ROM/archive."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'resources'))
import extract_model as resource
import lmd


def replace_member(archive, ordinal, replacement):
    if len(archive) > resource.MAX_ARC or len(replacement) > resource.MAX_MEMBER:
        raise ValueError('ARC/member exceeds the bounded probe profile')
    parsed = resource.arc_members(archive)
    if type(ordinal) is not int or not 0 <= ordinal < len(parsed['entries']):
        raise ValueError('Invalid member ordinal')
    entry = parsed['entries'][ordinal]
    original = resource.inflate_member(archive, entry)
    if original == replacement:
        return archive
    compressed = zlib.compress(replacement, level=9)
    if len(archive)+len(compressed) > resource.MAX_ARC:
        raise ValueError('Patched archive exceeds 128 MiB')
    # ARC19 members in this sample are byte-packed and use explicit offsets.
    # Append the new stream. Preserve every original payload, gap, name and flag.
    output = bytearray(archive)
    struct.pack_into('<3I', output, 12+ordinal*80+68, len(compressed),
                     (entry['size_flags_raw'] & 0xf0000000) | len(replacement), len(archive))
    output.extend(compressed)
    result = bytes(output)
    check = resource.arc_members(result)['entries'][ordinal]
    if resource.inflate_member(result, check) != replacement:
        raise ValueError('Patched member readback disagrees')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--member', required=True)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve() in (args.archive.resolve(), args.catalog.resolve(), args.report.resolve()):
        parser.error('Output must be distinct from source, catalog and report')
    if args.report.resolve() in (args.archive.resolve(), args.catalog.resolve()):
        parser.error('Report must be distinct from inputs')
    with args.archive.open('rb') as stream:
        archive = stream.read(resource.MAX_ARC+1)
    if len(archive) > resource.MAX_ARC:
        raise ValueError('Archive exceeds 128 MiB')
    entries = resource.arc_members(archive)['entries']
    matches = [entry for entry in entries if entry['name'] == args.member]
    if len(matches) != 1 or matches[0]['kind'] != '62440501':
        raise ValueError('Expected exactly one observed LMD member (type 0x62440501)')
    entry = matches[0]
    original = resource.inflate_member(archive, entry)
    document = lmd.decode(original)
    translations = lmd.load_catalog(args.catalog, document)
    replacement = lmd.rebuild(document, translations)
    rebuilt = lmd.decode(replacement)
    if rebuilt.labels != document.labels or rebuilt.controls != document.controls:
        raise ValueError('LMD labels/control records changed')
    for i, text in enumerate(rebuilt.texts):
        if text.value != translations.get(i, document.texts[i].value):
            raise ValueError('LMD readback differs from requested text')
    output = replace_member(archive, entry['index'], replacement)
    after = resource.arc_members(output)['entries']
    for before, current in zip(entries, after):
        if before['index'] != entry['index'] and before != current:
            raise ValueError('Untouched directory record changed')
    start = 12+entry['index']*80+68
    if output[:start] != archive[:start] or output[start+12:len(archive)] != archive[start+12:]:
        raise ValueError('Bytes outside selected directory fields changed')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    result = resource.save(args.output, output)
    report = {
        'schema': 'mh4u-reference-text-override-v1',
        'source_archive': {'path': str(args.archive.resolve()), 'sha256': resource.sha(archive), 'size': len(archive)},
        'source_member': entry,
        'source_lmd_sha256': resource.sha(original),
        'catalog': {'path': str(args.catalog.resolve()), 'sha256': hashlib.sha256(args.catalog.read_bytes()).hexdigest()},
        'translated_fragments': len(translations), 'total_fragments': len(document.texts),
        'output': result, 'output_lmd_sha256': resource.sha(replacement),
        'untouched_archive_records': len(entries)-1,
        'original_archive_bytes_preserved_except_selected_12_byte_record': True,
        'labels_controls_and_untranslated_text_preserved': True,
        'rom_modified': False, 'runtime_validated': False,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    resource.save(args.report, json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    main()
