import math
import copy
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import stage_composition as module


def fixture():
    # Root + unit + position + model reference; offsets deliberately unaligned.
    names = b'Root\0part\0mPos\0mpModel\0'
    reference = struct.pack('<I', module.resource.MODEL)+b'stage\\sample\0'
    strings = 24+4*24+4+16+4+4
    data = bytearray(struct.pack('<4sHH4I', b'SDL\0', 21, 4, 0, 0, 0, strings))
    data += struct.pack('<BBH5I', 1, 0, 0, 0, 0, 0, 0, 0)
    data += struct.pack('<BBH5I', 2, 0, 0, 9, 5, 0x45a468ee, 0, 0)
    data += struct.pack('<BBH5I', 7, 20, 1, 1, 10, 0, 120, 124)
    data += struct.pack('<BBH5I', 11, 128, 1, 1, 15, 0, 140, 144)
    data += struct.pack('<I4fII', 0x03000000, -5, 7, 11385, 0, 0, len(names))
    data += names+reference
    return data


class SchedulerTests(unittest.TestCase):
    def test_source_identity_and_relative_reference(self):
        parsed = module.parse_scheduler(fixture())
        self.assertEqual(parsed['tracks'][2]['keys'][0]['vector_xyzw'], [-5, 7, 11385, 0])
        self.assertEqual(parsed['tracks'][2]['keys'][0]['mode_raw'], 3)
        self.assertEqual(parsed['tracks'][3]['keys'][0]['resource']['name'], 'stage\\sample')
        self.assertEqual(parsed['tracks'][3]['keys'][0]['resource']['type_id'], module.resource.MODEL)
        self.assertEqual(parsed['tracks'][1]['parent_or_slot_raw'], 9)

    def test_ranges_counts_parent_and_nonfinite_fail_closed(self):
        mutations = [(6, '<H', 65535), (20, '<I', 1), (24+2*24+4, '<I', 3),
                     (24+2*24+2, '<H', 65535), (24+2*24+16, '<I', 0),
                     (24+2*24+20, '<I', 0xffffffff), (124, '<f', math.nan),
                     (144, '<I', 0xffffffff)]
        for offset, fmt, value in mutations:
            with self.subTest(offset=offset, value=value):
                data = fixture(); struct.pack_into(fmt, data, offset, value)
                with self.assertRaises(ValueError): module.parse_scheduler(data)
        with self.assertRaises(ValueError): module.parse_scheduler(fixture()[:-1])

    def test_null_resource_is_preserved_without_reading_string_base(self):
        data = fixture(); struct.pack_into('<I', data, 144, 0)
        self.assertIsNone(module.parse_scheduler(data)['tracks'][3]['keys'][0]['resource'])

    def test_connectivity_preserves_separate_surfaces(self):
        core = {'vertices_xyzw': [[0,0,0,0], [1,0,0,0], [0,0,1,0], [1,0,1,0],
                                  [0,2,0,0], [1,2,0,0], [0,2,1,0]],
                'faces': [{'index': i, 'vertices': ids, 'normal': [0,1,0]}
                          for i, ids in enumerate([[0,1,2], [1,3,2], [4,5,6]])]}
        result = module.surface_components(core)
        self.assertEqual([c['face_count'] for c in result['components']], [2,1])
        self.assertEqual(len(result['boundary_edges']), 7)
        self.assertEqual(result['nonmanifold_edges'], [])

    def test_snapshot_rejects_unproved_rotation_scale_or_binding(self):
        tracks = module.parse_scheduler(fixture())['tracks']
        tracks += [dict(index=4, kind='vector', kind_raw=7, property_type_raw=20,
                        parent_or_slot_raw=1, name='mScale', keys=[{'frame':0,'vector_xyzw':[1,1,1,0]}]),
                   dict(index=5, kind='vector', kind_raw=7, property_type_raw=20,
                        parent_or_slot_raw=1, name='mAngle', keys=[{'frame':0,'vector_xyzw':[0,0,0,0]},
                                                              {'frame':599,'vector_xyzw':[0,6.28,0,0]}]),
                   dict(index=6, kind='vector', kind_raw=7, property_type_raw=22,
                        parent_or_slot_raw=1, name='mQuat', keys=[])]
        scene = {'tracks': tracks}
        self.assertEqual(module.first_key_transform(scene)['translation'], [-5,7,11385])
        for change in ('angle', 'scale', 'type', 'reference', 'duplicate'):
            value = copy.deepcopy(scene)
            if change == 'angle': value['tracks'][5]['keys'][0]['vector_xyzw'][1] = 1
            if change == 'scale': value['tracks'][4]['keys'][0]['vector_xyzw'][0] = 2
            if change == 'type': value['tracks'][2]['property_type_raw'] = 22
            if change == 'reference': value['tracks'][3]['keys'][0]['resource']['type_id'] = 1
            if change == 'duplicate': value['tracks'].append(value['tracks'][2])
            with self.subTest(change=change):
                with self.assertRaises(ValueError): module.first_key_transform(value)


if __name__ == '__main__': unittest.main()
