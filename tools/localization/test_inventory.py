"""Synthetic ARC -> resource index -> inventory CLI tests; no ROM fixtures."""
import contextlib
import io
import json
from pathlib import Path
import sqlite3
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

import inventory
import lmd


def payload(text, variant=0x1010b):
    encoded = text.encode('utf-16le')
    strings = encoded + b'\0\0'
    strings += b'\0' * (-len(strings) % 4)
    return (struct.pack('<9I', 0x646d6c, variant, 1, 2, 1, 36, 44, 76, 88+len(strings))
            + struct.pack('<IHH', 0x12345678, 0, 5)
            + struct.pack('<8I', 1, 0, 0, 0, 0x9f, 7, 8, 9)
            + struct.pack('<3I', 88, len(encoded)//2, len(encoded)//2)
            + strings + b'IdenticalName\0')


def archive(members):
    body = bytearray(); directory = bytearray(); start = 12+80*len(members)
    for name, raw in members:
        compressed = zlib.compress(raw)
        directory.extend(struct.pack('<64s4I', name.encode('ascii'), inventory.LMD_TYPE,
                                     len(compressed), len(raw), start+len(body)))
        body.extend(compressed)
    return struct.pack('<4sHHI', b'ARC\0', 19, len(members), 0)+directory+body


class InventoryCliTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.source = self.base/'fixture.rom'
        self.index = self.base/'resources.sqlite'
        original = payload('Keep %s\n\ue000')
        eng = archive([('eng\\msg\\One_eng', original), ('eng\\msg\\Two_eng', original),
                       ('eng\\msg\\Three_eng', payload('\u00e9')),
                       ('eng\\msg\\Four_eng', payload('e\u0301')),
                       ('eng\\msg\\Rejected_eng', payload('Unverified', 0x1010c)),
                       ('fre\\msg\\Conflict_fre', original)])
        fre = archive([('fre\\msg\\One_fre', original)])
        raw = eng+fre
        self.source.write_bytes(raw)
        (self.base/'manifest.json').write_text(json.dumps({'source': {
            'path': str(self.source), 'size': len(raw), 'sha256': lmd.digest(raw)}}))
        (self.base/'romfs-inventory.json').write_text(json.dumps([
            {'path': '/eng/test.arc', 'source_offset': 0, 'size': len(eng)},
            {'path': '/fre/test.arc', 'source_offset': len(eng), 'size': len(fre)}]))
        with contextlib.redirect_stdout(io.StringIO()):
            inventory.resources.build(self.base, self.index)

    def cli(self, *args, code=0):
        result = subprocess.run([sys.executable, '-B', str(Path(inventory.__file__)), *map(str, args)],
                                capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, code, result.stdout+result.stderr)
        return json.loads(result.stdout)

    def build(self, folder='catalog'):
        out = self.base/folder
        return out, self.cli('build', '--index', self.index, '--output', out)

    def test_public_pipeline_preserves_identity_controls_and_exact_duplicates(self):
        out, result = self.build()
        self.assertEqual((result['english_members'], result['ambiguous_members'], result['extraction_failures']), (5, 1, 0))
        self.assertEqual((result['unique_payloads'], result['supported_unique_payloads'], result['rejected_unique_payloads']), (4, 3, 1))
        self.assertEqual((result['supported_member_instances'], result['rejected_member_instances']), (4, 1))
        self.assertEqual((result['exact_duplicate_payload_groups'], result['additional_exact_duplicate_member_instances']), (1, 1))
        self.assertEqual((result['fragments_in_unique_supported_payloads'], result['fragment_instances_including_duplicate_members'], result['distinct_exact_fragment_texts']), (3, 4, 3))
        self.assertTrue(result['all_supported_unchanged_roundtrips_byte_exact'])
        with sqlite3.connect(out/'catalog.sqlite') as connection:
            self.assertEqual(connection.execute('pragma integrity_check').fetchone()[0], 'ok')
            self.assertEqual(connection.execute('pragma foreign_key_check').fetchall(), [])
            self.assertEqual(connection.execute('select count(distinct original_key) from fragments').fetchone()[0], 1)
            self.assertEqual(connection.execute('select count(distinct internal_name) from documents').fetchone()[0], 1)
            texts = {r[0] for r in connection.execute('select source_text from fragments')}
            self.assertEqual(texts, {'Keep %s\n\ue000', '\u00e9', 'e\u0301'})
            protected, references = connection.execute('select protected_tokens_json,label_references_json from fragments where source_text=?', ('Keep %s\n\ue000',)).fetchone()
            self.assertEqual(json.loads(protected), {'control_codepoints': [10, 0xe000], 'possible_placeholders': ['%s']})
            self.assertEqual(json.loads(references), [{'label_ordinal': 0, 'identifier_hex': '12345678', 'label_unknown_u16': 5, 'control_index': 0}])
            unknowns = connection.execute('select distinct words_json from controls where control_index=1').fetchall()
            self.assertEqual(unknowns, [('[159,7,8,9]',)])
            rejected = connection.execute("select error_json,header_json from documents where status='rejected'").fetchone()
            self.assertEqual(json.loads(rejected[0])['code'], 'unsupported_variant')
            self.assertEqual(json.loads(rejected[1])[1], 0x1010c)
            for entry, extraction in connection.execute('select entry_id,extraction_json from members where sha256 is not null'):
                extraction = json.loads(extraction)
                self.assertTrue(extraction['selected_compressed_hash_verified'])
                self.assertFalse(extraction['whole_rom_hash_recomputed_on_extract'])
                self.assertEqual(extraction['sha256'], lmd.digest((out/'source'/f'entry-{entry}.lmd').read_bytes()))
        self.assertEqual(len(list((out/'source').glob('*.lmd'))), 5)

    def test_summary_is_historical_and_build_refuses_clobber(self):
        out, result = self.build()
        before = (out/'catalog.sqlite').read_bytes()
        result = self.cli('summary', '--database', out/'catalog.sqlite')
        self.assertTrue(result['historical_read_only'])
        self.assertFalse(result['source_reverified_now'])
        self.cli('build', '--index', self.index, '--output', out, code=2)
        self.assertEqual((out/'catalog.sqlite').read_bytes(), before)
        with sqlite3.connect(out/'catalog.sqlite') as connection:
            connection.execute("update metadata set value='false' where key='complete'")
        failed = self.cli('summary', '--database', out/'catalog.sqlite', code=2)
        self.assertEqual(failed['error']['code'], 'incomplete_catalog')

    def test_changed_compressed_member_is_retained_as_extraction_failure(self):
        with sqlite3.connect(self.index) as connection:
            offset = connection.execute('select source_offset from entries where id=1').fetchone()[0]
        data = bytearray(self.source.read_bytes()); data[offset] ^= 1; self.source.write_bytes(data)
        out, result = self.build()
        self.assertEqual(result['extraction_failures'], 1)
        self.assertEqual(result['ambiguous_members'], 1)
        self.assertEqual(result['supported_member_instances'], 3)
        self.assertFalse((out/'source'/'entry-1.lmd').exists())
        with sqlite3.connect(out/'catalog.sqlite') as connection:
            error, sha = connection.execute('select error_json,sha256 from members where entry_id=1').fetchone()
            self.assertEqual(json.loads(error)['code'], 'extraction_failed')
            self.assertIsNone(sha)

    def test_empty_supported_coverage_never_claims_roundtrip_success(self):
        connection = sqlite3.connect(':memory:')
        self.addCleanup(connection.close)
        inventory.initialize(connection)
        self.assertIsNone(inventory.summary(connection)['all_supported_unchanged_roundtrips_byte_exact'])


if __name__ == '__main__':
    unittest.main()
