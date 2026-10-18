"""Known-value producer checks for LMT67 YW/ZW14 compression."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import animation_probe as motion


class PlayerCodecTests(unittest.TestCase):
    def test_axis_placement_and_affine_values(self):
        extremes = [2, 4, 6, 8, -1, -2, -3, -4]
        word = (7 << 28) | (8192 << 14) | 16383
        f = float.fromhex('0x1.004010p-14')
        for codec, expected in [(11, [-1+2*16375*f,-2,-3,-4+8*8184*f]),
                                (12, [-1,-2+4*16375*f,-3,-4+8*8184*f]),
                                (13, [-1,-2,-3+6*16375*f,-4+8*8184*f])]:
            with self.subTest(codec=codec):
                keys = motion.decode_buffer(codec, word.to_bytes(4, 'little'), extremes)
                self.assertEqual(keys[0]['next_frame_delta'], 7)
                for observed, value in zip(keys[0]['value'], expected):
                    self.assertAlmostEqual(observed, value, places=12)

    def test_multiple_keys_accumulate_delta_and_preserve_component_bias(self):
        data = ((3 << 28) | (16383 << 14)).to_bytes(4, 'little') + (8192).to_bytes(4, 'little')
        for codec, axis in [(12, 1), (13, 2)]:
            keys = motion.decode_buffer(codec, data, [1, 1, 1, 1, .1, .2, .3, .4])
            self.assertEqual([k['frame'] for k in keys], [0, 3])
            self.assertAlmostEqual(keys[1]['value'][axis], [0.1, 0.2, 0.3][axis]+8184*float.fromhex('0x1.004010p-14'))
            self.assertAlmostEqual(keys[0]['value'][3], .4+16375*float.fromhex('0x1.004010p-14'))

    def test_signed14_twos_complement_boundaries(self):
        fields = [8191, 8192, 16383, 0]
        word = sum(x << s for x,s in zip(fields, (42,28,14,0)))
        key = motion.decode_buffer(6, word.to_bytes(8,'little'), None)[0]
        self.assertEqual(key['value'], [8191/4096, -2, -1/4096, 0])

    def test_unorm7_wrap_and_vcvt_round_before_multiply(self):
        fields = [0, 7, 8, 120]
        word = sum(x << s for x,s in zip(fields, (21,14,7,0)))
        key = motion.decode_buffer(7, word.to_bytes(4,'little'), [1]*4+[0]*4)[0]
        # VCVT.F32.U32 rounds both wrapped integers to exactly 2^32.
        factor = float.fromhex('0x1.24924ap-7')
        self.assertEqual(key['value'], [2**32*factor, 2**32*factor, 0, 112*factor])
        self.assertNotEqual(key['value'][0], (2**32-8)*factor)

    def test_malformed_packed_data_and_missing_scale_rejected(self):
        for codec in [12, 13]:
            for payload, scales, code in [(b'\0'*3, [1]*8, 'invalid_codec_size'),
                                           (b'\0'*4, None, 'missing_extremes'),
                                           (b'\0'*8, [1]*8, 'nonincreasing_frames')]:
                with self.assertRaises(motion.ProbeError) as caught:
                    motion.decode_buffer(codec, payload, scales)
                self.assertEqual(caught.exception.code, code)


if __name__ == '__main__':
    unittest.main()
