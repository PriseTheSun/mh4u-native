#!/usr/bin/env python3
"""Compare the real native glTF sampler with independent reads of LMT quaternion bytes.

No animation producer imports, no Hermite/tangent equations in the oracle, and no
execution of the original game. Tests the derived raw-nlerp contract, not VFP
bit equivalence or gameplay timing. All input resources remain local.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def norm(q):
    length=math.sqrt(sum(x*x for x in q))
    assert math.isfinite(length) and length>0
    return [x/length for x in q]
def angular(a,b):
    a,b=norm(a),norm(b)
    chord=min(math.dist(a,b),math.dist(a,[-x for x in b]))
    return math.degrees(4*math.asin(min(1,chord/2)))


def source_keys(data,slot,ordinal):
    assert data[:6]==b'LMT\0C\0'
    count=struct.unpack_from('<H',data,6)[0];assert 0<=slot<count
    action=struct.unpack_from('<I',data,8+4*slot)[0]
    offset,tracks,frames=struct.unpack_from('<3I',data,action)
    assert ordinal<tracks
    codec,kind,btype,bone,weight,size,buffer,*tail=struct.unpack_from('<4BfII4fI',data,offset+36*ordinal)
    assert kind==0 and btype==0 and weight==1
    if codec==2:return codec,frames,[(0,list(tail[:4]))]
    assert codec in (6,7,11,12,13)
    extremes=struct.unpack_from('<8f',data,tail[4]) if codec!=6 else None
    stride=8 if codec==6 else 4
    assert size>0 and size%stride==0 and buffer+size<=len(data)
    result=[];frame=0
    for start in range(buffer,buffer+size,stride):
        word=int.from_bytes(data[start:start+stride],'little')
        if codec==6:
            # Decode four signed14 fields using a different algebraic form:
            # sign-extend by XOR/subtract, then exact power-of-two division.
            fields=[(word>>shift)&0x3fff for shift in (42,28,14,0)]
            q=[((v^0x2000)-0x2000)/4096 for v in fields];delta=word>>56
        elif codec==7:
            fields=[(word>>shift)&127 for shift in (21,14,7,0)]
            q=[f32((v-8)%(2**32))*float.fromhex('0x1.24924ap-7') for v in fields];delta=word>>28
        else:
            q=[0.,0.,0.,(((word>>14)&0x3fff)-8)*float.fromhex('0x1.004010p-14')]
            q[codec-11]=((word&0x3fff)-8)*float.fromhex('0x1.004010p-14');delta=word>>28
        if extremes:q=[extremes[i+4]+extremes[i]*q[i] for i in range(4)]
        result.append((frame,q));frame+=delta
        assert delta>0 or start+stride==buffer+size
    assert delta==0
    return codec,frames,result


def sample_source(keys,frame):
    # Each source pair is independent. Rotation comparison is modulo sign, so
    # no global sign transport or exporter's clipping construction is needed.
    for (fa,a),(fb,b) in zip(keys,keys[1:]):
        if frame<fb:
            if sum(x*y for x,y in zip(a,b))<0:b=[-x for x in b]
            t=max(0,(frame-fa)/(fb-fa))
            return norm([(1-t)*x+t*y for x,y in zip(a,b)])
    return norm(keys[-1][1])


def verify(asset,native,tolerance):
    doc=json.loads(asset.read_text());report=json.loads(asset.with_name('animation-provenance.json').read_text())
    records=report.get('actions',[{'source_lmt':report.get('source_lmt',{}).get('path'),
        'sha256':report.get('source_lmt',{}).get('sha256'),'slot':doc['animations'][0]['extras']['source_slot'],
        'channels':report.get('channels',[])}])
    source_cache={};buffers={};queries=[];counts=Counter();samplers=Counter();clipped=holds=0
    def accessor(index):
        a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']];p=asset.parent/doc['buffers'][v['buffer']]['uri']
        raw=buffers.setdefault(str(p),p.read_bytes())
        assert a['type']=='SCALAR' and a['componentType']==5126
        start=v.get('byteOffset',0)+a.get('byteOffset',0)
        return [struct.unpack_from('<f',raw,start+4*i)[0] for i in range(a['count'])]
    for ci,(clip,rec) in enumerate(zip(doc['animations'],records)):
        path=Path(rec['source_lmt']);raw=source_cache.setdefault(str(path),path.read_bytes())
        assert hashlib.sha256(raw).hexdigest()==rec['sha256']
        for channel,info in zip(clip['channels'],rec['channels']):
            if channel['target']['path']!='rotation':continue
            codec,frames,keys=source_keys(raw,rec['slot'],info['track'])
            assert codec==info['codec']
            sampler=clip['samplers'][channel['sampler']];samplers[sampler['interpolation']]+=1
            times=accessor(sampler['input']);node=channel['target']['node'];fps=clip['extras']['export_fps']
            assert doc['nodes'][node]['extras']['sourceFunctionIdCandidate']==info['bone_id']
            if keys[-1][0]>frames-1:clipped+=1
            if keys[-1][0]<frames-1:holds+=1
            sample_times=set(times)
            for left,right in zip(times,times[1:]):
                sample_times.update(f32(left+(right-left)*fraction) for fraction in (.25,.5,.75))
            for seconds in sorted(sample_times):
                expected=sample_source(keys,min(frames-1,seconds*fps))
                queries.append((ci,node,seconds,expected,codec,rec['slot'],info['track']))
                counts[codec]+=1
    maxima={};sum_squared=0;worst=None;maximum=0
    for start in range(0,len(queries),60000):
        batch=queries[start:start+60000]
        text=''.join(f'{ci} {node} {seconds:.17g}\n' for ci,node,seconds,*_ in batch)
        result=subprocess.run([str(native.resolve()),str(asset.resolve())],input=text,capture_output=True,text=True,check=True)
        lines=result.stdout.splitlines();assert len(lines)==len(batch)
        for query,line in zip(batch,lines):
            ci,node,seconds,expected,codec,slot,track=query
            fields=line.split();assert len(fields)==13 and int(fields[0])==ci and int(fields[1])==node
            actual=[float(x) for x in fields[6:10]]
            error=angular(actual,expected);sum_squared+=error**2
            maxima[codec]=max(maxima.get(codec,0),error)
            if error>maximum:
                maximum=error;worst={'clip':ci,'slot':slot,'node':node,'track':track,'codec':codec,
                    'seconds':seconds,'actual':actual,'expected':expected,'degrees':error}
            assert error<=tolerance,(asset,query,line,error)
    return {'asset':str(asset.resolve()),'sha256':sha(asset),'animation_sha256':sha(asset.with_name('animation.bin')),
            'decoder_profile':report['decoder_profile'],'clips':len(doc['animations']),'queries':len(queries),
            'queries_by_codec':dict(counts),'samplers':dict(samplers),'clipped_rotation_tracks':clipped,'held_rotation_tracks':holds,
            'max_error_degrees':maximum,'rms_error_degrees':math.sqrt(sum_squared/max(1,len(queries))),
            'max_error_by_codec_degrees':maxima,'worst':worst,
            'sources':[{'path':p,'sha256':hashlib.sha256(data).hexdigest()} for p,data in source_cache.items()]}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--asset',action='append',type=Path,required=True)
    ap.add_argument('--native',type=Path,required=True)
    ap.add_argument('--max-error-degrees',type=float,default=.002)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();results=[verify(p,args.native,args.max_error_degrees) for p in args.asset]
    report={'status':'passed','method':'Independent LMT byte decode and pairwise raw nlerp versus production C++ load_gltf/sample_animation',
            'native_binary':str(args.native.resolve()),'native_sha256':sha(args.native),'verifier_sha256':sha(Path(__file__)),
            'tolerance_degrees':args.max_error_degrees,'original_game_executed':False,
            'limits':['Python double oracle with exact float32 constants and codec7 VCVT conversion; not VFP bit equivalence',
                      'Supplied FPS convention, not original game wall-clock timing','Only accepted exported local joint tracks'],
            'artifacts':results}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('x') as f:json.dump(report,f,indent=2);f.write('\n')
    print(json.dumps({'status':'passed','queries':sum(r['queries'] for r in results),
                      'max_error_degrees':max(r['max_error_degrees'] for r in results),'output':str(args.output)}))
if __name__=='__main__':main()
