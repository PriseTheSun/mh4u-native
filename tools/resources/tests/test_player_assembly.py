"""Distinct skin-layout and rig-composition semantics using synthetic, non-game fixtures."""
import copy
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import model_scene as model
import player_assembly as assembly
from test_model_scene import fixture


def fixture28():
    data, offsets = fixture();header = list(struct.unpack_from('<4s4H13I', data))
    start, end = offsets['vertices'], offsets['indices']
    vertices = b''.join(data[start+36*i:start+36*i+28] for i in range(3))
    data = bytearray(data[:start]+vertices+data[end:]);header[8] = 84;header[17] = start+84
    struct.pack_into('<4s4H13I', data, 0, *header);data[offsets['primitive']+10] = 28
    return data, offsets


def single_joint_fixture():
    joint = 128;palette = joint+152+256;material = palette+28;primitive = material+128
    vertex = primitive+48;index = vertex+84;data = bytearray(index+6)
    struct.pack_into('<4s4H13I', data, 0, b'MOD\0', 230, 1, 1, 1, 3, 3, 1, 84, 0, 0, 1,
                     joint, material, material, primitive, vertex, index)
    local = model.IDENTITY.copy();local[12:15] = [3, 4, 5]
    q = model.IDENTITY.copy();q[0] = q[5] = q[10] = 10;q[12:15] = [-2, -3, -4]
    struct.pack_into('<4B5f', data, joint, 4, 255, 0, 0, 0, 0, 0, 0, 0)
    struct.pack_into('<16f', data, joint+24, *local)
    struct.pack_into('<16f', data, joint+88, *model.multiply(model.inverse(local), q))
    struct.pack_into('<IB', data, palette, 1, 0);data[material:material+8] = b'material'
    struct.pack_into('<HHIHBB9I', data, primitive, 0, 3, 0, 0, 28, 4, 0, 0, 0, 0, 2, 0, 0, 0, 0)
    for i, p in enumerate([(0,0,0), (1,0,0), (0,1,0)]):
        struct.pack_into('<3f4b2f4B', data, vertex+28*i, *p, 0, 127, 0, 127, p[0], p[1], 0, 0, 255, 0)
    struct.pack_into('<3H', data, index, 0, 1, 2)
    return data


def partial_rig():
    actor = model.parse_actor(fixture()[0], [0]);base = actor['skeleton'];piece = copy.deepcopy(base)
    for key in ('joints', 'global_matrices', 'inverse_bind_matrices'):
        piece[key] = copy.deepcopy(base[key][1:])
    piece['parents'] = [255, 0]
    piece['local_matrices'] = [base['global_matrices'][1], base['local_matrices'][2]]
    return base, piece


class PlayerAssemblyTests(unittest.TestCase):
    def test_stride28_explicit_and_bind_roundtrip(self):
        data, _ = fixture28()
        with self.assertRaisesRegex(ValueError, 'stride36'):
            model.parse_actor(data, [0])
        actor = model.parse_actor(data, [0], allow_two_influence_vertices=True)
        doc, binary = model.make_gltf(actor, 'fixture.png')
        self.assertLess(model.validate_scene(doc, binary)['max_bind_position_error_source_units'], 1e-5)
        self.assertEqual(actor['meshes'][0]['weights'][1], [128/255, 127/255, 0, 0])

    def test_stride28_weight_and_palette_errors(self):
        data, offsets = fixture28();data[offsets['vertices']+26] = 200
        with self.assertRaisesRegex(ValueError, 'summing to255'):
            model.parse_actor(data, [0], allow_two_influence_vertices=True)
        data, offsets = fixture28();data[offsets['vertices']+24] = 100
        with self.assertRaisesRegex(ValueError, 'outside palette'):
            model.parse_actor(data, [0], allow_two_influence_vertices=True)

    def test_stride28_declared_bounds(self):
        data, _ = fixture28();header = list(struct.unpack_from('<4s4H13I', data));header[8] -= 1
        struct.pack_into('<4s4H13I', data, 0, *header)
        with self.assertRaisesRegex(ValueError, 'escape declared buffer'):
            model.parse_actor(data, [0], allow_two_influence_vertices=True)

    def test_single_joint_explicit_bind_contract(self):
        data = single_joint_fixture()
        with self.assertRaisesRegex(ValueError, '2..256'):
            model.parse_actor(data, [0], allow_two_influence_vertices=True)
        actor = model.parse_actor(data, [0], allow_two_influence_vertices=True, allow_single_joint=True)
        doc, binary = model.make_gltf(actor, 'fixture.png')
        self.assertEqual(doc['skins'][0]['joints'], [0])
        self.assertLess(model.validate_scene(doc, binary)['max_bind_position_error_source_units'], 1e-5)
        self.assertEqual(actor['meshes'][0]['positions'][0], [-2, -3, -4])

    def test_partial_rotated_root_maps_by_identity_and_bind(self):
        base, piece = partial_rig();mapping, report = assembly.joint_map(base, piece)
        self.assertEqual(mapping, [1, 2]);self.assertTrue(report['joints'][0]['source_root_maps_to_base_internal_joint'])
        mesh = {'primitive_index': 5, 'material_index': 0, 'positions': [[2,3,0], [3,3,0], [2,4,0]],
                'normals': [[0,0,1]]*3, 'uvs': [[0,0], [1,0], [0,1]], 'triangles': [[0,1,2]],
                'joints': [[0,1,0,0]]*3, 'weights': [[0.5,0.5,0,0]]*3}
        mapped, validation = assembly.remap_piece({'skeleton': piece, 'meshes': [mesh]}, mapping, base, 123, 4, 7)
        self.assertEqual(mapped[0]['joints'][0], [1,2,0,0]);self.assertEqual(mapped[0]['positions'], mesh['positions'])
        self.assertEqual(mapped[0]['material_index'], 4);self.assertEqual(mapped[0]['source_primitive_index'], 5)
        self.assertLess(validation['max_reference_deformation_change'], 1e-8)

    def test_shared_ids_with_incompatible_bind_rejected(self):
        base, piece = partial_rig();piece['global_matrices'][0][12] += 1
        with self.assertRaisesRegex(assembly.AssemblyError, 'incompatible bind'):
            assembly.joint_map(base, piece)

    def test_duplicate_missing_and_conflicting_parent_ids(self):
        base, piece = partial_rig();piece['joints'][1]['source_function_id_candidate'] = piece['joints'][0]['source_function_id_candidate']
        with self.assertRaisesRegex(assembly.AssemblyError, 'not unique'):
            assembly.joint_map(base, piece)
        base, piece = partial_rig();piece['joints'][0]['source_function_id_candidate'] = 99
        with self.assertRaisesRegex(assembly.AssemblyError, 'absent'):
            assembly.joint_map(base, piece)
        base, piece = partial_rig();piece['parents'][0] = 1
        with self.assertRaisesRegex(assembly.AssemblyError, 'parent relation'):
            assembly.joint_map(base, piece)


if __name__ == '__main__':
    unittest.main()
