#!/usr/bin/env python3
"""Inventory player appearance resources and export only explicitly selected primitives."""
import argparse
import copy
import json
from pathlib import Path
import struct

import extract_model as resource
import index_resources as indexer
import model_scene as model
import player_assembly as assembly
import player_probe as player
import stage_scene as material


def select_piece(piece, indices):
    actor = piece['actor']
    model.require(indices and len(indices) == len(set(indices)) and
                  all(type(i) is int and 0 <= i < len(actor['meshes']) for i in indices),
                  'Select distinct existing primitive indices explicitly')
    selected = copy.deepcopy(actor)
    selected['meshes'] = [copy.deepcopy(actor['meshes'][i]) for i in indices]
    used_materials = sorted({m['material_index'] for m in selected['meshes']})
    bundle = piece['material_bundle']
    used_textures = sorted({bundle['materials'][i]['pbrMetallicRoughness']['baseColorTexture']['index'] for i in used_materials})
    used_images = sorted({bundle['textures'][i]['source'] for i in used_textures})
    compact = {'materials':[copy.deepcopy(bundle['materials'][i]) for i in used_materials],
               'textures':[copy.deepcopy(bundle['textures'][i]) for i in used_textures],
               'images':[copy.deepcopy(bundle['images'][i]) for i in used_images]}
    for m in selected['meshes']:
        m['material_index'] = used_materials.index(m['material_index'])
    selected['material_names'] = [actor['material_names'][i] for i in used_materials]
    for m in compact['materials']:
        reference = m['pbrMetallicRoughness']['baseColorTexture']
        reference['index'] = used_textures.index(reference['index'])
    for t in compact['textures']:
        t['source'] = used_images.index(t['source'])
    names = {image['uri'] for image in compact['images']}
    # Preserve optional alpha evidence without assigning original blend semantics.
    names |= {name.removesuffix('.png')+'-alpha.pgm' for name in names}
    files = [(name,data) for name,data in piece['files'] if name in names]
    return selected, compact, files


def write_scene(actor, bundle, files, output, extras):
    output.mkdir(parents=True,exist_ok=True)
    doc, binary = model.make_gltf(actor, material_bundle=bundle)
    doc['asset']['generator'] = 'mh4u-native player_appearance.py'
    doc['extras'] = extras
    for mesh, source in zip(doc['meshes'],actor['meshes']):
        mesh['extras'] = {'sourceEntryId':source.get('source_entry_id',extras.get('sourceEntryId')),
                          'sourcePrimitiveIndex':source.get('source_primitive_index',source['primitive_index'])}
    validation = model.validate_scene(doc,binary)
    outputs = [resource.save(output/name,data) for name,data in files]
    outputs += [resource.save(output/'scene.gltf',json.dumps(doc,indent=2)+'\n'),resource.save(output/'scene.bin',binary)]
    return {'outputs':outputs,'validation':validation}


def load(connection, metadata, identifier, shared, source):
    return player.load_piece(connection,metadata,identifier,shared,source,
                             allow_two_influence_vertices=True,allow_single_joint=True,max_vertices=25000)


def inventory(index, identifiers, base_entry, shared, output):
    model.require(1 <= len(identifiers) <= 4 and len(set(identifiers)) == len(identifiers), 'Select one to four distinct appearance entries')
    connection, metadata = indexer.open_index(index)
    result = {'schema':'mh4u-player-appearance-v1','source':metadata['source'], 'explicit_entries':identifiers,
              'explicit_base_entry':base_entry,'explicit_shared_archive':shared,'pieces':[],
              'original_appearance_selection':'unknown; isolated previews are not defaults',
              'target_executed':False,'producer_sha256':resource.sha(Path(__file__).read_bytes())}
    try:
        base = load(connection,metadata,base_entry,shared,output/'source'/str(base_entry))
        result['base_members'] = base['members']
        for identifier in identifiers:
            piece = load(connection,metadata,identifier,shared,output/'source'/str(identifier))
            actor = piece['actor'];_,association = assembly.joint_map(base['actor']['skeleton'],actor['skeleton'])
            record = {k:piece[k] for k in ['entry','members','materials','textures']}
            record.update(association=association,byte_evidence=assembly.byte_evidence(actor),primitives=[])
            for mesh, primitive in zip(actor['meshes'],actor['primitives']):
                raw = bytes.fromhex(primitive['raw_record_hex']);mid = mesh['material_index']
                binding = piece['materials']['bindings'][mid]
                selected, bundle, files = select_piece(piece,[mesh['primitive_index']])
                destination = output/f'entry-{identifier}'/f'primitive-{mesh["primitive_index"]:02}'
                exported = write_scene(selected,bundle,files,destination,
                    {'scope':'One explicitly isolated source primitive; no appearance/default or visibility claim',
                     'sourceEntryId':identifier,'sourcePrimitiveIndex':mesh['primitive_index'],
                     'sourceMaterialIndex':mid,'originalShaderReimplemented':False,'alphaPolicy':'opaque neutral RGB preview'})
                record['primitives'].append({'index':mesh['primitive_index'],'vertices':len(mesh['positions']),
                    'triangles':len(mesh['triangles']),'bounds':material.bounds(mesh['positions']),
                    'uv_bounds':material.bounds([(*uv,0) for uv in mesh['uvs']]),
                    'material_index':mid,'material_name':actor['material_names'][mid],
                    'mrl_material_index':binding['mrl_material_index'],'texture_name':binding['texture_name'],
                    'raw_record_hex':raw.hex(),'u16_at_0':struct.unpack_from('<H',raw,0)[0],
                    'u32_at_4':struct.unpack_from('<I',raw,4)[0],
                    'low12_at_4_candidate_selection_id':struct.unpack_from('<I',raw,4)[0]&0xfff,
                    'selection_id_semantics':'unknown; raw correlation only','isolated_preview':str(destination.resolve()),
                    **exported})
            result['pieces'].append(record)
        resource.save(output/'inventory.json',json.dumps(result,indent=2)+'\n')
        return {'output':str(output.resolve()),'pieces':len(result['pieces']),
                'isolated_primitives':sum(len(p['primitives']) for p in result['pieces'])}
    finally:
        connection.close()


def compose(index, selections, base_entry, shared, output):
    model.require(1 <= len(selections) <= 8 and len({i for i,_ in selections}) == len(selections),'Select one to eight distinct pieces explicitly')
    connection,metadata = indexer.open_index(index)
    try:
        base = load(connection,metadata,base_entry,shared,output/'source'/str(base_entry))
        actor = {'skeleton':copy.deepcopy(base['actor']['skeleton']),'material_names':[],'meshes':[]}
        bundle = {'images':[],'textures':[],'materials':[]};files={};records=[];vertices=0
        for identifier,indices in selections:
            piece=load(connection,metadata,identifier,shared,output/'source'/str(identifier))
            selected,selected_bundle,selected_files=select_piece(piece,indices)
            vertices += sum(len(mesh['positions']) for mesh in selected['meshes'])
            model.require(vertices <= 100000,'Explicit composition exceeds the 100000-vertex probe budget')
            mapping,association=assembly.joint_map(actor['skeleton'],selected['skeleton'])
            meshes,validation=assembly.remap_piece(selected,mapping,actor['skeleton'],identifier,len(bundle['materials']),len(actor['meshes']))
            for name,data in selected_files:
                model.require(name not in files or files[name] == data,'Conflicting bytes for texture identity')
                files[name]=data
            assembly.merge_bundle(bundle,selected_bundle)
            actor['meshes'].extend(meshes);actor['material_names'].extend(selected['material_names'])
            records.append({'entry_id':identifier,'selected_primitive_indices':indices,
                            'omitted_primitive_indices':[i for i in range(len(piece['actor']['meshes'])) if i not in indices],
                            'members':piece['members'],'materials':piece['materials'],'textures':piece['textures'],
                            'association':association,'remap_validation':validation})
        exported=write_scene(actor,bundle,list(files.items()),output,
            {'scope':'Explicit appearance inspection candidate; no original default or visibility rule established',
             'selections':[{'entry_id':i,'primitive_indices':indices} for i,indices in selections],
             'baseEntryId':base_entry,'baseGeometryIncluded':False,'originalShaderReimplemented':False,'alphaPolicy':'opaque neutral RGB preview'})
        record={'schema':'mh4u-player-appearance-selection-v1','source':metadata['source'],'base_members':base['members'],
                'explicit_base_entry':base_entry,'explicit_shared_archive':shared,'selections':records,**exported,
                'producer_sha256':resource.sha(Path(__file__).read_bytes()),'target_executed':False,
                'limitations':['Explicit inspection selection, not the original default appearance','Source rules for mutual exclusion remain unknown','Opaque RGB; original alpha, color customization and shader are unknown','Base skeleton retained; body geometry not included']}
        resource.save(output/'provenance.json',json.dumps(record,indent=2)+'\n')
        return {'output':str(output.resolve()),'primitives':len(actor['meshes']),**exported['validation']}
    finally:
        connection.close()


def selection(value):
    try:
        identifier,indices=value.split(':')
        values=[int(x) for x in indices.split(',')]
        if not values or any(x<0 for x in values) or int(identifier)<=0:
            raise ValueError()
        return int(identifier),values
    except ValueError as error:
        raise argparse.ArgumentTypeError('Selection requires positive ENTRY:INDEX[,INDEX...]') from error


def main():
    parser=argparse.ArgumentParser(description=__doc__);commands=parser.add_subparsers(dest='command',required=True)
    for command in ['inventory','compose']:
        p=commands.add_parser(command);p.add_argument('--index',type=Path,required=True)
        p.add_argument('--base-entry',type=int,required=True);p.add_argument('--shared-archive',required=True)
        p.add_argument('--output',type=Path,required=True)
        p.add_argument('--entry' if command=='inventory' else '--selection',type=int if command=='inventory' else selection,action='append',required=True)
    args=parser.parse_args()
    try:
        result=inventory(args.index,args.entry,args.base_entry,args.shared_archive,args.output) if args.command=='inventory' else compose(args.index,args.selection,args.base_entry,args.shared_archive,args.output)
        print(json.dumps(result,indent=2))
    except (ValueError,OSError) as error:
        print(json.dumps({'error':getattr(error,'code','invalid_input'),'message':str(error)}));raise SystemExit(2)


if __name__=='__main__':main()
