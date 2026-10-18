"""Multi-material opt-in at the existing skeletal producer boundary."""
import struct
import unittest

from test_model_scene import fixture, scene


def two_material_fixture():
    data, offsets = fixture()
    header = list(struct.unpack_from('<4s4H13I', data))
    position = header[14]+128
    data[position:position] = b'second\0'+b'\0'*121
    header[4] = 2
    for index in (15, 16, 17):
        header[index] += 128
    struct.pack_into('<4s4H13I', data, 0, *header)
    struct.pack_into('<I', data, header[15]+4, 1 << 12)
    return data


class PlayerModelTests(unittest.TestCase):
    def test_default_stays_single_material_and_opt_in_preserves_primitive_mapping(self):
        data = two_material_fixture()
        with self.assertRaisesRegex(ValueError, 'explicit multi-material opt-in'):
            scene.parse_actor(data, [0])
        actor = scene.parse_actor(data, [0], allow_multiple_materials=True)
        self.assertEqual(actor['material_names'], ['material', 'second'])
        self.assertNotIn('material_name', actor)
        self.assertEqual(actor['meshes'][0]['material_index'], 1)
        bundle = {'materials': [{'name': 'material'}, {'name': 'second'}],
                  'images': [{'uri': 'synthetic.png'}], 'textures': [{'source': 0}]}
        doc, binary = scene.make_gltf(actor, material_bundle=bundle)
        self.assertEqual(doc['meshes'][0]['primitives'][0]['material'], 1)
        self.assertEqual(doc['materials'], bundle['materials'])
        self.assertLess(scene.validate_scene(doc, binary)['max_bind_position_error_source_units'], 1e-5)

    def test_missing_or_incomplete_bundle_is_rejected(self):
        actor = scene.parse_actor(two_material_fixture(), [0], allow_multiple_materials=True)
        with self.assertRaisesRegex(ValueError, 'explicit complete material bundle'):
            scene.make_gltf(actor, 'invalid.png')
        with self.assertRaisesRegex(ValueError, 'match explicit multi-material actor'):
            scene.make_gltf(actor, material_bundle={'materials': [{}], 'images': [], 'textures': []})


if __name__ == '__main__':
    unittest.main()
