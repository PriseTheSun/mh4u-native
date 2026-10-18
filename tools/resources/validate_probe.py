#!/usr/bin/env python3
"""Validate the selected local assets using a pinned historical RGB decoder.

Standard library only. Requires an explicitly supplied local reference file;
does not download, import Blender, execute the game, or install dependencies.
"""
import argparse
import array
import ast
import hashlib
import json
from pathlib import Path
import struct
import sys

import extract_model as probe

REFERENCE = json.loads(Path(__file__).with_name('reference.lock.json').read_text())


class ReferenceImage:
    def __init__(self, width, height):
        self.size = (width, height)
        self.depth = 32

    def update(self):
        pass

    def pack(self, value):
        pass


def reference_decoder(path):
    source = path.read_bytes()
    if hashlib.sha256(source).hexdigest() != REFERENCE['sha256']:
        raise ValueError('Reference source SHA-256 differs from the reviewed pin')
    if sys.byteorder != 'little' or array.array('I').itemsize != 4:
        raise ValueError('Historical reference requires little-endian 32-bit unsigned int')
    tree = ast.parse(source)
    selected = [node for node in tree.body
                if (isinstance(node, ast.FunctionDef) and node.name == 'decode_etc1')
                or (isinstance(node, ast.Assign) and any(
                    isinstance(target, ast.Name) and target.id == 'modifier_tables'
                    for target in node.targets))]
    if len(selected) != 2:
        raise ValueError('Reference AST selection differs from the reviewed source')
    scope = {'array': array}
    # Only these two reviewed AST nodes execute; bpy and other top-level code do not.
    exec(compile(ast.Module(body=selected, type_ignores=[]), REFERENCE['url'], 'exec'), scope)
    return scope['decode_etc1']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', type=Path, required=True, help='Directory from extract_model.py --output')
    parser.add_argument('--reference', type=Path, required=True, help='Local, unchanged reference at the pinned SHA-256')
    parser.add_argument('--output', type=Path, required=True, help='Explicit JSON report path; differing existing files are refused')
    args = parser.parse_args()
    assets = args.assets.resolve()
    decode_reference = reference_decoder(args.reference)
    checks = []

    def check(name, passed):
        if not passed:
            raise ValueError('Validation failed: ' + name)
        checks.append({'check': name, 'passed': True})

    def rejects(name, action):
        try:
            action()
        except (ValueError, struct.error):
            check(name, True)
            return
        raise ValueError(name + ' unexpectedly accepted')

    manifest_bytes = (assets/'manifest.json').read_bytes()
    manifest = json.loads(manifest_bytes)
    for record in manifest['outputs']:
        path = Path(record['path'])
        if path.parent != assets:
            raise ValueError('Manifest output path is outside the selected asset directory')
        data = path.read_bytes()
        if len(data) != record['size'] or probe.sha(data) != record['sha256']:
            raise ValueError('Manifest size/digest mismatch: ' + path.name)
    check('All manifest output lengths and SHA-256 digests match', True)

    texture = (assets/'one133_BM.tex').read_bytes()
    texture_info, _, _ = probe.parse_tex(texture)
    width, height = texture_info['width'], texture_info['height']
    if texture_info['pixel_format'] != 12:
        raise ValueError('This reference comparison expects ETC1A4')
    offset = texture_info['pixel_data_base'] + texture_info['mipmap_offsets_raw'][0]
    image = ReferenceImage(width, height)
    decode_reference(image, probe.region(texture, offset, width*height, 'reference base mip'))
    reference = bytearray()
    for y in range(height):
        for x in range(width):
            pixel = ((height-1-y)*width+x)*4
            reference.extend(round(channel*255) for channel in image.pixels[pixel:pixel+3])
    expected_ppm = f'P6\n{width} {height}\n255\n'.encode() + reference
    check(f'ETC1 RGB equals pinned reference on all {width*height} pixels',
          (assets/'one133_BM.ppm').read_bytes() == expected_ppm)

    for name in ('one133', 'sld133'):
        model = probe.parse_mod((assets/f'{name}.mod').read_bytes())
        check(name + ' total polygons equal decoded nondegenerate triangles',
              sum(mesh['triangle_count'] for mesh in model['meshes']) == model['header_words'][6])
        check(name + ' total vertices equal primitive vertex sum',
              sum(mesh['vertex_count'] for mesh in model['meshes']) == model['header_words'][4])
        check(name + ' OBJ exactly re-exports selected primitive',
              (assets/f'{name}.obj').read_bytes() == probe.obj(model['meshes'][0]).encode())

    rejects('truncated MOD header', lambda: probe.parse_mod(b'MOD\0'))
    model_bytes = (assets/'one133.mod').read_bytes()
    bad = bytearray(model_bytes)
    struct.pack_into('<I', bad, 0x38, len(bad)+1)
    rejects('vertex buffer beyond file', lambda: probe.parse_mod(bad))
    bad = bytearray(model_bytes)
    offset = struct.unpack_from('<I', bad, 0x3c)[0]
    struct.pack_into('<H', bad, offset, 60000)
    rejects('primitive index out of range', lambda: probe.parse_mod(bad))
    bad = bytearray(model_bytes)
    primitive = struct.unpack_from('<I', bad, 0x34)[0]
    struct.pack_into('<I', bad, primitive+0x1c, 0xffffffff)
    rejects('oversized strip extent rejected before allocation', lambda: probe.parse_mod(bad))
    rejects('truncated TEX data', lambda: probe.parse_tex(texture[:-20000]))
    bad = bytearray(texture)
    bad[13] = 99
    rejects('unsupported TEX pixel format', lambda: probe.parse_tex(bad))
    archive = (assets/'archive.arc').read_bytes()
    entry = probe.arc_members(archive)['entries'][0].copy()
    entry['expected_size'] -= 1
    rejects('zlib declared-size mismatch', lambda: probe.inflate_member(archive, entry))

    report = {
        'scope': 'Static decoding of selected local assets; no game runtime',
        'assets': str(assets),
        'manifest_sha256': probe.sha(manifest_bytes),
        'reference': {'url': REFERENCE['url'], 'source_sha256': REFERENCE['sha256'],
                      'license': REFERENCE['license_from_file_header'],
                      'copyright': REFERENCE['copyright_from_file_header']},
        'checks': checks,
        'unverified': ['Alpha channel not independently decoded', 'Material shader and alpha semantics',
                       'World transforms, physical units and skeletal pose',
                       'Correctness against frames rendered by the original game']}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    probe.save(args.output, json.dumps(report, indent=2)+'\n')
    print(json.dumps({'passed': len(checks), 'report': str(args.output.resolve())}))


if __name__ == '__main__':
    main()
