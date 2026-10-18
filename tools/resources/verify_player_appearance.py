#!/usr/bin/env python3
"""Read exported appearance accessors back against selected MOD/MRL/TEX identities."""
import argparse
import json
from pathlib import Path
import struct

import extract_model as resource
import model_scene as model


def verify(root,candidate):
    inventory=json.loads((root/'isolated/inventory.json').read_text())
    checks=[];pieces={}
    def check(label,ok):
        if not ok:raise AssertionError(label)
        checks.append(label)
    def decode_piece(piece,identifier,directory):
        raw=(directory/f'entry-{identifier}.bin').read_bytes()
        check(f'{identifier}: MOD source hash',resource.sha(raw)==piece['members'][0]['sha256'])
        if identifier in pieces:
            check(f'{identifier}: isolated/candidate source identity matches',pieces[identifier]['members'][0]['sha256']==piece['members'][0]['sha256'])
        h=resource.unpack('<4s4H13I',raw,0,'MOD header')
        piece['actor']=model.parse_actor(raw,list(range(h[3])),allow_multiple_materials=True,
                                        allow_two_influence_vertices=True,allow_single_joint=True)
        piece['_source_directory']=directory
        pieces[identifier]=piece
    for piece in inventory['pieces']:
        identifier=piece['entry']['id']
        decode_piece(piece,identifier,root/f'isolated/source/{identifier}')
    report=json.loads((candidate/'provenance.json').read_text())
    for piece in report['selections']:
        decode_piece(piece,piece['entry_id'],candidate/'source'/str(piece['entry_id']))
    def read_scene(directory,selections,remap):
        document=json.loads((directory/'scene.gltf').read_text());binary=(directory/'scene.bin').read_bytes()
        check(str(directory)+': bind pose validated',model.validate_scene(document,binary)['max_bind_position_error_source_units']<1e-3)
        expected=[(entry,index) for entry,indices in selections for index in indices]
        observed=[(m['extras']['sourceEntryId'],m['extras']['sourcePrimitiveIndex']) for m in document['meshes']]
        check(str(directory)+': exact selected order, no unselected primitives',observed==expected)
        for mesh,(entry,index) in zip(document['meshes'],expected):
            source=pieces[entry]['actor']['meshes'][index];p=mesh['primitives'][0];label=f'{directory}/{entry}/{index}'
            def values(semantic):return model.accessor_values(document,binary,p['attributes'][semantic])
            for semantic,field in [('POSITION','positions'),('NORMAL','normals'),('TEXCOORD_0','uvs'),('WEIGHTS_0','weights')]:
                expected_float=[struct.unpack('<'+'f'*len(row),struct.pack('<'+'f'*len(row),*row)) for row in source[field]]
                check(label+': '+semantic+' preserved float32',values(semantic)==expected_float)
            mapping=remap.get(entry)
            expected_joint=[tuple(mapping[j] if w else 0 for j,w in zip(row,weights)) if mapping else tuple(row)
                            for row,weights in zip(source['joints'],source['weights'])]
            check(label+': joint identity exact',values('JOINTS_0')==expected_joint)
            check(label+': triangles exact',model.accessor_values(document,binary,p['indices'])==[(i,) for triangle in source['triangles'] for i in triangle])
            material=document['materials'][p['material']];binding=pieces[entry]['materials']['bindings'][source['material_index']]
            check(label+': original material identity',material['name']==binding['name'] and material['extras']['sourceMrlMaterialIndex']==binding['mrl_material_index'])
            texture=next(t for t in pieces[entry]['textures'] if t['name']==binding['texture_name'])
            image=document['images'][document['textures'][material['pbrMetallicRoughness']['baseColorTexture']['index']]['source']]
            check(label+': original texture entry',image['uri']==f'texture-{texture["entry_id"]}.png')
            tex=(pieces[entry]['_source_directory']/f'entry-{texture["entry_id"]}.bin').read_bytes()
            info,rgb,alpha=resource.parse_tex(tex)
            check(label+': original RGB pixels',resource.png_rgb(info['width'],info['height'],rgb)==(directory/image['uri']).read_bytes())
            check(label+': opaque preview explicit',material['alphaMode']=='OPAQUE')
    for piece in inventory['pieces']:
        entry=piece['entry']['id']
        for p in piece['primitives']:
            read_scene(root/f'isolated/entry-{entry}/primitive-{p["index"]:02}',[(entry,[p['index']])],{})
    selections=[];maps={}
    for piece in report['selections']:
        entry=piece['entry_id'];selected=piece['selected_primitive_indices'];omitted=piece['omitted_primitive_indices']
        check(f'{entry}: all selected/omitted accounted for',not set(selected)&set(omitted) and sorted(selected+omitted)==list(range(len(pieces[entry]['actor']['meshes']))))
        selections.append((entry,selected));maps[entry]={j['source_joint_index']:j['target_joint_index'] for j in piece['association']['joints']}
    read_scene(candidate,selections,maps)
    return {'status':'passed','checks':len(checks),'isolated_scenes':sum(len(p['primitives']) for p in inventory['pieces']),
            'candidate_selections':selections,'candidate_sha256':resource.sha((candidate/'scene.gltf').read_bytes()),
            'target_executed':False,'scope':'Decoded resource identity and float32 serialization; no original visibility or shader equivalence'}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--candidate',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();report=verify(args.root,args.candidate)
    resource.save(args.output,json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
