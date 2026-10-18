#!/usr/bin/env python3
"""Read-only MH4U ARC/MOD/TEX probe. Standard library only; no game execution.

Format facts: svanheulen/mhff Wiki MH4U-rArchive/rModel/rTexture-Format;
vertex layout corroborated by its n3ds/io_import_scene_mod.py.
This intentionally supports only the observed MOD v230, type-4/28-byte vertices,
single-texture 2D ETC1/ETC1A4 assets. Unparsed fields remain in the JSON.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib

MODEL = 0x58A15856
TEXTURE = 0x241F5DEB
MATERIAL = 0x2749C8A8
MAX_MEMBER = 64 * 1024 * 1024
MAX_ARC = 128 * 1024 * 1024
MODIFIERS = ((2,8,-2,-8),(5,17,-5,-17),(9,29,-9,-29),(13,42,-13,-42),
             (18,60,-18,-60),(24,80,-24,-80),(33,106,-33,-106),(47,183,-47,-183))


def sha(data):
    return hashlib.sha256(data).hexdigest()


def region(data, offset, length, label):
    if not isinstance(offset, int) or not isinstance(length, int) or offset < 0 or length < 0 or offset > len(data) or length > len(data)-offset:
        raise ValueError(f'{label}: out of bounds offset={offset} length={length} size={len(data)}')
    return data[offset:offset+length]


def unpack(fmt, data, offset, label):
    return struct.unpack(fmt, region(data, offset, struct.calcsize(fmt), label))


def save(path, data):
    if isinstance(data, str):
        data = data.encode('utf-8')
    if path.exists():
        if path.read_bytes() != data:
            raise ValueError(f'Refusing to replace different existing output: {path}')
    else:
        with path.open('xb') as handle:
            handle.write(data)
    return {'path': str(path.resolve()), 'size': len(data), 'sha256': sha(data)}


def arc_members(data):
    magic, version, count, unknown = unpack('<4sHHI', data, 0, 'ARC header')
    if magic != b'ARC\0' or version != 19:
        raise ValueError(f'Unsupported ARC header {magic!r}/{version}')
    region(data, 12, count*80, 'ARC directory')
    entries = []
    for i in range(count):
        raw_name, kind, compressed, size_flags, offset = unpack('<64s4I',data,12+i*80,'ARC record')
        name = raw_name.split(b'\0',1)[0].decode('ascii',errors='strict')
        expected = size_flags & 0x0fffffff
        region(data, offset, compressed, 'ARC payload')
        if offset < 12+count*80:
            raise ValueError('ARC payload overlaps directory')
        entries.append({'index':i,'name':name,'raw_name_hex':raw_name.hex(),'kind':f'{kind:08x}',
                        'compressed_size':compressed,'expected_size':expected,'size_flags_raw':size_flags,
                        'offset':offset})
    return {'version':version,'header_unknown_raw':unknown,'entries':entries}


def inflate_member(arc, entry):
    if entry['expected_size'] > MAX_MEMBER:
        raise ValueError('Member exceeds explicit 64 MiB probe bound')
    compressed=region(arc,entry['offset'],entry['compressed_size'],'compressed member')
    decoder=zlib.decompressobj()
    result=decoder.decompress(compressed,entry['expected_size']+1)
    if len(result)!=entry['expected_size'] or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise ValueError('Invalid zlib stream, trailing bytes, or declared-size mismatch')
    return result


def triangles(indices, start, count):
    window=[]
    parity=0
    result=[]
    degenerates=0
    restarts=0
    for index in indices:
        if index==0xffff:
            window=[]; parity=0; restarts+=1
            continue
        local=index-start
        if not 0 <= local < count:
            raise ValueError(f'Triangle strip index {index} outside selected vertex span')
        window.append(local)
        if len(window)<3:
            continue
        a,b,c=window[-3:]
        face=(a,b,c) if parity%2==0 else (a,c,b)
        parity+=1
        if len(set(face))==3:
            result.append(face)
        else:
            degenerates+=1
    return result, {'restart_count':restarts,'degenerate_triangles_omitted':degenerates}


def parse_mod(data):
    h=unpack('<4s4H13I',data,0,'MOD header')
    if h[0]!=b'MOD\0' or h[1]!=230:
        raise ValueError('Unsupported MOD magic/version')
    names=[]
    for i in range(h[4]):
        names.append(region(data,h[14]+i*128,128,'material name').split(b'\0',1)[0].decode('ascii'))
    region(data,h[16],h[8],'vertex buffer')
    meshes=[]
    for i in range(h[3]):
        info=unpack('<HHIHBB9I',data,h[15]+48*i,'primitive')
        size=info[4]
        if size!=28 or info[5]&63!=4:
            raise ValueError(f'Unsupported vertex layout: size={size}, type={info[5]&63}')
        offset=h[16]+info[6]*size+info[7]
        raw_vertices=region(data,offset,info[1]*size,'vertices')
        if offset < h[16] or offset+len(raw_vertices)>h[16]+h[8]:
            raise ValueError('Primitive vertices escape declared vertex buffer')
        vertices=[];uvs=[];normals=[]
        for v in range(info[1]):
            row=region(raw_vertices,v*size,size,'vertex')
            pos=unpack('<3f',row,0,'position');uv=unpack('<2f',row,16,'UV')
            if not all(math.isfinite(x) for x in pos+uv):
                raise ValueError('Non-finite vertex/UV')
            vertices.append(pos);uvs.append(uv)
            normals.append(tuple(max(-1.0,n/127.0) for n in unpack('<3b',row,12,'normal')))
        # Validate byte extent before allocating an index collection from a file count.
        raw_indices=region(data,h[17]+2*info[9],2*(info[10]+1),'triangle strip')
        indices=[index for (index,) in struct.iter_unpack('<H',raw_indices)]
        faces,stats=triangles(indices,info[6],info[1])
        material=(info[2]>>12)&0xfff
        if material>=len(names):
            raise ValueError('Material index outside material table')
        meshes.append({'primitive_index':i,'record_raw_hex':region(data,h[15]+48*i,48,'primitive raw').hex(),
                       'record_words':list(info),'material_index':material,'material_name':names[material],
                       'vertex_buffer_source_offset':offset,'vertex_count':len(vertices),
                       'index_buffer_source_offset':h[17]+2*info[9],'index_count_read':len(indices),
                       'triangle_count':len(faces),'positions':vertices,'uvs':uvs,'normals':normals,
                       'triangles':faces,'strip_statistics':stats})
    return {'version':h[1],'joint_count':h[2],'primitive_count':h[3],'material_names':names,
            'header_raw_hex':region(data,0,128,'extended header').hex(),
            'header_words':list(h[1:]),'source_bbox_candidate_floats':list(unpack('<12f',data,64,'bounds')),
            'meshes':meshes}


def decode_etc_block(raw, alpha):
    # 3DS stores the two ETC words little-endian; pixel bits are column-major.
    alpha_bits=int.from_bytes(raw[:8],'little') if alpha else (1<<64)-1
    lo,hi=struct.unpack('<II',raw[8:] if alpha else raw)
    if hi&2:
        a=[(hi>>s)&31 for s in (27,19,11)]
        delta=[(hi>>s)&7 for s in (24,16,8)]
        b=[v+(d if d<4 else d-8) for v,d in zip(a,delta)]
        if any(v<0 or v>31 for v in b):
            raise ValueError('ETC differential value outside ETC1 range (ETC2 unsupported)')
        a=[(v<<3)|(v>>2) for v in a];b=[(v<<3)|(v>>2) for v in b]
    else:
        a=[((hi>>s)&15)*17 for s in (28,20,12)]
        b=[((hi>>s)&15)*17 for s in (24,16,8)]
    table_a=(hi>>5)&7;table_b=(hi>>2)&7;flip=bool(hi&1)
    pixels=[]
    for y in range(4):
        for x in range(4):
            bit=x*4+y
            selector=((lo>>bit)&1)|(((lo>>(16+bit))&1)<<1)
            first=(y<2) if flip else (x<2)
            modifier=MODIFIERS[table_a if first else table_b][selector]
            rgb=tuple(max(0,min(255,c+modifier)) for c in (a if first else b))
            pixels.append(rgb+(((alpha_bits>>(bit*4))&15)*17,))
    return pixels


def parse_tex(data):
    magic,flags,dimensions,format_word=unpack('<4s3I',data,0,'TEX header')
    width=(dimensions>>6)&8191;height=(dimensions>>19)&8191;mips=dimensions&63
    texture_count=format_word&255;kind=(format_word>>8)&255
    # Bits28..31 are 2 for these ordinary 2D textures. Other dimensions unsupported.
    if magic!=b'TEX\0' or flags&4095!=165 or flags>>28!=2 or texture_count!=1 or kind not in (11,12):
        raise ValueError('Unsupported TEX version/dimension/count/format')
    if not mips or not width or not height or width%8 or height%8 or width*height>16777216:
        raise ValueError('Unsupported/bounded TEX dimensions')
    offsets=unpack('<'+'I'*mips,data,16,'mipmap offsets')
    base=16+4*mips
    stride=16 if kind==12 else 8
    length=width*height//16*stride
    level=region(data,base+offsets[0],length,'base mipmap')
    if mips>1 and offsets[1]-offsets[0]!=length:
        raise ValueError('Base mip length inconsistent with next offset')
    rgb=bytearray(width*height*3);a=bytearray(width*height)
    block=0
    for tile_y in range(0,height,8):
        for tile_x in range(0,width,8):
            for sub_y,sub_x in ((0,0),(0,4),(4,0),(4,4)):
                pixels=decode_etc_block(region(level,block*stride,stride,'ETC block'),kind==12);block+=1
                for y in range(4):
                    for x in range(4):
                        target=(tile_y+sub_y+y)*width+tile_x+sub_x+x
                        pixel=pixels[y*4+x]
                        rgb[3*target:3*target+3]=bytes(pixel[:3]);a[target]=pixel[3]
    return {'width':width,'height':height,'pixel_format':kind,'mipmap_count':mips,
            'mipmap_offsets_raw':list(offsets),'pixel_data_base':base,
            'header_raw_hex':data[:16].hex(),'decoded_mipmap':0,
            'row_order':'top-to-bottom; row zero is source ETC tile y=0'},bytes(rgb),bytes(a)


def png_rgb(width,height,pixels):
    def chunk(kind,content):
        return struct.pack('>I',len(content))+kind+content+struct.pack('>I',zlib.crc32(kind+content)&0xffffffff)
    rows=b''.join(b'\0'+pixels[y*width*3:(y+1)*width*3] for y in range(height))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')


def obj(mesh):
    lines=['# MH4U source positions/UVs; primitive selection is explicit, no skeletal transform.',
           '# Original units unknown. Triangle strips expanded; degenerates omitted.',
           f'o primitive_{mesh["primitive_index"]}']
    for row in mesh['positions']:lines.append('v '+' '.join(format(x,'.9g') for x in row))
    for row in mesh['uvs']:lines.append('vt '+' '.join(format(x,'.9g') for x in row))
    for row in mesh['normals']:lines.append('vn '+' '.join(format(x,'.9g') for x in row))
    for face in mesh['triangles']:lines.append('f '+' '.join(f'{i+1}/{i+1}/{i+1}' for i in face))
    return '\n'.join(lines)+'\n'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inspection',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    inspection_bytes=(args.inspection/'manifest.json').read_bytes()
    inventory_bytes=(args.inspection/'romfs-inventory.json').read_bytes()
    inspection=json.loads(inspection_bytes)
    inventory=json.loads(inventory_bytes)
    chosen=[x for x in inventory if x['path']=='/data/pl_one133.arc']
    if len(chosen)!=1:raise ValueError('Expected exactly one selected archive')
    entry=chosen[0]
    path=Path(inspection['source']['path']);before=path.stat()
    if before.st_size!=inspection['source']['size']:raise ValueError('ROM size changed since inspection')
    if not 0<entry['size']<=MAX_ARC or entry['source_offset']<0 or entry['source_offset']+entry['size']>before.st_size:
        raise ValueError('Selected archive outside ROM or probe resource bound')
    with path.open('rb') as handle:
        handle.seek(entry['source_offset']);arc=handle.read(entry['size'])
    after=path.stat()
    if (before.st_size,before.st_mtime_ns,before.st_ino)!=(after.st_size,after.st_mtime_ns,after.st_ino) or len(arc)!=entry['size']:
        raise ValueError('Source changed during selected-range read')
    args.output.mkdir(parents=True,exist_ok=True)
    directory=arc_members(arc)
    report={'schema':'mh4u-model-probe-v1','producer':'extract_model.py; ordinary local tooling, not REA Evidence',
            'inspection':{'path':str(args.inspection.resolve()),'manifest_sha256':sha(inspection_bytes),
                          'romfs_inventory_sha256':sha(inventory_bytes)},
            'source':{'path':str(path),'size':before.st_size,'sha256_from_prior_inspection':inspection['source']['sha256'],
                      'full_rom_hash_recomputed':False,'stat_stable_during_range_read':True},
            'archive':{**entry,'sha256':sha(arc),'directory':directory},'outputs':[],'members':[],
            'limitations':['Only selected ARC and source members examined; no game execution.',
              'Only primitive 0 of each model exported; other primitives retained in JSON; LOD semantics unknown.',
              'Position/UV values preserved; physical units, skeletal pose and world transform unknown.',
              'Normal interpretation is signed 8-bit normalized, corroborated by public importer; not runtime-verified.',
              'RGB base texture is an unlit preview; alpha retained separately without assigning opacity/specular semantics.',
              'Material shader state, animation, environmental reflections and mipmaps beyond level0 not rendered.']}
    for member in directory['entries']:
        kind=int(member['kind'],16)
        if kind not in (MODEL,TEXTURE,MATERIAL):continue
        data=inflate_member(arc,member)
        name=member['name'].rsplit('\\',1)[-1]
        if not name or name in ('.','..') or '/' in name or '\\' in name or ':' in name:
            raise ValueError('Unsupported member filename')
        extension={MODEL:'mod',TEXTURE:'tex',MATERIAL:'mrl'}[kind]
        report['outputs'].append(save(args.output/f'{name}.{extension}',data))
        record={**member,'sha256':sha(data),'compressed_sha256':sha(arc[member['offset']:member['offset']+member['compressed_size']])}
        if kind==MODEL:
            parsed=parse_mod(data);record['parsed']=parsed
            report['outputs'].append(save(args.output/f'{name}.obj',obj(parsed['meshes'][0])))
        elif kind==TEXTURE:
            parsed,rgb,alpha=parse_tex(data);record['parsed']=parsed;w=parsed['width'];h=parsed['height']
            report['outputs'].append(save(args.output/f'{name}.ppm',f'P6\n{w} {h}\n255\n'.encode()+rgb))
            report['outputs'].append(save(args.output/f'{name}_alpha.pgm',f'P5\n{w} {h}\n255\n'.encode()+alpha))
            report['outputs'].append(save(args.output/f'{name}.png',png_rgb(w,h,rgb)))
            record['parsed']['rgb_sha256']=sha(rgb);record['parsed']['alpha_sha256']=sha(alpha)
        elif kind==MATERIAL:
            # The observed first MRL texture record names the single shared map.
            hdr=unpack('<7I',data,0,'MRL header')
            if data[:4]!=b'MRL\0' or hdr[1]!=32:raise ValueError('Unsupported MRL')
            raw=region(data,hdr[5],76,'first MRL resource')
            record['parsed']={'header_raw_hex':data[:28].hex(),'first_resource_type':f'{unpack("<I",raw,0,"MRL type")[0]:08x}',
                              'first_resource_path':raw[12:76].split(b'\0',1)[0].decode('ascii'),'remaining_fields':'unknown'}
        report['members'].append(record)
    report['outputs'].append(save(args.output/'archive.arc',arc))
    save(args.output/'manifest.json',json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'output':str(args.output.resolve()),'archive_sha256':sha(arc),
                      'models':[{ 'name':r['name'],'meshes':[{k:m[k] for k in ('primitive_index','vertex_count','triangle_count')} for m in r['parsed']['meshes']]} for r in report['members'] if r['kind']==f'{MODEL:08x}']},indent=2))


if __name__=='__main__':
    main()
