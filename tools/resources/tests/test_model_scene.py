"""Synthetic format fixtures; contains no bytes or resources from the game."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import model_scene as scene


def fixture():
    count = 3
    local = [scene.IDENTITY.copy(), scene.IDENTITY.copy(), scene.IDENTITY.copy()]
    local[1] = scene.compose({'translation': [2, 0, 0], 'rotation': [0, 0, 2**-0.5, 2**-0.5]})
    local[2][13] = 3
    parents = [255, 0, 1]
    world = scene.globals_from_locals(local, parents)
    q = scene.IDENTITY.copy()
    q[0] = q[5] = q[10] = 10
    q[12:15] = [-2, -3, -4]
    skin = [scene.multiply(scene.inverse(matrix), q) for matrix in world]
    joint_offset = 128
    palette_offset = joint_offset+count*152+256
    material_offset = palette_offset+28
    primitive_offset = material_offset+128
    vertex_offset = primitive_offset+48+4
    index_offset = vertex_offset+3*36
    data = bytearray(index_offset+6)
    struct.pack_into('<4s4H13I', data, 0, b'MOD\0', 230, count, 1, 1, 3, 3, 1, 108,
                     0, 0, 1, joint_offset, material_offset, material_offset,
                     primitive_offset, vertex_offset, index_offset)
    for i in range(count):
        struct.pack_into('<4B5f', data, joint_offset+24*i, 10+i, parents[i], i, 0, 0, 0, 0, 0, 0)
        struct.pack_into('<16f', data, joint_offset+count*24+64*i, *local[i])
        struct.pack_into('<16f', data, joint_offset+count*88+64*i, *skin[i])
    struct.pack_into('<I3B', data, palette_offset, 3, 0, 1, 2)
    data[material_offset:material_offset+8] = b'material'
    struct.pack_into('<HHIHBB9I', data, primitive_offset, 0, 3, 0, 0, 36, 4, 0, 0, 0, 0, 2, 0, 0, 0, 0)
    positions = [(0, 0, 0), (1, 0, 0), (0, 1, 0)]
    bones = [(0, 0, 0, 0), (0, 1, 0, 0), (1, 2, 0, 0)]
    weights = [(255, 0, 0, 0), (128, 127, 0, 0), (128, 127, 0, 0)]
    for i, (point, js, ws) in enumerate(zip(positions, bones, weights)):
        offset = vertex_offset+36*i
        struct.pack_into('<3f4b2f4B4b4B', data, offset, *point, 0, 127, 0, 127, point[0], point[1],
                         js[0], js[1], ws[0], ws[1], 127, 0, 0, 127, js[2], js[3], ws[2], ws[3])
    struct.pack_into('<3H', data, index_offset, 0, 1, 2)
    return data, {'joints': joint_offset, 'palette': palette_offset, 'primitive': primitive_offset,
                  'vertices': vertex_offset, 'indices': index_offset}


class SkeletalFormatTests(unittest.TestCase):
    def test_rotated_hierarchy_bind_and_serialized_gltf(self):
        data, _ = fixture()
        actor = scene.parse_actor(data, [0])
        doc, binary = scene.make_gltf(actor, 'synthetic.png')
        report = scene.validate_scene(doc, binary)
        self.assertEqual(report['vertices_checked'], 3)
        self.assertLess(report['max_bind_position_error_source_units'], 1e-5)
        self.assertEqual(actor['meshes'][0]['positions'][0], [-2, -3, -4])
        self.assertEqual(actor['meshes'][0]['uvs'][0], [0, 1])
        self.assertEqual(doc['nodes'][1]['children'], [2])

    def test_truncated_joint_sections(self):
        data, _ = fixture()
        with self.assertRaisesRegex(ValueError, 'out of bounds'):
            scene.parse_actor(data[:180], [0])

    def test_parent_cycle(self):
        data, offset = fixture()
        data[offset['joints']+1] = 2
        with self.assertRaisesRegex(ValueError, 'Cyclic'):
            scene.parse_actor(data, [0])

    def test_invalid_palette_joint(self):
        data, offset = fixture()
        data[offset['palette']+4] = 99
        with self.assertRaisesRegex(ValueError, 'Palette joint'):
            scene.parse_actor(data, [0])

    def test_invalid_active_joint_slot(self):
        data, offset = fixture()
        data[offset['vertices']+24] = 99
        with self.assertRaisesRegex(ValueError, 'outside palette'):
            scene.parse_actor(data, [0])

    def test_invalid_weight_sum_is_not_silently_repaired(self):
        data, offset = fixture()
        data[offset['vertices']+26] = 200
        with self.assertRaisesRegex(ValueError, 'summing to255'):
            scene.parse_actor(data, [0])

    def test_inconsistent_source_bind_matrix(self):
        data, offset = fixture()
        struct.pack_into('<f', data, offset['joints']+3*88+64+48, 999)
        with self.assertRaisesRegex(ValueError, 'relationship is inconsistent'):
            scene.parse_actor(data, [0])

    def test_oversized_strip_extent_is_bounded_before_allocation(self):
        data, offset = fixture()
        struct.pack_into('<I', data, offset['primitive']+0x1c, 0xffffffff)
        with self.assertRaisesRegex(ValueError, 'out of bounds'):
            scene.parse_actor(data, [0])

    def test_out_of_range_triangle_index(self):
        data, offset = fixture()
        struct.pack_into('<H', data, offset['indices'], 10)
        with self.assertRaisesRegex(ValueError, 'outside selected vertex span'):
            scene.parse_actor(data, [0])

    def test_unknown_vertex_layout_rejected(self):
        data, offset = fixture()
        data[offset['primitive']+10] = 28
        with self.assertRaisesRegex(ValueError, 'type4/stride36'):
            scene.parse_actor(data, [0])

    def test_serialized_weight_corruption_is_detected(self):
        data, _ = fixture()
        doc, binary = scene.make_gltf(scene.parse_actor(data, [0]), 'synthetic.png')
        binary = bytearray(binary)
        accessor = doc['accessors'][doc['meshes'][0]['primitives'][0]['attributes']['WEIGHTS_0']]
        offset = doc['bufferViews'][accessor['bufferView']]['byteOffset']
        struct.pack_into('<f', binary, offset, 0.5)
        with self.assertRaisesRegex(ValueError, 'weights do not sum'):
            scene.validate_scene(doc, binary)


if __name__ == '__main__':
    unittest.main()
