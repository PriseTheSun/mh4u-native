#!/usr/bin/env python3
"""Index the local ROM's ARC resources and extract explicitly selected entries.

No target execution. The database contains metadata/hashes, never payload bytes.
Names and raw fields are retained; unsupported archives are reported explicitly.
"""
import argparse
import hashlib
import json
import sqlite3
import sys
from pathlib import Path

from extract_model import MAX_ARC, arc_members, inflate_member

SCHEMA = 'mh4u-resource-index-v1'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def identity(stat):
    return (stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns)


def type_id(value):
    import zlib
    if value.startswith('0x'):
        result = int(value, 16)
        if not 0 <= result <= 0xffffffff:
            raise ValueError('Type ID outside uint32')
        return result
    if not value or '\0' in value:
        raise ValueError('Type name must be nonempty ASCII without NUL')
    return (~zlib.crc32(value.encode('ascii'))) & 0x7fffffff


def build(inspection, output):
    manifest_bytes = (inspection/'manifest.json').read_bytes()
    inventory_bytes = (inspection/'romfs-inventory.json').read_bytes()
    manifest = json.loads(manifest_bytes)
    if not isinstance(manifest,dict):
        raise ValueError('Inspection manifest must be an object')
    source = manifest.get('source')
    inventory = json.loads(inventory_bytes)
    if (not isinstance(source,dict) or not isinstance(source.get('path'),str)
            or not isinstance(source.get('size'),int) or source['size'] < 0
            or not isinstance(source.get('sha256'),str) or len(source['sha256']) != 64
            or any(c not in '0123456789abcdef' for c in source['sha256'])):
        raise ValueError('Invalid source identity in inspection manifest')
    if not isinstance(inventory,list) or any(
            not isinstance(v,dict) or not isinstance(v.get('path'),str)
            or not isinstance(v.get('source_offset'),int) or not isinstance(v.get('size'),int)
            for v in inventory):
        raise ValueError('Invalid RomFS inventory records')
    arcs = sorted((v for v in inventory if v['path'].endswith('.arc')), key=lambda v:v['path'])
    if len({v['path'] for v in arcs}) != len(arcs):
        raise ValueError('Ambiguous duplicate RomFS archive paths')
    rom = Path(source['path'])
    before = rom.stat()
    if before.st_size != source['size']:
        raise ValueError('ROM size differs from inspected identity')
    with rom.open('rb') as handle:
        h = hashlib.sha256()
        for block in iter(lambda: handle.read(4*1024*1024), b''):
            h.update(block)
        if h.hexdigest() != source['sha256']:
            raise ValueError('ROM digest differs from inspection; re-inspect the selected source')
        if identity(rom.stat()) != identity(before):
            raise ValueError('ROM changed during full identity verification')
        with output.open('xb'):
            pass
        connection = sqlite3.connect(output)
        try:
            connection.executescript('''
                CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);
                CREATE TABLE archives (
                  id INTEGER PRIMARY KEY, path TEXT NOT NULL UNIQUE,
                  source_offset INTEGER NOT NULL, size INTEGER NOT NULL,
                  sha256 TEXT, version INTEGER, unknown_header INTEGER,
                  status TEXT NOT NULL, reason TEXT, entry_count INTEGER);
                CREATE TABLE entries (
                  id INTEGER PRIMARY KEY, archive_id INTEGER NOT NULL REFERENCES archives(id),
                  ordinal INTEGER NOT NULL, name TEXT NOT NULL, raw_name_hex TEXT NOT NULL,
                  type_id INTEGER NOT NULL, compressed_size INTEGER NOT NULL,
                  uncompressed_size INTEGER NOT NULL, raw_size_flags INTEGER NOT NULL,
                  archive_offset INTEGER NOT NULL, source_offset INTEGER NOT NULL,
                  compressed_sha256 TEXT NOT NULL, UNIQUE(archive_id,ordinal));
            ''')
            meta = {'schema':SCHEMA,'complete':False,'source':source,
                    'source_stat':{'device':before.st_dev,'inode':before.st_ino,
                                   'mtime_ns':before.st_mtime_ns,'size':before.st_size},
                    'manifest_sha256':digest(manifest_bytes),'inventory_sha256':digest(inventory_bytes),
                    'whole_rom_sha256_verified':True,
                    'producer':'index_resources.py; local format analysis, not REA Evidence',
                    'coverage':'Every .arc path in supplied RomFS inventory; every supported ARC directory and compressed payload hashed; no member decompressed'}
            connection.executemany('INSERT INTO metadata VALUES (?,?)',
                                   [(k,json.dumps(v)) for k,v in meta.items()])
            connection.commit()
            for archive_id, arc in enumerate(arcs,1):
                raw = None
                try:
                    offset, size = arc['source_offset'], arc['size']
                    if not isinstance(offset,int) or not isinstance(size,int) or offset < 0 or not 12 <= size <= MAX_ARC or offset+size > before.st_size:
                        raise ValueError('Archive range is invalid or exceeds the 128 MiB bound')
                    handle.seek(offset)
                    raw = handle.read(size)
                    if len(raw) != size:
                        raise ValueError('Incomplete ARC read')
                    parsed = arc_members(raw)
                    ranges = sorted((e['offset'],e['compressed_size']) for e in parsed['entries'])
                    if any(a+n>b for (a,n),(b,_) in zip(ranges,ranges[1:])):
                        raise ValueError('Overlapping compressed payloads')
                    rows = [(archive_id,e['index'],e['name'],e['raw_name_hex'],int(e['kind'],16),
                             e['compressed_size'],e['expected_size'],e['size_flags_raw'],e['offset'],
                             offset+e['offset'],digest(raw[e['offset']:e['offset']+e['compressed_size']]))
                            for e in parsed['entries']]
                except (ValueError,UnicodeError) as error:
                    connection.execute('INSERT INTO archives VALUES (?,?,?,?,?,?,?,?,?,?)',
                                       (archive_id,arc['path'],arc['source_offset'],arc['size'],
                                        digest(raw) if raw is not None else None,None,None,'unsupported',str(error),None))
                    continue
                connection.execute('INSERT INTO archives VALUES (?,?,?,?,?,?,?,?,?,?)',
                                   (archive_id,arc['path'],offset,size,digest(raw),parsed['version'],
                                    parsed['header_unknown_raw'],'indexed',None,len(rows)))
                connection.executemany('''INSERT INTO entries
                   (archive_id,ordinal,name,raw_name_hex,type_id,compressed_size,uncompressed_size,
                    raw_size_flags,archive_offset,source_offset,compressed_sha256)
                   VALUES (?,?,?,?,?,?,?,?,?,?,?)''',rows)
            if identity(rom.stat()) != identity(before):
                raise ValueError('ROM changed while indexing; incomplete database retained')
            connection.executescript('''CREATE INDEX entry_names ON entries(name);
                CREATE INDEX entry_types ON entries(type_id);''')
            connection.execute('UPDATE metadata SET value=? WHERE key=?',('true','complete'))
            connection.commit()
            print(json.dumps(summary(connection),indent=2))
        finally:
            connection.close()


def open_index(path):
    connection = sqlite3.connect(path.resolve().as_uri()+'?mode=ro',uri=True)
    connection.row_factory = sqlite3.Row
    try:
        meta = {r[0]:json.loads(r[1]) for r in connection.execute('SELECT key,value FROM metadata')}
        if meta.get('schema') != SCHEMA or meta.get('complete') is not True:
            raise ValueError('Unsupported or incomplete resource index')
    except BaseException:
        connection.close()
        raise
    return connection,meta


def summary(connection):
    return {'archives':connection.execute('SELECT COUNT(*) FROM archives').fetchone()[0],
            'indexed':connection.execute("SELECT COUNT(*) FROM archives WHERE status='indexed'").fetchone()[0],
            'entries':connection.execute('SELECT COUNT(*) FROM entries').fetchone()[0],
            'unsupported':[tuple(r) for r in connection.execute("SELECT path,reason FROM archives WHERE status!='indexed'")]}


def extract(connection,meta,entry_id,output):
    row = connection.execute('''SELECT e.*,a.path archive,a.size archive_size,a.source_offset archive_source_offset
                                FROM entries e JOIN archives a ON a.id=e.archive_id WHERE e.id=?''',(entry_id,)).fetchone()
    if row is None:
        raise ValueError('No entry with that ID in this index')
    source = Path(meta['source']['path'])
    before = source.stat()
    offset,size = row['source_offset'],row['compressed_size']
    if before.st_size != meta['source']['size'] or offset < 0 or not 0 < size <= MAX_ARC or offset+size > before.st_size:
        raise ValueError('Selected entry range or source size is invalid')
    if row['archive_offset'] < 12 or row['archive_offset']+size > row['archive_size'] or offset != row['archive_source_offset']+row['archive_offset']:
        raise ValueError('Entry escapes its declared archive')
    with source.open('rb') as handle:
        handle.seek(offset)
        compressed = handle.read(size)
    if identity(source.stat()) != identity(before) or digest(compressed) != row['compressed_sha256']:
        raise ValueError('Selected compressed bytes changed or disagree with index')
    decoded = inflate_member(compressed,{'expected_size':row['uncompressed_size'],
                                        'offset':0,'compressed_size':size})
    if output.exists():
        if output.stat().st_size != len(decoded) or output.read_bytes() != decoded:
            raise ValueError('Refusing to replace different existing output')
    else:
        with output.open('xb') as handle:
            handle.write(decoded)
    return {'entry':dict(row),'output':str(output.resolve()),'size':len(decoded),'sha256':digest(decoded),
            'source_sha256_from_index':meta['source']['sha256'],
            'whole_rom_hash_recomputed_on_extract':False,'selected_compressed_hash_verified':True}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    commands=parser.add_subparsers(dest='command',required=True)
    b=commands.add_parser('build');b.add_argument('--inspection',type=Path,required=True);b.add_argument('--output',type=Path,required=True)
    for name in ('summary','find','types','extract'):
        p=commands.add_parser(name);p.add_argument('--index',type=Path,required=True)
        if name=='find':
            p.add_argument('--name',help='Exact internal resource path')
            p.add_argument('--contains',help='Literal substring, case sensitive')
            p.add_argument('--archive',help='Exact RomFS archive path')
            p.add_argument('--type',dest='kind',help='Observed type name or 0x-prefixed numeric ID')
            p.add_argument('--offset',type=int,default=0);p.add_argument('--limit',type=int,default=40)
        if name=='extract':
            p.add_argument('--entry',type=int,required=True);p.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.command=='build':
        return build(args.inspection,args.output)
    connection,meta=open_index(args.index)
    try:
        if args.command=='summary':result={**summary(connection),'metadata':meta}
        elif args.command=='types':
            result=[dict(r) for r in connection.execute('SELECT type_id,COUNT(*) count,MIN(name) example FROM entries GROUP BY type_id ORDER BY count DESC,type_id')]
        elif args.command=='extract':result=extract(connection,meta,args.entry,args.output)
        else:
            if not 1 <= args.limit <= 1000 or args.offset < 0:raise ValueError('limit must be 1..1000 and offset nonnegative')
            filters=[];values=[]
            for key,value in [('e.name',args.name),('a.path',args.archive),('e.type_id',type_id(args.kind) if args.kind else None)]:
                if value is not None:filters.append(key+'=?');values.append(value)
            if args.contains is not None:filters.append('instr(e.name,?)>0');values.append(args.contains)
            where=' WHERE '+' AND '.join(filters) if filters else ''
            query=' FROM entries e JOIN archives a ON a.id=e.archive_id'+where
            total=connection.execute('SELECT COUNT(*)'+query,values).fetchone()[0]
            rows=connection.execute('SELECT e.id,e.name,e.type_id,e.uncompressed_size,a.path archive'+query+' ORDER BY e.id LIMIT ? OFFSET ?',values+[args.limit,args.offset])
            result={'total':total,'offset':args.offset,'limit':args.limit,'entries':[dict(r) for r in rows]}
        print(json.dumps(result,ensure_ascii=False,indent=2))
    finally:connection.close()


if __name__=='__main__':
    try:main()
    except (ValueError,OSError,sqlite3.Error) as error:
        print(json.dumps({'error':str(error)}),file=sys.stderr)
        sys.exit(2)
