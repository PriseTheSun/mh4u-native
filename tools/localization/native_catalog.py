#!/usr/bin/env python3
"""Build bounded UTF-8 native catalogs from the existing LMD owner and seeds.

The C++ consumer receives fragments and opaque provenance, never LMD commands.
Generated catalogs/oracles contain source text and belong in assets/local or
research/private, not the source repository. No ROM/profile mutation occurs.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import tempfile

import lmd

MAGIC = b'MH4U-NATIVE-TEXT 1\n'
MAX_PACKAGE = 8 * 1024 * 1024
MAX_DOCUMENTS = 64
MAX_FRAGMENTS = 65536
MAX_TEXT = 256 * 1024
MAX_METADATA = 4 * 1024 * 1024
NAME = re.compile(r'[A-Za-z0-9_]{1,128}\Z')


def require(ok, code, message):
    lmd.require(ok, code, message)


def netstring(value: str) -> bytes:
    raw = value.encode('utf-8', errors='strict')
    return str(len(raw)).encode('ascii') + b':' + raw + b','


def newline_shape(value):
    lines = value.split('\n')
    return {'count': len(lines) - 1, 'blank_lines': [i for i, line in enumerate(lines) if not line],
            'trailing': value.endswith('\n')}


def read_document(source: Path, catalog: Path | None, locale: str) -> dict:
    raw = lmd.read_bytes(source, MAX_PACKAGE)
    doc = lmd.decode(raw)
    require(len(doc.texts) <= MAX_FRAGMENTS, 'resource_limit', 'Source exceeds native fragment limit')
    require(NAME.fullmatch(doc.internal_name) is not None, 'unsupported_document_name',
            'Native document name must be 1..128 ASCII letters, digits or underscores')
    require(lmd.rebuild(doc) == raw, 'roundtrip_mismatch', 'Source LMD rebuild differs')
    catalog_raw = lmd.read_bytes(catalog, MAX_PACKAGE) if catalog else None
    translations = lmd.load_catalog(catalog, doc) if catalog else {}
    if catalog:
        require(lmd.read_bytes(catalog, MAX_PACKAGE) == catalog_raw, 'input_changed', 'Seed changed during owner validation')
    seed = json.loads(catalog_raw.decode('utf-8')) if catalog_raw is not None else None
    if seed:
        require(seed.get('locale') == locale, 'locale_mismatch', 'Seed locale differs from selected locale')
        # These optional source facts must agree if supplied, not be silently ignored.
        source_meta = seed['source']
        if 'size' in source_meta:
            require(source_meta['size'] == doc.source_size, 'source_identity_mismatch', 'Seed source size differs')
        if 'archive' in source_meta:
            require(isinstance(source_meta['archive'], str) and source_meta['archive'].startswith('/eng/'),
                    'source_locale_mismatch', 'Selected archive is not marked English')
        if 'entry_path' in source_meta:
            require(isinstance(source_meta['entry_path'], str) and source_meta['entry_path'].startswith('eng\\')
                    and source_meta['entry_path'].endswith('_eng'),
                    'source_locale_mismatch', 'Selected resource is not marked English')
        for item in seed['entries']:
            index = item['text_index']
            if 'key' in item:
                require(item['key'] == f'{doc.internal_name}:{index:06d}', 'fragment_identity_mismatch', 'Seed key differs from stable index')
            if 'source_offset' in item:
                require(item['source_offset'] == doc.texts[index].offset, 'fragment_identity_mismatch', 'Seed source offset differs')
            if 'label_references' in item:
                require(item['label_references'] == doc.references(index), 'fragment_identity_mismatch', 'Seed references differ from LMD owner')
            if 'protected_tokens' in item:
                require(item['protected_tokens'] == lmd.protected_tokens(doc.texts[index].value),
                        'fragment_identity_mismatch', 'Seed protected-token facts differ from source')
    # Invoke the owner to verify controls/placeholders and UTF-16 validity.
    rebuilt = lmd.decode(lmd.rebuild(doc, translations))
    require(rebuilt.labels == doc.labels and rebuilt.controls == doc.controls,
            'immutable_data_changed', 'LMD owner changed labels or controls')
    fragments = []
    for index, text in enumerate(doc.texts):
        translated = translations.get(index)
        if translated is not None:
            require(newline_shape(translated) == newline_shape(text.value),
                    'line_structure_changed', f'Translation {index} changes line/blank/trailing structure')
            if translated == text.value:
                translated = None  # preserve the source; no false translation coverage
        require(rebuilt.texts[index].value == translations.get(index, text.value),
                'readback_mismatch', 'Owner readback differs from requested translation')
        for value in (text.value, translated):
            if value is not None:
                require(len(value.encode('utf-8')) <= MAX_TEXT, 'resource_limit', 'Text exceeds 256 KiB')
        fragments.append({'index': index, 'source_offset': text.offset,
                          'original': text.value, 'translation': translated})
    exported = lmd.export(doc, source)
    metadata = {'schema': 'mh4u-native-source-facts-v1', 'lmd_profile': lmd.PROFILE,
                'source_size': doc.source_size, 'header_u32': exported['header_u32'],
                'labels': exported['labels'], 'controls': exported['controls'],
                'fragment_facts': [{key: item[key] for key in
                    ('text_index', 'descriptor_offset', 'offset', 'length_utf16_units',
                     'second_length_raw', 'padding_hex', 'protected_tokens', 'label_references')}
                    for item in exported['texts']],
                'unknowns': exported['unknowns'],
                'interpretation': 'Opaque provenance; preserve fragment boundaries; execute no LMD commands'}
    metadata_text = json.dumps(metadata, ensure_ascii=False, separators=(',', ':'), sort_keys=True)
    require(len(metadata_text.encode('utf-8')) <= MAX_METADATA, 'resource_limit', 'Document metadata exceeds 4 MiB')
    return {'identity': {'document': doc.internal_name, 'source_sha256': doc.source_sha256,
                         'fragment_count': len(doc.texts)},
            'metadata': metadata_text, 'fragments': fragments,
            'provenance': {'source_path': str(source.resolve()), 'source_size': doc.source_size,
                           'catalog_path': str(catalog.resolve()) if catalog else None,
                           'catalog_sha256': lmd.digest(catalog_raw) if catalog_raw is not None else None,
                           'seed_entries': len(translations),
                           'changed_fragments': sum(f['translation'] is not None for f in fragments),
                           'unchanged_seed_entries': sum(doc.texts[i].value == t for i, t in translations.items()),
                           'source_roundtrip_byte_exact': True,
                           'translated_owner_readback_all_fragments': True,
                           'labels_controls_tokens_line_structure_preserved': True}}


def document_fields(doc):
    identity = doc['identity']
    yield from (identity['document'], identity['source_sha256'], str(identity['fragment_count']), doc['metadata'])
    for fragment in doc['fragments']:
        translation = fragment['translation']
        yield from (str(fragment['index']), str(fragment['source_offset']), fragment['original'],
                    '1' if translation is not None else '0', translation if translation is not None else '')


def encoded_size(values):
    lengths = (len(value.encode('utf-8')) for value in values)
    return sum(len(str(n)) + 2 + n for n in lengths)


def encode(documents: list[dict], locale: str) -> bytes:
    require(locale in ('pt-BR', 'en'), 'unsupported_locale', 'Supported native locales are pt-BR and en')
    require(0 < len(documents) <= MAX_DOCUMENTS, 'resource_limit', 'Document count must be 1..64')
    require(sum(d['identity']['fragment_count'] for d in documents) <= MAX_FRAGMENTS,
            'resource_limit', 'Aggregate fragments exceed 65536')
    names = [d['identity']['document'] for d in documents]
    require(len(names) == len(set(names)), 'duplicate_identity', 'One selected source variant per document name is required')
    fields = [locale, 'en', lmd.PROFILE, str(len(documents))]
    for doc in sorted(documents, key=lambda d: d['identity']['document']):
        require(locale != 'en' or all(f['translation'] is None for f in doc['fragments']),
                'locale_mismatch', 'English catalog cannot contain target translations')
        fields.extend(document_fields(doc))
    # Account for UTF-8 expansion and framing before constructing the aggregate.
    total = len(MAGIC) + encoded_size(fields)
    require(total <= MAX_PACKAGE, 'resource_limit', 'Native catalog exceeds 8 MiB')
    output = MAGIC + b''.join(netstring(value) for value in fields)
    require(len(output) == total, 'internal_size_mismatch', 'Encoding size accounting differs')
    return output


def header(manifest):
    # No source/translated texts: this is a development identity pin only.
    docs = ',\n'.join('        {' + json.dumps(d['document']) + ', ' + json.dumps(d['source_sha256'])
                     + ', ' + str(d['fragment_count']) + '}' for d in manifest['documents'])
    return ('// Generated by tools/localization/native_catalog.py; names/digests/counts only.\n'
            '// Development identity pin: regenerate with the selected sources and seeds.\n'
            '#pragma once\n#include "localization.hpp"\n'
            'namespace mh4u::localization::generated {\n'
            'inline ExpectedCatalog expected_catalog() {\n'
            f'    return {{{json.dumps(manifest["locale"])}, {json.dumps(manifest["package_sha256"])}, {{\n'
            + docs + '\n    }};\n}\n}\n')


def build(pairs, output: Path, locale='pt-BR'):
    require(0 < len(pairs) <= MAX_DOCUMENTS, 'resource_limit', 'At most 64 source/catalog pairs')
    docs = []
    total_bytes = len(MAGIC) + encoded_size((locale, 'en', lmd.PROFILE, str(len(pairs))))
    total_fragments = 0
    names = set()
    for a, b in pairs:
        doc = read_document(Path(a), None if b == '-' else Path(b), locale)
        name = doc['identity']['document']
        require(name not in names, 'duplicate_identity', 'One selected source variant per document name is required')
        names.add(name)
        total_bytes += encoded_size(document_fields(doc))
        total_fragments += doc['identity']['fragment_count']
        require(total_bytes <= MAX_PACKAGE and total_fragments <= MAX_FRAGMENTS,
                'resource_limit', 'Aggregate native source facts exceed package/fragment policy')
        docs.append(doc)
    docs.sort(key=lambda d: d['identity']['document'])
    package = encode(docs, locale)
    manifest = {'schema': 'mh4u-native-text-manifest-v1', 'locale': locale, 'source_locale': 'en',
                'profile': lmd.PROFILE, 'package_file': 'catalog.mhtxt',
                'package_sha256': lmd.digest(package), 'package_size': len(package),
                'documents': [d['identity'] for d in docs],
                'fragment_count': sum(d['identity']['fragment_count'] for d in docs),
                'translated_count': sum(d['provenance']['changed_fragments'] for d in docs)}
    provenance = {'manifest': manifest, 'producer_sha256': lmd.digest(Path(__file__).read_bytes()),
                  'lmd_owner_sha256': lmd.digest(Path(lmd.__file__).read_bytes()),
                  'source_language_authority': 'Caller-selected English sources; optional seed archive/resource markers checked; not inferred from text',
                  'documents': [{'identity': d['identity'], **d['provenance']} for d in docs],
                  'native_renderer_validated': False,
                  'unknowns': ['LMD command execution remains unimplemented', 'Native glyph/layout coverage is unverified',
                               'Only selected source variants are included; no total-game coverage claim']}
    # This independent hex oracle is for comparing every C++ lookup with Python
    # owner values; it is not consumed by the application or the native loader.
    oracle = []
    for doc in docs:
        for f in doc['fragments']:
            target = f['translation'] if f['translation'] is not None else f['original']
            oracle.append('\t'.join([doc['identity']['document'], doc['identity']['source_sha256'],
                str(f['index']), str(f['source_offset']), f['original'].encode('utf-8').hex(),
                target.encode('utf-8').hex(), '1' if f['translation'] is not None else '0']))
    outputs = {'catalog.mhtxt': package, 'catalog_identity.hpp': header(manifest).encode('utf-8'),
               'manifest.json': (json.dumps(manifest, ensure_ascii=False, indent=2)+'\n').encode(),
               'provenance.json': (json.dumps(provenance, ensure_ascii=False, indent=2)+'\n').encode(),
               'lookup-oracle.tsv': ('\n'.join(oracle)+'\n').encode('utf-8')}
    # Preflight existing outputs before publishing any file. Identical reruns
    # are accepted; new revisions require another directory or explicit removal.
    for name, raw in outputs.items():
        dest = output/name
        require(not dest.exists() or dest.read_bytes() == raw, 'output_exists', f'Refusing to replace different {dest}')
    output.mkdir(parents=True, exist_ok=True)
    for name, raw in outputs.items():
        lmd.write_new(output/name, raw)
    return manifest


def self_test():
    # Public synthetic source exercises owner -> native package, no game data.
    original = 'Keep %s\n\ue000'
    raw_text = original.encode('utf-16le') + b'\0\0'
    raw_text += b'\0' * (-len(raw_text) % 4)
    source = (struct.pack('<9I', 0x646d6c, 0x1010b, 1, 2, 1, 36, 44, 76, 88+len(raw_text))
              + struct.pack('<IHH', 0x12345678, 0, 5)
              + struct.pack('<8I', 1, 0, 0, 0, 0x9f, 7, 8, 9)
              + struct.pack('<3I', 88, len(original.encode('utf-16le'))//2, len(original.encode('utf-16le'))//2)
              + raw_text + b'Fixture\0')
    checks = 0
    with tempfile.TemporaryDirectory(prefix='mh4u-native-catalog-') as temp:
        p = Path(temp); (p/'source.lmd').write_bytes(source)
        seed = {'schema': 'mh4u-translation-seed-v1', 'locale': 'pt-BR',
                'source': {'internal_name': 'Fixture', 'sha256': lmd.digest(source)},
                'entries': [{'text_index': 0, 'source_text': original, 'target_text': 'Manter %s\n\ue000'}]}
        def put(value): (p/'seed.json').write_text(json.dumps(value), encoding='utf-8')
        put(seed); pairs = [(str(p/'source.lmd'), str(p/'seed.json'))]
        manifest = build(pairs, p/'out'); checks += 1
        assert build(pairs, p/'out') == manifest; checks += 1
        assert netstring('é\n,:') == b'5:\xc3\xa9\n,:,'; checks += 1
        def reject(call, code):
            nonlocal checks
            try: call()
            except lmd.LmdError as error:
                assert error.code == code, (error.code, code); checks += 1
            else: raise AssertionError('Malformed input accepted')
        reject(lambda: build(pairs*2, p/'duplicate'), 'duplicate_identity')
        reject(lambda: build(pairs, p/'bad-locale', 'en'), 'locale_mismatch')
        bad = json.loads(json.dumps(seed));bad['source']['sha256'] = '0'*64;put(bad)
        reject(lambda: build(pairs,p/'bad-hash'), 'source_identity_mismatch')
        bad = json.loads(json.dumps(seed));bad['entries'][0]['source_offset'] = 89;put(bad)
        reject(lambda: build(pairs,p/'bad-offset'), 'fragment_identity_mismatch')
        bad = json.loads(json.dumps(seed));bad['source']['archive'] = None;put(bad)
        reject(lambda: build(pairs,p/'bad-archive-type'), 'source_locale_mismatch')
        bad = json.loads(json.dumps(seed));bad['source']['entry_path'] = 'fre\\msg\\Fixture_fre';put(bad)
        reject(lambda: build(pairs,p/'bad-language-marker'), 'source_locale_mismatch')
        bad = json.loads(json.dumps(seed));bad['entries'][0]['target_text'] = 'Manter %d\n\ue000';put(bad)
        reject(lambda: build(pairs,p/'bad-token'), 'protected_tokens_changed')
        bad = json.loads(json.dumps(seed));bad['entries'][0]['target_text'] = '\nManter %s\ue000';put(bad)
        reject(lambda: build(pairs,p/'bad-lines'), 'line_structure_changed')
        put(seed);(p/'out/catalog.mhtxt').write_bytes(b'changed')
        reject(lambda: build(pairs,p/'out'), 'output_exists')
    return {'synthetic_producer_checks': checks}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    b = commands.add_parser('build')
    b.add_argument('--document', nargs=2, metavar=('SOURCE_LMD', 'SEED_OR_DASH'), action='append', required=True)
    b.add_argument('--locale', default='pt-BR', choices=('pt-BR', 'en'))
    b.add_argument('--output', type=Path, required=True)
    commands.add_parser('self-test')
    args = parser.parse_args()
    try:
        result = self_test() if args.command == 'self-test' else build(args.document, args.output, args.locale)
        print(json.dumps(result, ensure_ascii=False, indent=2))
    except (lmd.LmdError, OSError, UnicodeError, json.JSONDecodeError) as error:
        record = error.record() if isinstance(error, lmd.LmdError) else {'code': 'io_or_encoding', 'message': str(error)}
        print(json.dumps({'error': record}, ensure_ascii=False), file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
