"""Synthetic SBC fixtures; no game payloads."""
import json
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import collision_probe as collision


def fixture():
    # One odd-length pair makes the following arrays unaligned by design.
    data = bytearray(72+72+10+32+48+32+7)
    bbox = (0., 0., 0., 0., 1., 0., 1., 0.)
    struct.pack_into('<4s3I2H5I8f', data, 0, b'SBC\xff', collision.VERSION, 0, 2,
                     1, 1, 1, 1, 3, 123, 456, *bbox)
    struct.pack_into('<8f10I', data, 72, *bbox, 55, 66, 77, 0, 1, 0, 1, 0, 3, 0xffffffff)
    struct.pack_into('<HH4BH', data, 144, 0, 65535, 255, 255, 255, 255, 1234)
    struct.pack_into('<3f4HI4BI', data, 154, 0., 1., 0., 0, 1, 2, 0, 99, 1, 2, 3, 4, 100)
    for i, vertex in enumerate(((0, 0, 0, 0), (0, 0, 1, 0), (1, 0, 0, 0))):
        struct.pack_into('<4f', data, 186+16*i, *vertex)
    data[234:266] = bytes(range(32))
    data[266:] = b'UNKNOWN'
    return data


def two_groups():
    one = fixture()
    data = bytearray(72+144+20+64+96+32+7)
    data[:72] = one[:72]
    struct.pack_into('<HHIII', data, 16, 2, 1, 2, 2, 6)
    struct.pack_into('<f', data, 56, 11)
    data[72:144] = one[72:144]
    data[144:216] = one[72:144]
    struct.pack_into('<f', data, 144, 10)
    struct.pack_into('<f', data, 144+16, 11)
    struct.pack_into('<6I', data, 144+0x2c, 1, 1, 1, 1, 3, 3)
    data[216:236] = one[144:154]*2
    data[236:300] = one[154:186]*2
    data[300:396] = one[186:234]*2
    for offset in (348, 364, 380):
        x = struct.unpack_from('<f', data, offset)[0]
        struct.pack_into('<f', data, offset, x+10)
    data[396:] = one[234:]
    return data


class CollisionTests(unittest.TestCase):
    def test_odd_pair_alignment_unknowns_and_preview(self):
        raw = fixture()
        core = collision.parse_collision(raw)
        self.assertEqual(core['spans']['faces']['offset'], 154)
        self.assertEqual(core['pairs'][0]['attribute_id_raw'], 1234)
        self.assertEqual(core['faces'][0]['unknown_u32_14'], 99)
        self.assertEqual(core['groups'][0]['unknown_loader_zeroed_u32_20_24_28'], [55, 66, 77])
        self.assertEqual(core['unparsed_tail']['size'], 7)
        self.assertEqual(core['unparsed_tail']['sha256'], collision.sha(b'UNKNOWN'))
        self.assertEqual(core['validation']['minimum_winding_normal_dot'], 1)
        self.assertIn('f 1//1 2//1 3//1', collision.make_obj(core))
        doc, binary = collision.make_gltf(core)
        self.assertEqual(doc['accessors'][0]['count'], 3)
        self.assertEqual(struct.unpack_from('<9f', binary), (0, 0, 0, 0, 0, 1, 1, 0, 0))
        self.assertEqual(doc['buffers'][0]['byteLength'], len(binary))
        json.dumps(core, allow_nan=False)

    def test_incomplete_sections_and_exaggerated_counts(self):
        with self.assertRaisesRegex(ValueError, 'out of bounds'):
            collision.parse_collision(fixture()[:150])
        raw = fixture(); struct.pack_into('<I', raw, 24, 0xffffffff)
        with self.assertRaisesRegex(ValueError, 'out of bounds'):
            collision.parse_collision(raw)

    def test_representation_expansion_bounded_before_collections(self):
        raw = fixture()
        struct.pack_into('<I', raw, 24, 40000)
        raw.extend(bytes(40000*32))
        with self.assertRaisesRegex(ValueError, 'accounting budget'):
            collision.parse_collision(raw)

    def test_unconfirmed_version_and_out_of_bounds_group_fail_closed(self):
        raw = fixture(); struct.pack_into('<I', raw, 4, 123)
        with self.assertRaisesRegex(ValueError, 'magic/version'):
            collision.parse_collision(raw)
        raw = fixture(); struct.pack_into('<I', raw, 72+0x3c, 1)
        with self.assertRaisesRegex(ValueError, 'Group vertex span out of bounds'):
            collision.parse_collision(raw)

    def test_multiple_groups_use_local_vertex_indices_and_preserve_raw_values(self):
        core = collision.parse_collision(two_groups())
        self.assertEqual(core['faces'][0]['vertices'], [0, 1, 2])
        self.assertEqual(core['faces'][1]['vertices'], [3, 4, 5])
        self.assertEqual(core['faces'][1]['vertices_local_raw'], [0, 1, 2])
        self.assertEqual(core['vertices_xyzw'][3], [10, 0, 0, 0])
        self.assertEqual(core['validation']['minimum_winding_normal_dot'], 1)
        doc, binary = collision.make_gltf(core)
        self.assertEqual(struct.unpack_from('<3f', binary, 36), (10, 0, 0))
        self.assertEqual(doc['accessors'][0]['count'], 6)

    def test_group_overlap_is_not_accepted_as_shared_ownership(self):
        raw = two_groups(); struct.pack_into('<I', raw, 144+0x3c, 0)
        with self.assertRaisesRegex(ValueError, 'Overlapping group vertex'):
            collision.parse_collision(raw)

    def test_group_local_pair_and_global_attribute_bounds(self):
        raw = two_groups(); struct.pack_into('<H', raw, 226, 1)
        with self.assertRaisesRegex(ValueError, 'Pair face index'):
            collision.parse_collision(raw)
        raw = fixture(); struct.pack_into('<H', raw, 154+18, 1)
        with self.assertRaisesRegex(ValueError, 'Face attribute index'):
            collision.parse_collision(raw)

    def test_vertex_and_pair_index_bounds(self):
        raw = fixture(); struct.pack_into('<H', raw, 154+12, 3)
        with self.assertRaisesRegex(ValueError, 'Face vertex index'):
            collision.parse_collision(raw)
        raw = fixture(); struct.pack_into('<H', raw, 144+2, 1)
        with self.assertRaisesRegex(ValueError, 'Pair face index'):
            collision.parse_collision(raw)

    def test_nonfinite_and_bbox_escapes(self):
        raw = fixture(); struct.pack_into('<f', raw, 186, float('nan'))
        with self.assertRaisesRegex(ValueError, 'non-finite'):
            collision.parse_collision(raw)
        raw = fixture(); struct.pack_into('<f', raw, 186, 8)
        with self.assertRaisesRegex(ValueError, 'escape header bbox'):
            collision.parse_collision(raw)

    def test_winding_disagreement_reported_without_rewriting_source(self):
        raw = fixture(); struct.pack_into('<f', raw, 154+4, -1)
        core = collision.parse_collision(raw)
        self.assertEqual(core['validation']['minimum_winding_normal_dot'], -1)
        self.assertEqual(core['faces'][0]['vertices'], [0, 1, 2])

    def test_degenerate_triangles_retained_and_reported(self):
        raw = fixture(); struct.pack_into('<3H', raw, 154+12, 0, 1, 1)
        core = collision.parse_collision(raw)
        self.assertEqual(core['validation']['degenerate_face_indices'], [0])
        self.assertIsNone(core['validation']['minimum_winding_normal_dot'])


if __name__ == '__main__':
    unittest.main()
