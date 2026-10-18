#!/usr/bin/env python3
"""Wrap the verified 3DS code image in a derived ELF for segmented static analysis.

ELF metadata is synthesized, not recovered from the game. The default BSS
profile verifies the exact code digest, startup instructions and zeroed range.
--bss-profile none explicitly retains the earlier mapping without BSS.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


PROFILE='mh4u-63940d7e'
CODE_SHA256='63940d7ef1fecc119f9fb820f5f6a2cf2f2a5549e4a70f00319fbd6c9c1ad8dc'
ROM_SHA256='c16b6cd17cb761508d3077c99be3fb310b460aa8fcf57d3c651310f082bda666'
BSS_START,BSS_END=0x01081b7c,0x0111d128
STUB=(0xe59f0014,0xe59f1014,0xe3a02000,0xe1500001,0x34802004,0x3afffffc,0xe12fff1e)
FIELDS=('name','virtual_address','code_bin_offset','initialized_size','page_aligned_size')
EXPECTED_SEGMENTS=(('text',1048576,0,13549804,13553664),
                   ('rodata',14602240,13553664,863584,864256),
                   ('data',15466496,14417920,1842044,1843200))


class AnalysisError(Exception):
    def __init__(self,code,message):
        super().__init__(message)
        self.code=code


def require(condition,code,message):
    if not condition:raise AnalysisError(code,message)


def digest(data):return hashlib.sha256(data).hexdigest()


def validate_inputs(manifest,code,profile):
    """All validation precedes ELF allocation and derived mapping."""
    require(isinstance(manifest,dict),'invalid_manifest','Manifest must be an object')
    mapping=manifest.get('memory_map')
    require(isinstance(mapping,dict),'invalid_manifest','Missing memory_map object')
    require(digest(code)==mapping.get('code_bin_sha256'),'source_digest_mismatch','Code digest differs from source manifest')
    require(len(code)==mapping.get('code_bin_size'),'source_size_mismatch','Code size differs from source manifest')
    segments=mapping.get('segments')
    require(isinstance(segments,list) and len(segments)==3,'invalid_segments','Expected three segments')
    source_end=0;virtual_end=None
    for name,segment in zip(('text','rodata','data'),segments):
        require(isinstance(segment,dict) and segment.get('name')==name,'invalid_segments','Expected text/rodata/data order')
        for field in FIELDS[1:]:
            require(type(segment.get(field)) is int and 0<=segment[field]<=0xffffffff,
                    'invalid_segment_field',f'{name}.{field} must be a nonnegative ELF32 integer')
        start=segment['code_bin_offset'];length=segment['page_aligned_size']
        address=segment['virtual_address'];initialized=segment['initialized_size']
        require(start==source_end and start+length<=len(code),'invalid_source_bounds','Source spans must be contiguous and within code.bin')
        require(initialized>0 and length==((initialized+4095)&~4095),'invalid_segment_size','Initialized and page-aligned sizes disagree')
        require(address%4096==0 and address+length<=0xffffffff,'invalid_virtual_bounds','Virtual span is unaligned or exceeds ELF32')
        require(virtual_end is None or address>=virtual_end,'overlapping_segments','Virtual spans overlap')
        source_end=start+length;virtual_end=address+length
    require(source_end==len(code),'unmapped_source_bytes','Source spans must cover code.bin')
    require(mapping.get('entry_point_derived_from_text_address')==segments[0]['virtual_address'],
            'invalid_entry','Entry must equal the text virtual address')
    require(type(mapping.get('bss_size')) is int and 0<=mapping['bss_size']<=0xffffffff,'invalid_bss_size','BSS size must be an ELF32 integer')
    require(isinstance(manifest.get('source'),dict),'invalid_manifest','Missing parent source identity')
    if profile=='none':return mapping,None
    require(profile==PROFILE,'unsupported_profile','Unknown BSS profile')
    require(len(code)>=0x48,'truncated_startup','Missing startup instructions/literals')
    require(struct.unpack_from('<I',code,0)[0]==0xeb000007,'startup_entry_mismatch','First instruction must BL to text+0x24')
    require(struct.unpack_from('<7I',code,0x24)==STUB,'startup_stub_mismatch','Zeroing instructions differ from verified profile')
    start,end=struct.unpack_from('<2I',code,0x40)
    require((start,end)==(BSS_START,BSS_END),'bss_literal_mismatch','Zeroing literals differ from verified interval')
    require(tuple(tuple(s[f] for f in FIELDS) for s in segments)==EXPECTED_SEGMENTS,
            'profile_segment_mismatch','Segment map differs from verified build')
    data=segments[2]
    require(start==data['virtual_address']+data['initialized_size'] and end-start==mapping['bss_size']==636332
            and start%4==0 and end%4==0,'bss_range_mismatch','Zeroing interval and declared BSS disagree')
    require(digest(code)==CODE_SHA256 and len(code)==16261120,'profile_digest_mismatch','Not the exact verified code image')
    require(manifest['source'].get('sha256')==ROM_SHA256,'profile_rom_mismatch','Parent ROM identity differs from verified profile')
    checks=manifest.get('checks')
    require(isinstance(checks,dict) and checks.get('exheader_hash_matches') is True,'unverified_exheader','Source manifest must report matching exheader hash')
    padding_start=data['code_bin_offset']+data['initialized_size']
    padding=code[padding_start:data['code_bin_offset']+data['page_aligned_size']]
    require(not any(padding),'nonzero_data_padding','Omitted data-page padding must be zero')
    return mapping,{'profile':profile,'evidence_type':'static inference; not runtime observation',
        'entry_code_offset':0,'entry_instruction_u32':0xeb000007,
        'derived_first_bl_target':segments[0]['virtual_address']+8+(7<<2),
        'stub_code_offset':0x24,'stub_virtual_address':segments[0]['virtual_address']+0x24,
        'stub_words_u32':list(STUB),'stub_sha256':digest(code[0x24:0x40]),
        'literal_code_offsets':[0x40,0x44],'start_inclusive':start,'end_exclusive':end,'size':end-start,
        'word_stride':4,'source_manifest_reports_exheader_hash_match':True,
        'omitted_source_padding':{'source_code_offset':padding_start,'size':len(padding),'sha256':digest(padding),'all_zero':True},
        'inference':'Matched ARM loop stores zero in [start,end), four bytes at a time; range equals initialized data end plus declared BSS'}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('inspection_directory',type=Path)
    p.add_argument('new_elf',type=Path)
    p.add_argument('--bss-profile',choices=(PROFILE,'none'),default=PROFILE)
    args=p.parse_args()
    manifest_path=args.inspection_directory/'manifest.json'
    manifest_bytes=manifest_path.read_bytes()
    manifest=json.loads(manifest_bytes)
    code=(args.inspection_directory/'code.bin').read_bytes()
    mapping,bss=validate_inputs(manifest,code,args.bss_profile)
    align=lambda n:(n+4095)&~4095
    blob=bytearray(4096)
    programs=[]
    sections=[bytes(40)]
    names=b'\0.text\0.rodata\0.data\0'+(b'.bss\0' if bss else b'')+b'.shstrtab\0'
    permissions={'text':(5,6),'rodata':(4,2),'data':(6,3)}
    provenance=[]
    for segment in mapping['segments']:
        start=segment['code_bin_offset'];length=segment['page_aligned_size']
        if bss and segment['name']=='data':length=segment['initialized_size']
        memory_size=length+(bss['size'] if bss and segment['name']=='data' else 0)
        blob.extend(bytes(align(len(blob))-len(blob)))
        offset=len(blob);data=code[start:start+length];blob.extend(data)
        address=segment['virtual_address'];program_flags,section_flags=permissions[segment['name']]
        programs.append(struct.pack('<8I',1,offset,address,address,length,memory_size,program_flags,4096))
        sections.append(struct.pack('<10I',names.index(('.'+segment['name']).encode()),1,section_flags,
                                    address,offset,length,0,0,4096,0))
        if bss and segment['name']=='data':
            sections.append(struct.pack('<10I',names.index(b'.bss'),8,3,bss['start_inclusive'],
                                        offset+length,bss['size'],0,0,4,0))
        provenance.append({'name':segment['name'],'source_code_offset':start,'elf_offset':offset,'size':length,
                           'file_size':length,'memory_size':memory_size,'source_page_aligned_size':segment['page_aligned_size'],
                           'virtual_address':address,'sha256':hashlib.sha256(data).hexdigest(),
                           'permissions':'derived from NCCH segment role'})
    strings_offset=len(blob);blob.extend(names)
    sections.append(struct.pack('<10I',names.index(b'.shstrtab'),3,0,0,strings_offset,len(names),0,0,1,0))
    blob.extend(bytes((-len(blob))%4));shoff=len(blob);blob.extend(b''.join(sections))
    ident=b'\x7fELF'+bytes([1,1,1,0])+bytes(8)
    header=struct.pack('<16sHHIIIIIHHHHHH',ident,2,40,1,mapping['entry_point_derived_from_text_address'],
                       52,shoff,0,52,32,len(programs),40,len(sections),len(sections)-1)
    blob[:52]=header
    blob[52:52+len(programs)*32]=b''.join(programs)
    record={'format':'synthetic ELF32 little-endian ARM; analysis-only, not a runnable native port',
            'producer':'make_analysis_elf.py (ordinary local tooling, not REA Evidence)',
            'bss_profile':args.bss_profile,
            'parent_rom':manifest['source'],'parent_code_sha256':mapping['code_bin_sha256'],
            'source_manifest':{'path':str(manifest_path.resolve()),'sha256':digest(manifest_bytes)},
            'source_code_path':str((args.inspection_directory/'code.bin').resolve()),
            'elf_sha256':hashlib.sha256(blob).hexdigest(),'elf_size':len(blob),'segments':provenance,
            'declared_bss_bytes':mapping['bss_size'],'bss_mapped':bss is not None,'bss_verification':bss,
            'limitations':['No original ELF metadata exists in this source',
                           'BSS location inferred statically for this exact code image, not observed execution' if bss else 'BSS deliberately omitted by explicit profile',
                           'Source manifest exheader verification retained; ROM exheader is not re-read by this tool',
                           'No ABI or compiler inferred from synthetic ELF flags',
                           'Entry is derived from NCCH text address, not a runtime observation']}
    sidecar=args.new_elf.with_suffix('.provenance.json')
    require(not args.new_elf.exists() and not sidecar.exists(),'output_exists','ELF or provenance already exists; select a new path')
    with args.new_elf.open('xb') as f:f.write(blob)
    with sidecar.open('x') as f:f.write(json.dumps(record,indent=2)+'\n')
    print(json.dumps({'ok':True,'elf':str(args.new_elf.resolve()),'sha256':record['elf_sha256'],'size':len(blob),
                      'bss_mapped':bss is not None,'provenance':str(sidecar.resolve())},indent=2))


if __name__=='__main__':
    try:main()
    except (AnalysisError,OSError,ValueError) as error:
        print(json.dumps({'ok':False,'error':{'code':error.code if isinstance(error,AnalysisError) else 'input_or_output_error','message':str(error)}}))
        sys.exit(2)
