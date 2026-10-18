"""Synthetic static-stage parser, material-binding and geometry checks; no ROM."""
import math
from pathlib import Path
import struct
import sys
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import stage_scene as stage


def mod_fixture():
    data = bytearray(428)
    struct.pack_into('<4s4H13I', data, 0, b'MOD\0', 230, 0, 1, 1,
                     3, 4, 1, 84, 0, 1, 0, 0, 128, 160, 288, 336, 420)
    data[160:162] = b'A\0'
    struct.pack_into('<HHIHBB9I', data, 288, 0, 3, 0, 1, 28, 4, 0, 0, 0, 0, 3, 0, 0, 0, 0)
    for i, position in enumerate([(0, 0, 0), (0, 0, 1), (1, 0, 0)]):
        struct.pack_into('<3f4b2f4B', data, 336+i*28, *position, 0, 127, 0, 0, .25, .75, 0, 0, 0, 0)
    struct.pack_into('<4H', data, 420, 0, 1, 2, 2)
    return data


def mrl_fixture():
    data = bytearray(344)
    struct.pack_into('<7I', data, 0, int.from_bytes(b'MRL\0', 'little'), 32, 2, 2, 0, 28, 180)
    for i, name in enumerate([b'tex-a', b'tex-b']):
        struct.pack_into('<I', data, 28+76*i, stage.resource.TEXTURE)
        data[40+76*i:40+76*i+len(name)] = name
    # MRL order differs from MOD material order; filler bits are deliberately nonzero.
    for i, (name, tex) in enumerate([('B', 2), ('A', 1)]):
        row = [0, (~zlib.crc32(name.encode()))&0xffffffff, 12, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 320+12*i, 0]
        struct.pack_into('<15I', data, 180+60*i, *row)
        struct.pack_into('<3I', data, 320+12*i, 0x1a0abcd3, tex, 0xcd06f1a0)
    return data


class StageTests(unittest.TestCase):
    def assert_stage_error(self, expected, function):
        with self.assertRaises(stage.StageError) as caught:
            function()
        self.assertEqual(caught.exception.code, expected)

    def test_static_geometry_identity_and_uv_conversion(self):
        model = stage.parse_stage(mod_fixture())
        self.assertEqual(model['meshes'][0]['positions'], [(0, 0, 0), (0, 0, 1), (1, 0, 0)])
        self.assertEqual(model['meshes'][0]['uvs'], [(.25, .25)]*3)
        self.assertEqual(model['meshes'][0]['triangles'], [(0, 1, 2)])

    def test_skeleton_and_layout_rejected(self):
        data = mod_fixture(); struct.pack_into('<H', data, 6, 1)
        self.assert_stage_error('unsupported_skeleton', lambda: stage.parse_stage(data))
        data = mod_fixture(); data[298] = 36
        with self.assertRaisesRegex(ValueError, 'Unsupported vertex layout'):
            stage.parse_stage(data)

    def test_vertex_count_and_normal_rejected(self):
        data = mod_fixture(); struct.pack_into('<I', data, 12, 4)
        self.assert_stage_error('vertex_count_mismatch', lambda: stage.parse_stage(data))
        data = mod_fixture(); data[348:351] = b'\0'*3
        self.assert_stage_error('invalid_normal', lambda: stage.parse_stage(data))

    def test_out_of_bounds_mesh_rejected(self):
        with self.assertRaisesRegex(ValueError, 'out of bounds'):
            stage.parse_stage(mod_fixture()[:-4])

    def test_index_expansion_bound_precedes_allocation(self):
        data = mod_fixture(); struct.pack_into('<I', data, 288+28, stage.MAX_INDICES)
        self.assert_stage_error('resource_limit', lambda: stage.parse_stage(data))

    def test_material_binding_uses_hash_and_one_based_texture(self):
        result = stage.parse_materials(mrl_fixture(), ['A', 'B'])
        self.assertEqual([b['mrl_material_index'] for b in result['bindings']], [1, 0])
        self.assertEqual([b['texture_name'] for b in result['bindings']], ['tex-a', 'tex-b'])

    def test_missing_hash_and_zero_texture_rejected(self):
        self.assert_stage_error('ambiguous_material', lambda: stage.parse_materials(mrl_fixture(), ['C', 'B']))
        data = mrl_fixture(); struct.pack_into('<I', data, 336, 0)
        self.assert_stage_error('invalid_texture_index', lambda: stage.parse_materials(data, ['A', 'B']))

    def test_unknown_shader_and_wrong_resource_rejected(self):
        data = mrl_fixture(); struct.pack_into('<I', data, 340, 0x123451a0)
        self.assert_stage_error('ambiguous_albedo', lambda: stage.parse_materials(data, ['A', 'B']))
        data = mrl_fixture(); struct.pack_into('<I', data, 28, 0)
        self.assert_stage_error('unsupported_albedo_resource', lambda: stage.parse_materials(data, ['A', 'B']))

    def test_command_bounds_and_index_rejected(self):
        data = mrl_fixture(); struct.pack_into('<I', data, 204, 2)
        self.assert_stage_error('invalid_command_count', lambda: stage.parse_materials(data, ['A', 'B']))
        data = mrl_fixture(); struct.pack_into('<I', data, 340, 0xcd06f1a1)
        self.assert_stage_error('shader_index_mismatch', lambda: stage.parse_materials(data, ['A', 'B']))

    def test_triangle_distance_analytic_regions(self):
        a, b, c = [0, 0, 0], [2, 0, 0], [0, 2, 0]
        self.assertAlmostEqual(stage.triangle_distance_squared([.5, .5, 3], a, b, c), 9)
        self.assertAlmostEqual(stage.triangle_distance_squared([2, 2, 0], a, b, c), 2)
        self.assertAlmostEqual(stage.triangle_distance_squared([-1, -2, 0], a, b, c), 5)
        self.assertAlmostEqual(stage.triangle_distance_squared([1, 2, 0], a, a, b), 4)

    def test_gltf_has_identity_placement_and_explicit_preview(self):
        model = stage.parse_stage(mod_fixture())
        materials = {'bindings': [{'name': 'A', 'texture_name': 'tex-a', 'mod_material_index': 0, 'mrl_material_index': 0}]}
        doc, binary = stage.make_gltf(model, materials, {'tex-a': 'texture.png'})
        self.assertNotIn('translation', doc['nodes'][0])
        self.assertNotIn('matrix', doc['nodes'][0])
        self.assertFalse(doc['materials'][0]['extras']['originalShaderReimplemented'])
        position = doc['accessors'][0]; view = doc['bufferViews'][position['bufferView']]
        self.assertEqual(struct.unpack_from('<9f', binary, view['byteOffset']), (0, 0, 0, 0, 0, 1, 1, 0, 0))


if __name__ == '__main__':
    unittest.main()
