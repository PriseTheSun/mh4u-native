#!/usr/bin/env python3
"""Serialize owner-verified event facts and signed kernels; no LMT decoder here."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

import animation_probe as motion
import lmt_events_probe as events
import root_motion_curves as common

SCHEMA='mh4u-native-motion-events-v1'
MAGIC=b'MH4UME01'
KERNELS=(0x958638,0x9586ac)
STATES={'empty_slot':0,'events_absent':1,'parsed':2,'rejected':3}
GROUP_STATES={'unavailable':0,'absent':1,'present':2}
MAX_ORACLE_QUERIES=1000000  # Local validation budget, not a format limit.


def load_inventory(path, expected_sha, source_root):
    raw=motion.read(path)
    motion.require(motion.sha(raw)==expected_sha,'inventory_hash_mismatch','Inventory differs from caller-selected hash')
    doc=json.loads(raw)
    motion.require(isinstance(doc,dict) and doc.get('schema')==events.SCHEMA and doc.get('profile')==events.PROFILE,
                   'unsupported_inventory','Expected verified LMT67 event inventory')
    sources=doc.get('sources')
    motion.require(isinstance(sources,list) and 0<len(sources)<=events.MAX_SOURCES,
                   'invalid_sources','Invalid source list')
    seen=set();verified=[]
    for source in sources:
        motion.require(isinstance(source,dict) and type(source.get('entry_id')) is int and source['entry_id']>0 and
                       source['entry_id'] not in seen,'invalid_source','Invalid/duplicate indexed entry')
        entry=source['entry_id'];seen.add(entry);path=source_root/f'entry-{entry}.lmt'
        data=motion.read(path);owner=motion.parse_lmt(data)
        reconstructed={'entry_id':entry,'archive':source.get('archive'),'name':source.get('name'),**events.parse_events(data,owner)}
        motion.require(source==reconstructed,'inventory_source_mismatch','Inventory differs from source bytes and owner reconstruction')
        verified.append(reconstructed)
    motion.require(sum(s['slot_count'] for s in verified)<=4096,'resource_limit','Aggregate source slots exceed package policy')
    return doc,verified


def strict_queries(group):
    """Same recipe as the pinned 89,697-query ARM inventory; digest checked below."""
    n=group['action_frame_count']
    motion.require(2*n+12*len(group['records'])<=MAX_ORACLE_QUERIES,
                   'resource_limit','Per-group oracle recipe exceeds local query budget')
    result={(x,x) for x in range(n)} | {(x,x+1) for x in range(n-1)} | {(0,n-1)}
    for row in group['records']:
        for boundary in (row['start_frame_derived'],row['end_frame_exclusive_derived']):
            for value in (boundary-1,boundary,boundary+1):
                if 0<=value<n:result.add((0,value));result.add((value,n-1))
    return sorted(result)


def extended_queries(group):
    n=group['action_frame_count'];low,high=-0x80000000,0x7fffffff
    return sorted({(-1,-1),(-2,-1),(-1,0),(0,-1),(1,0),(n-1,n-2),(n,n),(n+3,0),
                   (n+1,n+2),(-1,n),(0,n),(low,high),(high,low),(low,low),(high,high)})


def query_digest(group, queries):
    result=hashlib.sha256()
    for begin,end in queries:
        mask=events.kernel_mask(group,begin,end)
        for address in KERNELS:result.update(struct.pack('<IiiI',address,begin,end,mask))
    return result.hexdigest()


def action_info(source,action):
    if action['status']=='empty':state='empty_slot';presence='unavailable';reason=''
    else:
        status=action['events']['status']
        state={'absent_pointer':'events_absent','parsed':'parsed','rejected':'rejected'}[status]
        presence={'events_absent':'absent','parsed':'present','rejected':'unavailable'}[state]
        reason=action['events']['error']['code'] if state=='rejected' else ''
    return {'entry_id':source['entry_id'],'slot':action['slot'],'lmt_sha256':source['source_lmt_sha256'],
            'frame_count':action.get('source_frame_count'),'state':state,'groups':[presence]*4,'rejection_code':reason}


def serialize_action(source_index,info,action):
    reason=info['rejection_code'].encode('ascii')
    motion.require(len(reason)<=96 and all(chr(c).islower() or chr(c).isdigit() or c==95 for c in reason),
                   'invalid_rejection_code','Rejection code must be bounded lowercase ASCII')
    data=bytearray(struct.pack('<8I',source_index,info['slot'],info['frame_count'] or 0,STATES[info['state']],len(reason),4,0,0)+reason)
    count=0
    for ordinal,presence in enumerate(info['groups']):
        group=action['events']['groups'][ordinal] if presence=='present' else None
        number=group['record_count'] if group else 0
        data.extend(struct.pack('<4I',ordinal,GROUP_STATES[presence],number,0))
        if group:
            motion.require(group['group_index']==ordinal and len(group['records'])==number and len(group['prefix_u16_raw'])==32,
                           'invalid_owner_group','Owner event group shape differs')
            data.extend(struct.pack('<32H',*group['prefix_u16_raw']))
            for row in group['records']:data.extend(struct.pack('<II',row['mask_raw'],row['duration_frames_raw']))
        count+=number
    return bytes(data),count


def identity_header(expected):
    lines=['#pragma once','#include "motion_events.hpp"',
           '// Generated local identities/states only; no event masks or prefix values.',
           'namespace mh4u::motion_events::generated {','inline ExpectedPackage expected_package() {',
           '    return {'+json.dumps(expected['package_sha256'])+','+json.dumps(expected['inventory_sha256'])+',{']
    for s in expected['sources']:lines.append('        {'+str(s['entry_id'])+','+json.dumps(s['lmt_sha256'])+','+str(s['slot_count'])+'},')
    lines.append('    }, {')
    for a in expected['actions']:
        lines.append('        {{'+str(a['entry_id'])+','+str(a['slot'])+','+json.dumps(a['lmt_sha256'])+'},'+
            (str(a['frame_count']) if a['frame_count'] is not None else 'std::nullopt')+',ActionState::'+a['state']+',{'+
            ','.join('GroupState::'+g for g in a['groups'])+'},'+json.dumps(a['rejection_code'])+'},')
    return '\n'.join(lines+['    }};','}','} // namespace mh4u::motion_events::generated',''])


def arm_report(path,sha,inventory_sha):
    raw=motion.read(path);motion.require(motion.sha(raw)==sha,'arm_report_hash_mismatch','ARM report differs from caller pin')
    doc=json.loads(raw)
    motion.require(isinstance(doc,dict) and doc.get('status')=='passed' and doc.get('inventory_sha256')==inventory_sha and
                   doc.get('mask_mismatches')==0,'invalid_arm_report','ARM report does not validate this inventory')
    rows={}
    for row in doc['results']:
        key=(row['entry_id'],row['slot'],row['group'])
        motion.require(key not in rows,'duplicate_arm_group','Repeated ARM result group');rows[key]=row
    return doc,rows


def build(inventory_path,inventory_sha,source_root,arm_path,arm_sha,extended_path,extended_sha,output,research):
    inventory,sources=load_inventory(inventory_path,inventory_sha,source_root)
    original,arm=arm_report(arm_path,arm_sha,inventory_sha)
    extended,extra=arm_report(extended_path,extended_sha,inventory_sha)
    motion.require(extended.get('schema')=='mh4u-event-signed-kernel-arm-v1','invalid_arm_report','Expected signed-kernel extension report')
    motion.require(not output.exists() and not research.exists(),'existing_output','Choose new output/research directories')
    output,research=output.resolve(),research.resolve()
    motion.require(output!=research and output not in research.parents and research not in output.parents,
                   'overlapping_output','Artifact and research directories must be separate')
    expected={'inventory_sha256':inventory_sha,'sources':[],'actions':[]};body=bytearray();records=0
    for s in sources:
        info={'entry_id':s['entry_id'],'lmt_sha256':s['source_lmt_sha256'],'slot_count':s['slot_count']}
        expected['sources'].append(info)
        body.extend(struct.pack('<I',info['entry_id'])+bytes.fromhex(info['lmt_sha256'])+struct.pack('<II',info['slot_count'],0))
    oracle=[];seen=set();strict_count=extra_count=0
    for source_index,source in enumerate(sources):
        for action in source['actions']:
            info=action_info(source,action);index=len(expected['actions']);expected['actions'].append(info)
            data,count=serialize_action(source_index,info,action);body.extend(data);records+=count
            motion.require(records<=events.MAX_RECORDS,'resource_limit','Aggregate event record limit exceeded')
            for group in action.get('events',{}).get('groups',[]):
                key=(source['entry_id'],action['slot'],group['group_index']);seen.add(key)
                queries=strict_queries(group);extras=extended_queries(group)
                motion.require(len(oracle)+len(queries)+len(extras)<=MAX_ORACLE_QUERIES,
                               'resource_limit','Aggregate oracle query budget exceeded')
                motion.require(key in arm and key in extra and arm[key]['queries']==len(queries) and
                               arm[key]['result_sha256']==query_digest(group,queries) and
                               extra[key]['queries']==len(extras) and extra[key]['result_sha256']==query_digest(group,extras),
                               'arm_oracle_mismatch','Owner values or query set differ from isolated ARM report')
                for mode,items in ((0,queries),(1,extras)):
                    for begin,end in items:
                        mask=events.range_mask(group,begin,end) if mode==0 else events.kernel_mask(group,begin,end)
                        oracle.append(f'{index}\t{group["group_index"]}\t{mode}\t{begin}\t{end}\t{mask}')
                strict_count+=len(queries);extra_count+=len(extras)
    motion.require(seen==set(arm)==set(extra) and strict_count==original['integer_interval_queries'] and
                   extra_count==extended['integer_interval_queries'],'arm_coverage_mismatch','ARM group/query coverage differs')
    binary=MAGIC+struct.pack('<6I',0x01020304,64+len(body),len(sources),len(expected['actions']),1,0)+bytes.fromhex(inventory_sha)+body
    motion.require(len(binary)<=8*1024*1024,'resource_limit','Native event package exceeds byte limit')
    expected['package_sha256']=motion.sha(binary)
    output.mkdir(parents=True);research.mkdir(parents=True);artifacts={}
    for name,data in [('motion-events.bin',binary),('motion_events_identity.hpp',identity_header(expected).encode()),
                      ('range-oracle.tsv', ('\n'.join(oracle)+'\n').encode())]:
        with (output/name).open('xb') as handle:handle.write(data)
        artifacts[name]={'sha256':motion.sha(data),'size':len(data)}
    common.write_new(output/'identity.json',expected)
    report={'schema':SCHEMA,'format_magic':MAGIC.decode(),'byte_order':'little','numeric_conversion':'none; original uint16/uint32 preserved',
            'expected':expected,'artifacts':artifacts,'source_inventory':{'path':str(inventory_path.resolve()),'sha256':inventory_sha},
            'source_root':str(source_root.resolve()),'sources_readback':True,'actions':len(expected['actions']),'records':records,
            'strict_queries':strict_count,'extended_signed_queries':extra_count,'oracle_queries':len(oracle),
            'arm_reports':[{'path':str(arm_path.resolve()),'sha256':arm_sha},{'path':str(extended_path.resolve()),'sha256':extended_sha}],
            'producer_sha256':motion.sha(Path(__file__).read_bytes()),'event_owner_sha256':motion.sha(Path(events.__file__).read_bytes()),
            'lmt_owner_sha256':motion.sha(Path(motion.__file__).read_bytes()),'runtime_dispatch_enabled':False,'remap_semantics':None}
    provenance=common.write_new(research/'provenance.json',report)
    return {'binary':artifacts['motion-events.bin'],'header':str(output/'motion_events_identity.hpp'),
            'actions':len(expected['actions']),'records':records,'oracle_queries':len(oracle),'provenance':provenance}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('inventory','source-root','arm-report','extended-arm-report','output','research'):p.add_argument('--'+name,type=Path,required=True)
    for name in ('inventory-sha256','arm-sha256','extended-arm-sha256'):p.add_argument('--'+name,required=True)
    a=p.parse_args()
    try:
        print(json.dumps(build(a.inventory,a.inventory_sha256,a.source_root,a.arm_report,a.arm_sha256,
                               a.extended_arm_report,a.extended_arm_sha256,a.output,a.research),indent=2));return 0
    except (motion.ProbeError,OSError,ValueError,KeyError,TypeError) as error:
        print(json.dumps({'error':getattr(error,'code','invalid_input'),'message':str(error)}),file=sys.stderr);return 1


if __name__=='__main__':raise SystemExit(main())
