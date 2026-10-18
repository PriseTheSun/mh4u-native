"""Analytical vector/timeline checks and the public LMT-to-glTF export boundary."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import animation_probe as motion


def track(codec, keys, reference=None, path='translation'):
    return {'decode_status': 'decoded', 'bone_type_raw': 0, 'weight_raw': 1.0,
            'path': path, 'codec': codec, 'keys': keys,
            'reference_xyzw_or_xyz': reference or [90, 91, 92, 1]}


class AnimationSamplerTests(unittest.TestCase):
    def test_vector_exact_literals_offset_and_unclamped_range(self):
        # Independently written hexadecimal floats match the shipped literal
        # bit patterns, not the producer's constants or nominal reciprocals.
        for codec, fmt, limit, factor in [(4, '<4H', 65535, float.fromhex('0x1.001002p-16')),
                                         (5, '<4B', 255, float.fromhex('0x1.111112p-8'))]:
            with self.subTest(codec=codec):
                row = struct.pack(fmt, 0, 8, limit, 0)
                observed = motion.decode_buffer(codec, row, [2, 4, 6, 0, -1, -2, -3, 0])[0]['value']
                self.assertEqual(observed, [-1-16*factor, -2, -3+6*(limit-8)*factor])
                self.assertLess(observed[0], -1)
                self.assertNotEqual(observed[2], 3)

    def test_stored_vector_starts_at_zero_for_all_loop_metadata(self):
        keys = motion.decode_buffer(5, bytes([8, 8, 8, 2, 248, 128, 8, 0]), [1]*4+[0]*4)
        for loop in [-1, 0, 1]:
            times, values = motion.channel_keys(track(5, keys), {'frame_count': 3, 'loop_frame': loop}, 60)
            self.assertEqual(times, [0, motion.float32(2/60)])
            self.assertEqual(values[0], [0, 0, 0])
            self.assertEqual(values[1], keys[1]['value'])

    def test_endpoint_interpolates_raw_tail_before_cutting(self):
        keys = [{'frame': 0, 'value': [1, 2, 3], 'next_frame_delta': 4},
                {'frame': 4, 'value': [9, 10, 11], 'next_frame_delta': 0}]
        times, values = motion.channel_keys(track(4, keys), {'frame_count': 4, 'loop_frame': 0}, 60)
        self.assertEqual(times, [0, motion.float32(3/60)])
        self.assertEqual(values, [[1, 2, 3], [7, 8, 9]])
        self.assertEqual(keys[-1]['frame'], 4)  # Evidence is not destructively trimmed.

    def test_reference_codecs_ignore_payload_and_hold_until_frame_max(self):
        for codec, path, ref in [(1, 'translation', [7, 8, 9, 0]), (2, 'rotation', [0, 0, 0, 1])]:
            keys = [{'frame': 0, 'value': [4, 5, 6], 'next_frame_delta': 1}]
            times, values = motion.channel_keys(track(codec, keys, ref, path), {'frame_count': 8, 'loop_frame': 3}, 30)
            self.assertEqual(times, [0, motion.float32(7/30)])
            self.assertEqual(values, [ref[:len(values[0])]]*2)

    def test_quaternion_timeline_uses_buffer_without_changing_codec_math(self):
        for codec in [6, 7, 11, 12, 13]:
            keys = [{'frame': 0, 'value': [0, 0, 0, 1], 'next_frame_delta': 4},
                    {'frame': 4, 'value': [0, 0, 1, 0], 'next_frame_delta': 0}]
            times, values = motion.channel_keys(track(codec, keys, [1, 0, 0, 0], 'rotation'),
                                                {'frame_count': 3, 'loop_frame': -1}, 1)
            self.assertEqual(times, [0, 2])
            self.assertEqual(values[0], [0, 0, 0, 1])
            self.assertAlmostEqual(values[1][2], 2**-.5)
            self.assertAlmostEqual(values[1][3], 2**-.5)

    def test_empty_unterminated_and_zero_delta_buffers_fail_closed(self):
        for keys, code in [([], 'missing_key_buffer'),
                           ([{'frame': 0, 'value': [1, 2, 3], 'next_frame_delta': 2}], 'unterminated_keys')]:
            with self.assertRaises(motion.ProbeError) as caught:
                motion.channel_keys(track(5, keys), {'frame_count': 3, 'loop_frame': 0}, 60)
            self.assertEqual(caught.exception.code, code)
        with self.assertRaises(motion.ProbeError) as caught:
            motion.decode_buffer(5, bytes([8, 8, 8, 0])*2, [1]*4+[0]*4)
        self.assertEqual(caught.exception.code, 'nonincreasing_frames')

    def test_single_frame_and_zero_delta_tail_hold(self):
        key = [{'frame': 0, 'value': [1, 2, 3], 'next_frame_delta': 0}]
        self.assertEqual(motion.channel_keys(track(5, key), {'frame_count': 1, 'loop_frame': 0}, 60),
                         ([0], [[1, 2, 3]]))
        self.assertEqual(motion.channel_keys(track(5, key), {'frame_count': 5, 'loop_frame': 0}, 60),
                         ([0, motion.float32(4/60)], [[1, 2, 3], [1, 2, 3]]))

    def test_public_export_interpolates_vector_tail_and_retains_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            # One action, one translation, two codec5 keys at frames0/4.
            data = bytearray(152)
            struct.pack_into('<4sHHI', data, 0, b'LMT\0', 67, 1, 16)
            struct.pack_into('<IIIi', data, 16, 76, 1, 4, 0)
            struct.pack_into('<3I', data, 64, 0x800000, 0, 0)
            struct.pack_into('<4BfII4fI', data, 76, 5, 1, 0, 0, 1, 8, 112, 99, 99, 99, 0, 120)
            data[112:120] = bytes([8, 8, 8, 4, 248, 128, 8, 0])
            struct.pack_into('<8f', data, 120, 2, 4, 6, 0, -1, -2, -3, 0)
            (root/'source.lmt').write_bytes(data)
            (root/'model.gltf').write_text(json.dumps({'asset': {'version': '2.0'}, 'scene': 0,
                'scenes': [{'nodes': [0]}], 'nodes': [{'extras': {'sourceFunctionIdCandidate': 0}}]}))
            result = subprocess.run([sys.executable, '-B', motion.__file__, 'export', '--lmt', str(root/'source.lmt'),
                '--model', str(root/'model.gltf'), '--slot', '0', '--fps', '60', '--output', str(root/'out')],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
            doc = json.loads((root/'out/animated.gltf').read_text());raw = (root/'out/animation.bin').read_bytes()
            sampler = doc['animations'][0]['samplers'][0]
            def values(index, dimension):
                accessor = doc['accessors'][index];view = doc['bufferViews'][accessor['bufferView']]
                return list(struct.iter_unpack('<'+'f'*dimension, raw[view['byteOffset']:view['byteOffset']+view['byteLength']]))
            self.assertEqual(values(sampler['input'], 1), [(0,), (struct.unpack('<f', struct.pack('<f', .05))[0],)])
            self.assertEqual(values(sampler['output'], 3)[0], (-1, -2, -3))
            for actual, expected in zip(values(sampler['output'], 3)[1], [.5, -.5, -3]):
                self.assertAlmostEqual(actual, expected, places=6)
            self.assertEqual(doc['animations'][0]['extras']['source_frame_max'], 3)
            self.assertEqual(doc['animations'][0]['extras']['decoder_profile'], motion.DECODER_PROFILE)


if __name__ == '__main__':
    unittest.main()
