"""Producer identity and representation checks, without optional ARM dependency."""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import animation_probe as motion
import lmt_events_probe as events
import native_motion_events as native
from test_lmt_events_probe import fixture


class NativeEventTests(unittest.TestCase):
    def error(self,code,fn):
        with self.assertRaises(motion.ProbeError) as raised:fn()
        self.assertEqual(raised.exception.code,code)

    def source(self):
        data=fixture();return data,{'entry_id':42,'archive':'/a.arc','name':'a',**events.parse_events(data,motion.parse_lmt(data))}

    def test_inventory_identity_and_complete_owner_readback(self):
        data,source=self.source()
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'entry-42.lmt').write_bytes(data)
            doc={'schema':events.SCHEMA,'profile':events.PROFILE,'sources':[source]}
            path=root/'inventory.json';path.write_text(json.dumps(doc));pin=motion.sha(path.read_bytes())
            loaded,sources=native.load_inventory(path,pin,root);self.assertEqual(sources,[source])
            self.error('inventory_hash_mismatch',lambda:native.load_inventory(path,'0'*64,root))
            bad=copy.deepcopy(doc);bad['sources'][0]['actions'][0]['events']['groups'][0]['records'][0]['mask_raw']=99
            path.write_text(json.dumps(bad));pin=motion.sha(path.read_bytes())
            self.error('inventory_source_mismatch',lambda:native.load_inventory(path,pin,root))
            doc['sources']*=2;path.write_text(json.dumps(doc));pin=motion.sha(path.read_bytes())
            self.error('invalid_source',lambda:native.load_inventory(path,pin,root))

    def test_serialization_preserves_prefix_masks_and_durations(self):
        data,source=self.source();action=source['actions'][0];info=native.action_info(source,action)
        raw,count=native.serialize_action(0,info,action)
        self.assertEqual(count,12);self.assertEqual(struct.unpack_from('<8I',raw), (0,0,5,2,0,4,0,0))
        self.assertEqual(struct.unpack_from('<4I',raw,32),(0,2,3,0))
        self.assertEqual(list(struct.unpack_from('<32H',raw,48)),list(range(32)))
        self.assertEqual(raw[112:136],data[368:392])

    def test_absent_rejected_and_empty_are_distinct_and_have_no_fake_prefix(self):
        _,source=self.source()
        for action,state,presence in [({'slot':0,'status':'empty'},0,0),
            ({'slot':0,'status':'parsed','source_frame_count':1,'events':{'status':'absent_pointer','groups':[]}},1,1),
            ({'slot':0,'status':'parsed','source_frame_count':1,'events':{'status':'rejected','groups':[], 'error':{'code':'invalid_bounds'}}},3,0)]:
            info=native.action_info(source,action);raw,count=native.serialize_action(0,info,action)
            self.assertEqual(count,0);self.assertEqual(struct.unpack_from('<I',raw,12)[0],state)
            self.assertEqual(len(raw),96+len(info['rejection_code']))
            self.assertEqual(struct.unpack_from('<I',raw,32+len(info['rejection_code'])+4)[0],presence)

    def test_query_recipe_signed_values_and_header_are_deterministic(self):
        _,source=self.source();group=source['actions'][0]['events']['groups'][0]
        self.assertIn((-1,-1),native.extended_queries(group));self.assertIn((4,3),native.extended_queries(group))
        self.assertIn((-0x80000000,0x7fffffff),native.extended_queries(group))
        self.assertEqual(native.strict_queries(group),sorted(set(native.strict_queries(group))))
        info=native.action_info(source,source['actions'][0])
        expected={'package_sha256':'a'*64,'inventory_sha256':'b'*64,'sources':[{'entry_id':42,'lmt_sha256':'c'*64,'slot_count':1}],'actions':[info]}
        header=native.identity_header(expected)
        self.assertIn('inline ExpectedPackage expected_package()',header)
        self.assertNotIn('0x80000002',header)
        self.assertEqual(header,native.identity_header(expected))
        oversized=copy.deepcopy(group);oversized['action_frame_count']=1000000
        self.error('resource_limit',lambda:native.strict_queries(oversized))


if __name__=='__main__':unittest.main()
