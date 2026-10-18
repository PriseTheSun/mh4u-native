#!/usr/bin/env python3
"""Exercise the native glTF/texture-pack boundary through the viewer CLI."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib
from texture_pack import create


def png(color, width=2, height=2):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', width, height, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress((b'\0' + bytes(color) * 2) * 2)) + chunk(b'IEND', b''))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--binary', required=True, type=Path)
    ap.add_argument('--report', type=Path)
    args = ap.parse_args()
    checks = []
    with tempfile.TemporaryDirectory(prefix='mh4u-texture-pack-') as directory:
        root = Path(directory)
        for name, color in [('red', (255, 0, 0, 255)), ('blue', (0, 0, 255, 255)), ('green', (0, 255, 0, 255))]:
            (root / (name + '.png')).write_bytes(png(color))
        (root / 'mesh.bin').write_bytes(struct.pack('<24f', 0, 0, 0, 1, 0, 0, 0, 1, 0,
                                                   0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1))
        scene = {'asset': {'version': '2.0'}, 'buffers': [{'uri': 'mesh.bin', 'byteLength': 96}],
            'bufferViews': [{'buffer': 0, 'byteOffset': off, 'byteLength': length} for off, length in [(0,36),(36,36),(72,24)]],
            'accessors': [{'bufferView': i, 'componentType': 5126, 'type': shape, 'count': 3} for i, shape in enumerate(['VEC3','VEC3','VEC2'])],
            'images': [{'uri': 'red.png'}, {'uri': 'blue.png'}], 'textures': [{'source': 0}, {'source': 1}],
            'materials': [{'pbrMetallicRoughness': {'baseColorTexture': {'index': i}}} for i in range(2)],
            'meshes': [{'primitives': [{'attributes': {'POSITION':0,'NORMAL':1,'TEXCOORD_0':2}, 'material': i} for i in range(2)]}],
            'nodes': [{'mesh':0}], 'scenes': [{'nodes':[0]}], 'scene':0}
        (root / 'scene.gltf').write_text(json.dumps(scene))
        manifest = create('Diagnóstico ç', [(root/'red.png', root/'green.png')], root/'pack')
        good = manifest.read_bytes()
        def check(name, data, success, expected=None):
            manifest.write_bytes(data)
            p = subprocess.run([str(args.binary.resolve()), '--model', str(root/'scene.gltf'),
                                '--texture-pack', str(manifest), '--inspect-only'], capture_output=True, text=True, timeout=15)
            assert (p.returncode == 0) == success, (name, p.stdout, p.stderr)
            if expected: assert expected in p.stdout, (name, p.stdout)
            checks.append({'name':name,'exit_code':p.returncode,'stdout':p.stdout,'stderr':p.stderr})
        check('partial match with original fallback', good, True, 'MATCHED_IMAGES 1 TOTAL_IMAGES 2')
        source_hash=hashlib.sha256((root/'red.png').read_bytes()).hexdigest().encode()
        check('unmatched pack retains originals', good.replace(source_hash,b'0'*64), True, 'MATCHED_IMAGES 0 TOTAL_IMAGES 2')
        row=good.splitlines()[2]
        check('duplicate source identity',good+row+b'\n',False)
        check('unknown version',good.replace(b'PACK 1',b'PACK 2'),False)
        check('invalid UTF-8 label',good.replace('ç'.encode(),b'\xff'),False)
        check('truncated manifest',good[:-1],False)
        parts=row.split(b'\t')
        def row_with(field,value):
            values=parts.copy();values[field]=value
            return b'\n'.join(good.splitlines()[:2])+b'\n'+b'\t'.join(values)+b'\n'
        check('replacement digest mismatch',row_with(2,b'0'*64),False)
        check('replacement size mismatch',row_with(3,str(int(parts[3])+1).encode()),False)
        check('path traversal',row_with(4,b'../green.png'),False)
        (root/'pack/outside.png').symlink_to(root/'green.png')
        check('symlink outside pack',row_with(4,b'outside.png'),False)
        bomb=png((0,255,0,255),65535,65535);(root/'pack/bomb.png').write_bytes(bomb)
        bparts=parts.copy();bparts[2]=hashlib.sha256(bomb).hexdigest().encode();bparts[3]=str(len(bomb)).encode();bparts[4]=b'bomb.png'
        check('decoded allocation rejected before PNG decoding',b'\n'.join(good.splitlines()[:2])+b'\n'+b'\t'.join(bparts)+b'\n',False)
        # The producer must preserve a previously built pack instead of overwriting it.
        try: create('replacement',[(root/'red.png',root/'blue.png')],root/'pack')
        except FileExistsError: pass
        else: raise AssertionError('Existing pack overwritten')
    result={'checks':checks,'passed':len(checks),'existing_pack_preserved':True}
    if args.report: args.report.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print(f'Texture pack: {len(checks)} native CLI cases and existing-output preservation passed')


if __name__ == '__main__': main()
