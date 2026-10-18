"""Producer-boundary checks for the native package; no second LMT decoder."""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import animation_probe as motion
import native_model_motion as native
import root_motion_curves as curves
from test_root_motion_curves import track,parsed


class NativeModelProducerTests(unittest.TestCase):
    def error(self, code, callback):
        with self.assertRaises(motion.ProbeError) as caught:callback()
        self.assertEqual(caught.exception.code,code)

    def test_binary64_keeps_values_that_float32_would_lose(self):
        source=parsed([track(reference=[1e-300,2.0**100,0,1])])
        action={'entry_id':1,**curves.action_curve(source,0)}
        data,info=native.serialized_action(action,{'joint_clip_index':7})
        self.assertEqual(struct.unpack_from('<4d',data,80),(0,1e-300,2.0**100,0))
        self.assertEqual(info['gltf_clip_index'],7)
        self.assertEqual(info['presence_mask'],1)
        self.assertEqual(struct.unpack_from('<Ii',data,40),(4,-1))

    def test_raw_rotation_and_missing_translation_are_distinct(self):
        action={'entry_id':1,**curves.action_curve(parsed([track(3,2,reference=[0,0,0,0])]),0)}
        data,info=native.serialized_action(action,{'joint_clip_index':0})
        self.assertEqual(info['presence_mask'],2)
        self.assertEqual(struct.unpack_from('<5d',data,80),(0,0,0,0,0))
        changed=copy.deepcopy(action);changed['channels']['rotation']['interpolation']='linear_vector'
        self.error('unsupported_sampler',lambda:native.serialized_action(changed,{'joint_clip_index':0}))

    def test_binding_paths_fail_before_read_for_unsafe_domains(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)
            for uri in ('../x','/x','a//b','a/./b','a/../b','a\\b','https://x','a\0b','é.png'):
                self.error('invalid_binding_path',lambda:native.local_file(root,uri))
            self.assertEqual(native.local_file(root,'joints/animation.bin'),root.resolve()/'joints/animation.bin')

    def test_caller_hash_is_required_before_side_effects(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);p=root/'manifest.json';p.write_text('{}')
            self.error('paired_hash_mismatch',lambda:native.build(p,'0'*64,root/'missing-index',root/'out',root/'research'))
            self.assertFalse((root/'out').exists());self.assertFalse((root/'research').exists())


if __name__=='__main__':unittest.main()
