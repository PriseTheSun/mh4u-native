"""Curve contract tests independent of the native renderer and LMT bit packing."""
import json
import math
import struct
import tempfile
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import animation_probe as motion


def fixture(values, frames=None):
    frames = frames or list(range(len(values)))
    return {'codec':6,'path':'rotation','decode_status':'decoded','bone_type_raw':0,'weight_raw':1.,
            'reference_xyzw_or_xyz':[0,0,0,1],
            'keys':[{'frame':f,'value':v,'next_frame_delta':frames[i+1]-f if i+1<len(frames) else 0}
                    for i,(f,v) in enumerate(zip(frames,values))]}


def hermite(rows,index,t,duration):
    p0,m0,p1,m1=rows[3*index+1],rows[3*index+2],rows[3*index+4],rows[3*index+3]
    coeff=(2*t**3-3*t*t+1,t**3-2*t*t+t,-2*t**3+3*t*t,t**3-t*t)
    return motion.normalize([coeff[0]*a+coeff[1]*duration*b+coeff[2]*c+coeff[3]*duration*d
                             for a,b,c,d in zip(p0,m0,p1,m1)])


def write_lmt_fixture(directory):
    """Two original synthetic actions: raw clipping and negative→zero dot."""
    data=bytearray(256)
    struct.pack_into('<4sHH2I',data,0,b'LMT\0',67,2,16,80)
    def word(values,delta):
        return sum((v&16383)<<shift for v,shift in zip(values,(42,28,14,0))) | (delta<<56)
    specs=[(16,144,216,4,[([0,0,0,6144],4),([0,0,4096,0],0)]),
           (80,180,232,5,[([0,0,0,4096],2),([0,0,0,-6144],2),([0,0,4096,0],0)])]
    for action,track,payload,frames,keys in specs:
        struct.pack_into('<IIIi',data,action,track,1,frames,0)
        struct.pack_into('<3I',data,action+48,0x800000,0,0)
        struct.pack_into('<4BfII4fI',data,track,6,0,0,0,1,8*len(keys),payload,0,0,0,1,0)
        for i,(q,delta) in enumerate(keys):struct.pack_into('<Q',data,payload+8*i,word(q,delta))
    source=directory/'synthetic.lmt';source.write_bytes(data)
    model=directory/'model.gltf'
    model.write_text(json.dumps({'asset':{'version':'2.0'},'scene':0,'scenes':[{'nodes':[0]}],
        'nodes':[{'extras':{'sourceFunctionIdCandidate':0}}]}))
    return source,model


class QuaternionCurveTests(unittest.TestCase):
    def assertDirection(self,a,b):
        a,b=motion.normalize(a),motion.normalize(b)
        self.assertLess(min(math.dist(a,b),math.dist(a,[-x for x in b])),2e-13)

    def test_unequal_lengths_use_raw_nlerp_not_unit_nlerp_or_slerp(self):
        a,b=[0,0,0,2],[0,0,1,0]
        times,rows,mode=motion.channel_sampler(fixture([a,b],[0,4]),{'frame_count':5},2)
        self.assertEqual((times,mode),([0,2],'CUBICSPLINE'))
        for t in [0,.1,.25,.5,.75,.9,1]:
            self.assertDirection(hermite(rows,0,t,2),[0,0,t,2*(1-t)])
        self.assertGreater(math.dist(hermite(rows,0,.5,2),motion.normalize([0,0,1,1])),.3)
        self.assertEqual(rows[0], [0]*4);self.assertEqual(rows[-1],[0]*4)

    def test_cut_endpoint_preserves_raw_magnitude_and_entire_prefix(self):
        t,rows,mode=motion.channel_sampler(fixture([[0,0,0,3],[0,0,1,0]],[0,4]),{'frame_count':4},1)
        self.assertEqual(t,[0,3]);self.assertEqual(mode,'CUBICSPLINE')
        for u in [0,.25,.5,.75,1]:
            source_fraction=.75*u
            self.assertDirection(hermite(rows,0,u,3),[0,0,source_fraction,3*(1-source_fraction)])

    def test_sign_transport_preserves_exact_zero_dot_after_negative_segment(self):
        # Segment0 flips the second endpoint; the next raw dot is exactly0.
        # Therefore the third endpoint must retain the transported minus sign.
        raw=[[0,0,0,1],[0,0,0,-2],[0,0,1,0]]
        times,rows,mode=motion.channel_sampler(fixture(raw),{'frame_count':3},1)
        self.assertEqual(rows[7],[0,0,-1,0])
        self.assertDirection(hermite(rows,1,.5,1),[0,0,-.5,1])
        self.assertGreater(math.dist(hermite(rows,1,.5,1),motion.normalize([0,0,.5,1])),.8)

    def test_terminal_hold_and_single_frame(self):
        track=fixture([[0,0,0,2]])
        times,rows,mode=motion.channel_sampler(track,{'frame_count':3},1)
        self.assertEqual((times,mode),([0,2],'CUBICSPLINE'))
        self.assertTrue(all(x==0 for i in [0,2,3,5] for x in rows[i]))
        self.assertEqual(motion.channel_sampler(track,{'frame_count':1},1),([0],[[0,0,0,1]],'LINEAR'))

    def test_native_reader_of_exported_clipped_and_zero_dot_curves(self):
        import verify_rotation_curves as independent
        native=Path(__file__).resolve().parents[3]/'build/mh4u-animation-samples'
        if not native.is_file():self.skipTest('Build mh4u-animation-samples for native boundary verification')
        with tempfile.TemporaryDirectory() as name:
            root=Path(name);source,model=write_lmt_fixture(root)
            parsed=motion.parse_lmt(source.read_bytes())
            motion.export_gltf(source,parsed,0,model,2,root/'out',[(source,parsed,0),(source,parsed,1)])
            report=independent.verify(root/'out/animated.gltf',native,.002)
            self.assertEqual(report['clipped_rotation_tracks'],1)
            self.assertEqual(report['clips'],2)
            self.assertEqual(report['queries'],14)
            self.assertLess(report['max_error_degrees'],.00002)

    def test_zero_quaternion_and_float32_collapsed_timeline_reject(self):
        with self.assertRaises(motion.ProbeError) as e:
            motion.channel_sampler(fixture([[0]*4]),{'frame_count':1},1)
        self.assertEqual(e.exception.code,'zero_quaternion')
        with self.assertRaises(motion.ProbeError) as e:
            motion.channel_sampler(fixture([[0,0,0,1]]*2),{'frame_count':2},1e100)
        self.assertEqual(e.exception.code,'nonincreasing_frames')

if __name__=='__main__':unittest.main()
