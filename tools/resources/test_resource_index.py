#!/usr/bin/env python3
"""CLI boundary checks for metadata indexing, exact selection and extraction."""
import hashlib
import json
from pathlib import Path
import sqlite3
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

TOOL = Path(__file__).with_name('index_resources.py')


class ResourceIndexTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='mh4u index á ')
        self.root=Path(self.temp.name)
        self.inspection=self.root/'inspection';self.inspection.mkdir()
        self.rom=self.root/'source.3ds';self.index=self.root/'index.sqlite'
        self.payload=b'MOD\0'+bytes(range(256))
        compressed=zlib.compress(self.payload)
        self.arc=struct.pack('<4sHHI',b'ARC\0',19,1,0)+struct.pack(
            '<64s4I',b'unit\\mesh%_',0x58a15856,len(compressed),0x40000000|len(self.payload),92)+compressed
        self.write_source(self.arc)

    def tearDown(self):self.temp.cleanup()

    def write_source(self,arc):
        data=bytes(512)+arc
        self.rom.write_bytes(data)
        (self.inspection/'manifest.json').write_text(json.dumps({'source':{
            'path':str(self.rom),'size':len(data),'sha256':hashlib.sha256(data).hexdigest()}}))
        (self.inspection/'romfs-inventory.json').write_text(json.dumps([
            {'path':'/data/test.arc','source_offset':512,'size':len(arc)}]))

    def call(self,*args,success=True):
        result=subprocess.run([sys.executable,'-B',str(TOOL),*map(str,args)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0 if success else 2,result.stderr+result.stdout)
        return json.loads(result.stdout if success else result.stderr)

    def build(self):return self.call('build','--inspection',self.inspection,'--output',self.index)

    def test_public_workflow_exact_and_idempotent(self):
        self.assertEqual(self.build()['entries'],1)
        result=self.call('find','--index',self.index,'--contains','%_','--type','rModel')
        self.assertEqual(result['total'],1)
        self.assertEqual(result['entries'][0]['name'],'unit\\mesh%_')
        self.assertEqual(self.call('find','--index',self.index,'--contains',"' OR 1=1 --")['total'],0)
        output=self.root/'model.mod'
        a=self.call('extract','--index',self.index,'--entry',1,'--output',output)
        b=self.call('extract','--index',self.index,'--entry',1,'--output',output)
        self.assertEqual(output.read_bytes(),self.payload)
        self.assertEqual(a,b)
        self.assertFalse(a['whole_rom_hash_recomputed_on_extract'])

    def test_source_digest_failure_creates_no_index(self):
        data=bytearray(self.rom.read_bytes());data[0]^=1;self.rom.write_bytes(data)
        self.call('build','--inspection',self.inspection,'--output',self.index,success=False)
        self.assertFalse(self.index.exists())

    def test_truncated_archive_is_reported_without_false_entries(self):
        self.write_source(self.arc[:-2])
        result=self.build()
        self.assertEqual((result['indexed'],result['entries']),(0,0))
        self.assertEqual(len(result['unsupported']),1)
        self.assertEqual(self.call('summary','--index',self.index)['metadata']['complete'],True)

    def test_selected_payload_change_is_detected(self):
        self.build();data=bytearray(self.rom.read_bytes());data[-1]^=1;self.rom.write_bytes(data)
        output=self.root/'changed.mod'
        self.call('extract','--index',self.index,'--entry',1,'--output',output,success=False)
        self.assertFalse(output.exists())

    def test_incomplete_index_cannot_be_queried(self):
        self.build()
        with sqlite3.connect(self.index) as connection:
            connection.execute("UPDATE metadata SET value='false' WHERE key='complete'")
        self.call('find','--index',self.index,success=False)

    def test_forged_range_and_output_conflict(self):
        self.build();output=self.root/'keep.mod';output.write_bytes(b'keep this file')
        self.call('extract','--index',self.index,'--entry',1,'--output',output,success=False)
        self.assertEqual(output.read_bytes(),b'keep this file')
        with sqlite3.connect(self.index) as connection:
            connection.execute('UPDATE entries SET source_offset=-1 WHERE id=1')
        self.call('extract','--index',self.index,'--entry',1,'--output',self.root/'bad.mod',success=False)
        self.assertFalse((self.root/'bad.mod').exists())

    def test_index_output_is_exclusive_and_limits_are_enforced(self):
        self.build();before=self.index.read_bytes()
        self.call('build','--inspection',self.inspection,'--output',self.index,success=False)
        self.assertEqual(self.index.read_bytes(),before)
        self.call('find','--index',self.index,'--limit',0,success=False)
        self.call('find','--index',self.index,'--offset',-1,success=False)
        self.call('extract','--index',self.index,'--entry',99,'--output',self.root/'missing.mod',success=False)


if __name__=='__main__':unittest.main()
