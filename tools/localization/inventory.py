#!/usr/bin/env python3
"""Inventory English LMD resources into a private, provenance-preserving SQLite.

Uses the existing resource extractor and LMD producer. No translation generation,
ARC mutation, game execution or guessing of language from text content.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import resource as usage
import sqlite3
import struct
import sys
import time

import lmd

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'resources'))
import index_resources as resources

LMD_TYPE = 0x62440501
SCHEMA = 'mh4u-english-lmd-inventory-v1'


def encode(value):
    return json.dumps(value, ensure_ascii=False, separators=(',', ':'))


def classify(row):
    """Language markers are observed identity, not detection of natural language."""
    markers = {'archive_eng': row['archive'].startswith('/eng/'),
               'name_eng': row['name'].startswith('eng\\'),
               'suffix_eng': row['name'].endswith('_eng')}
    if all(markers.values()):
        return 'english', markers
    if any(markers.values()):
        return 'ambiguous', markers
    return 'other', markers


def initialize(connection):
    connection.executescript('''
      PRAGMA foreign_keys=ON;
      CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);
      CREATE TABLE documents(
        sha256 TEXT PRIMARY KEY, source_size INTEGER NOT NULL, representative_entry INTEGER NOT NULL,
        status TEXT NOT NULL, profile TEXT, internal_name TEXT, header_json TEXT,
        labels_count INTEGER, controls_count INTEGER, fragments_count INTEGER,
        roundtrip_byte_exact INTEGER NOT NULL, error_json TEXT);
      CREATE TABLE members(
        entry_id INTEGER PRIMARY KEY, archive TEXT NOT NULL, name TEXT NOT NULL,
        language_status TEXT NOT NULL, markers_json TEXT NOT NULL,
        sha256 TEXT REFERENCES documents(sha256), duplicate_of_entry INTEGER,
        extraction_json TEXT, error_json TEXT);
      CREATE TABLE labels(
        document_sha256 TEXT REFERENCES documents(sha256), ordinal INTEGER, source_offset INTEGER,
        identifier_hex TEXT, control_start_index INTEGER, unknown_u16 INTEGER,
        PRIMARY KEY(document_sha256,ordinal));
      CREATE TABLE controls(
        document_sha256 TEXT REFERENCES documents(sha256), control_index INTEGER,
        source_offset INTEGER, words_json TEXT, PRIMARY KEY(document_sha256,control_index));
      CREATE TABLE fragments(
        document_sha256 TEXT REFERENCES documents(sha256), text_index INTEGER,
        original_key TEXT, descriptor_offset INTEGER, source_offset INTEGER,
        utf16_units INTEGER, second_length_raw INTEGER, padding_hex TEXT,
        source_text TEXT NOT NULL, text_utf16_sha256 TEXT NOT NULL,
        protected_tokens_json TEXT NOT NULL, label_references_json TEXT NOT NULL,
        PRIMARY KEY(document_sha256,text_index));
      CREATE INDEX fragments_exact_text ON fragments(text_utf16_sha256);
      CREATE INDEX members_document ON members(sha256);
      CREATE INDEX members_name ON members(name);
    ''')


def insert_document(connection, raw, entry_id, source_path):
    sha = lmd.digest(raw)
    header = list(struct.unpack_from('<9I', raw)) if len(raw) >= 36 else None
    try:
        document = lmd.decode(raw)
        lmd.require(lmd.rebuild(document) == raw, 'roundtrip_mismatch', 'Unchanged rebuild differs from extracted source')
        # Export semantics belong to lmd.py, including controls, references and guards.
        exported = lmd.export(document, source_path)
    except lmd.LmdError as error:
        connection.execute('INSERT INTO documents VALUES (?,?,?,?,?,?,?,?,?,?,?,?)',
                           (sha, len(raw), entry_id, 'rejected', lmd.PROFILE, None,
                            encode(header), None, None, None, 0, encode(error.record())))
        return 'rejected'
    connection.execute('INSERT INTO documents VALUES (?,?,?,?,?,?,?,?,?,?,?,?)',
                       (sha, len(raw), entry_id, 'supported', lmd.PROFILE, document.internal_name,
                        encode(exported['header_u32']), len(exported['labels']), len(exported['controls']),
                        len(exported['texts']), 1, None))
    connection.executemany('INSERT INTO labels VALUES (?,?,?,?,?,?)',
        [(sha, x['ordinal'], x['offset'], x['identifier_hex'], x['control_start_index'], x['unknown_u16'])
         for x in exported['labels']])
    connection.executemany('INSERT INTO controls VALUES (?,?,?,?)',
        [(sha, x['index'], x['offset'], encode(x['words_u32'])) for x in exported['controls']])
    connection.executemany('INSERT INTO fragments VALUES (?,?,?,?,?,?,?,?,?,?,?,?)',
        [(sha, x['text_index'], x['key'], x['descriptor_offset'], x['offset'], x['length_utf16_units'],
          x['second_length_raw'], x['padding_hex'], x['source_text'], lmd.digest(x['source_text'].encode('utf-16le')),
          encode(x['protected_tokens']), encode(x['label_references'])) for x in exported['texts']])
    return 'supported'


def summary(connection):
    def scalar(sql):
        return connection.execute(sql).fetchone()[0]
    supported = scalar("SELECT COUNT(*) FROM documents WHERE status='supported'")
    rejected = [dict(zip(['sha256','entry_id','size','error'], row)) for row in connection.execute(
        "SELECT sha256,representative_entry,source_size,error_json FROM documents WHERE status='rejected' ORDER BY representative_entry")]
    for row in rejected:
        row['error'] = json.loads(row['error'])
    duplicate_groups = scalar('SELECT COUNT(*) FROM (SELECT sha256 FROM members WHERE sha256 IS NOT NULL GROUP BY sha256 HAVING COUNT(*)>1)')
    return {'english_members': scalar("SELECT COUNT(*) FROM members WHERE language_status='english'"),
            'ambiguous_members': scalar("SELECT COUNT(*) FROM members WHERE language_status='ambiguous'"),
            'extraction_failures': scalar("SELECT COUNT(*) FROM members WHERE language_status='english' AND sha256 IS NULL AND error_json IS NOT NULL"),
            'unique_payloads': scalar('SELECT COUNT(*) FROM documents'),
            'supported_unique_payloads': supported, 'rejected_unique_payloads': len(rejected),
            'supported_member_instances': scalar("SELECT COUNT(*) FROM members m JOIN documents d ON m.sha256=d.sha256 WHERE d.status='supported'"),
            'rejected_member_instances': scalar("SELECT COUNT(*) FROM members m JOIN documents d ON m.sha256=d.sha256 WHERE d.status='rejected'"),
            'exact_duplicate_payload_groups': duplicate_groups,
            'additional_exact_duplicate_member_instances': scalar('SELECT COUNT(*) FROM members WHERE duplicate_of_entry IS NOT NULL'),
            'fragments_in_unique_supported_payloads': scalar('SELECT COUNT(*) FROM fragments'),
            'fragment_instances_including_duplicate_members': scalar('SELECT COALESCE(SUM(d.fragments_count),0) FROM members m JOIN documents d ON m.sha256=d.sha256'),
            'distinct_exact_fragment_texts': scalar('SELECT COUNT(DISTINCT text_utf16_sha256) FROM fragments'),
            'empty_fragments_in_unique_payloads': scalar("SELECT COUNT(*) FROM fragments WHERE source_text=''"),
            'label_records_in_unique_payloads': scalar('SELECT COUNT(*) FROM labels'),
            'control_records_in_unique_payloads': scalar('SELECT COUNT(*) FROM controls'),
            'all_supported_unchanged_roundtrips_byte_exact': (scalar("SELECT COUNT(*) FROM documents WHERE status='supported' AND roundtrip_byte_exact!=1") == 0) if supported else None,
            'rejected': rejected,
            'limitations': ['Coverage is indexed LMD English markers, not all game text or images',
                           'Fragments are not necessarily complete messages or translation units',
                           'Equal text at different identities is retained separately',
                           'Control grammar, rendering and game-wide text coverage remain unknown',
                           'No translations generated and no runtime validation by this inventory']}


def build(index, output):
    started = time.monotonic(); before_usage = usage.getrusage(usage.RUSAGE_SELF)
    output.mkdir(parents=True, exist_ok=True)
    database = output/'catalog.sqlite'
    # Deliberately no cache shortcut: build is fresh, summary reads historical results.
    with database.open('xb'):
        pass
    source_dir = output/'source'; source_dir.mkdir(exist_ok=True)
    source_connection, source_metadata = resources.open_index(index)
    connection = sqlite3.connect(database)
    try:
        initialize(connection)
        rows = [dict(r) for r in source_connection.execute('''SELECT e.id,e.name,a.path archive
             FROM entries e JOIN archives a ON e.archive_id=a.id WHERE e.type_id=? ORDER BY e.id''', (LMD_TYPE,))]
        scopes = Counter(classify(row)[0] for row in rows)
        metadata = {'schema': SCHEMA, 'complete': False, 'producer': 'local inventory using lmd.py; not REA Evidence',
                    'source': source_metadata['source'], 'index_path': str(index.resolve()),
                    'index_sha256': lmd.digest(index.read_bytes()), 'index_metadata': source_metadata,
                    'producer_sha256': lmd.digest(Path(__file__).read_bytes()),
                    'lmd_producer_sha256': lmd.digest(Path(lmd.__file__).read_bytes()), 'profile': lmd.PROFILE,
                    'indexed_type_id': LMD_TYPE, 'indexed_lmd_members_all_languages': len(rows),
                    'language_scope_counts': dict(scopes), 'selection': 'All three markers must agree: /eng/ archive, eng\\ resource prefix, _eng suffix',
                    'whole_rom_hash_recomputed': False,
                    'deduplication': 'SHA256 of extracted decompressed payload; every selected compressed member independently verified'}
        connection.executemany('INSERT INTO metadata VALUES (?,?)', [(k, encode(v)) for k, v in metadata.items()])
        connection.commit()
        for row in rows:
            language, markers = classify(row)
            if language == 'other':
                continue
            if language == 'ambiguous':
                connection.execute('INSERT INTO members VALUES (?,?,?,?,?,?,?,?,?)',
                    (row['id'], row['archive'], row['name'], language, encode(markers), None, None, None,
                     encode({'code': 'ambiguous_language_markers', 'message': 'Language markers disagree; no natural-language guess'})))
                continue
            source_path = source_dir/f'entry-{row["id"]}.lmd'
            try:
                extraction = resources.extract(source_connection, source_metadata, row['id'], source_path)
                raw = lmd.read_bytes(source_path)
            except (ValueError, OSError, lmd.LmdError) as error:
                connection.execute('INSERT INTO members VALUES (?,?,?,?,?,?,?,?,?)',
                    (row['id'], row['archive'], row['name'], language, encode(markers), None, None, None,
                     encode({'code': 'extraction_failed', 'message': str(error)})))
                continue
            sha = lmd.digest(raw)
            existing = connection.execute('SELECT representative_entry FROM documents WHERE sha256=?', (sha,)).fetchone()
            if not existing:
                insert_document(connection, raw, row['id'], source_path)
            connection.execute('INSERT INTO members VALUES (?,?,?,?,?,?,?,?,?)',
                (row['id'], row['archive'], row['name'], language, encode(markers), sha,
                 existing[0] if existing else None, encode(extraction), None))
            connection.commit()
        after_usage = usage.getrusage(usage.RUSAGE_SELF)
        result = summary(connection)
        result['measurement'] = {'elapsed_seconds': time.monotonic()-started,
            'process_user_seconds': after_usage.ru_utime-before_usage.ru_utime,
            'process_system_seconds': after_usage.ru_stime-before_usage.ru_stime,
            'peak_process_rss_bytes': after_usage.ru_maxrss if sys.platform == 'darwin' else after_usage.ru_maxrss*1024,
            'scope': 'Completed inventory process; no child processes; cold export; operating-system file cache state unmeasured'}
        result['database'] = str(database.resolve())
        result['schema'] = SCHEMA
        connection.execute('UPDATE metadata SET value=? WHERE key=?', ('true', 'complete'))
        connection.execute('INSERT INTO metadata VALUES (?,?)', ('summary', encode(result)))
        connection.commit()
        lmd.write_new(output/'summary.json', (json.dumps(result, ensure_ascii=False, indent=2)+'\n').encode())
        return result
    finally:
        connection.close(); source_connection.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    create = commands.add_parser('build'); create.add_argument('--index', type=Path, required=True); create.add_argument('--output', type=Path, required=True)
    read = commands.add_parser('summary'); read.add_argument('--database', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == 'build':
            result = build(args.index, args.output)
        else:
            with sqlite3.connect(args.database.resolve().as_uri()+'?mode=ro', uri=True) as connection:
                metadata = {k: json.loads(v) for k, v in connection.execute('SELECT key,value FROM metadata')}
                lmd.require(metadata.get('schema') == SCHEMA and metadata.get('complete') is True,
                            'incomplete_catalog', 'Inventory must have completed with the expected schema')
                result = {**metadata['summary'], 'historical_read_only': True, 'source_reverified_now': False}
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (ValueError, OSError, sqlite3.Error, lmd.LmdError) as error:
        print(json.dumps({'ok': False, 'error': error.record() if isinstance(error, lmd.LmdError)
                         else {'code': type(error).__name__, 'message': str(error)}}))
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
