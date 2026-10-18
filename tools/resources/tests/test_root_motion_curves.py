"""Synthetic boundaries for the source-frame model-curve adapter and owner."""
import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import animation_probe as motion
import root_motion_curves as curves


def track(kind=4, codec=1, bone=255, reference=None, keys=None):
    return {'ordinal': 0, 'record_offset': 80, 'record_raw_hex': '00'*36,
            'codec': codec, 'codec_name': motion.CODECS[codec][0], 'track_type': kind,
            'path': motion.PATHS[kind], 'bone_type_raw': 0, 'bone_id': bone,
            'weight_raw': 1.0, 'buffer_offset': 0, 'buffer_size': 0,
            'buffer_sha256': motion.sha(b''), 'reference_xyzw_or_xyz': reference or [0, 0, 0, 1],
            'extremes_offset': 0, 'extremes_scale_bias': None, 'decode_status': 'decoded',
            'keys': keys or []}


def parsed(tracks, frames=4, loop=-1):
    return {'sha256': 'a'*64, 'action_slot_count': 2, 'actions': [
        {'slot': 0, 'offset': 16, 'status': 'parsed', 'frame_count': frames, 'loop_frame': loop,
         'flags_raw': 0x800000, 'header_raw_hex': '00'*60, 'scene_end_position_raw': [91, 92, 93, 0],
         'scene_end_rotation_raw': [0, 0, 0, 1], 'events_offset_unparsed': 124,
         'float_tracks_offset_unparsed': 0, 'tracks': tracks},
        {'slot': 1, 'offset': 0, 'status': 'empty'}]}


class ModelCurveTests(unittest.TestCase):
    def error(self, code, fn):
        with self.assertRaises(motion.ProbeError) as caught:
            fn()
        self.assertEqual(caught.exception.code, code)

    def test_binding_is_type_not_bone255(self):
        source = parsed([track(4, 1, 7), track(0, 2, 255)])
        result = curves.action_curve(source, 0)
        self.assertEqual(result['channels']['translation']['bone_id_raw'], 7)
        self.assertEqual(result['channels']['rotation']['status'], 'absent')
        self.assertIsNone(result['channels']['rotation']['default'])
        self.assertNotIn('samples', result['channels']['rotation'])

    def test_reference_preserves_nonunit_rotation_and_no_translation_default(self):
        source = parsed([track(3, 2, 255, [0, 0, 0, 2])])
        result = curves.action_curve(source, 0)
        self.assertEqual(result['channels']['rotation']['samples'], [[0, 0, 0, 2]]*7)
        self.assertEqual(result['channels']['translation']['status'], 'absent')
        self.assertIsNone(result['normal_binder_translation_base_derived'])

    def test_fractional_vector_sampling_clips_owner_tail_and_keeps_source(self):
        keys = [{'frame': 0, 'value': [0, 2, 4], 'next_frame_delta': 4},
                {'frame': 4, 'value': [8, 10, 12], 'next_frame_delta': 0}]
        source = parsed([track(4, 4, keys=keys)])
        original = copy.deepcopy(source)
        result = curves.action_curve(source, 0)
        channel = result['channels']['translation']
        self.assertEqual(result['sample_frames'], [0, .5, 1, 1.5, 2, 2.5, 3])
        self.assertEqual(channel['samples'], [[i, i+2, i+4] for i in range(7)])
        self.assertEqual(channel['key_frames'], [0, 3])
        self.assertEqual(channel['source_last_key_frame'], 4)
        self.assertEqual(source, original)

    def test_raw_loop_header_and_derived_base_are_distinct(self):
        keys = [{'frame': 0, 'value': [0, 0, 4], 'next_frame_delta': 4},
                {'frame': 4, 'value': [0, 0, 12], 'next_frame_delta': 0}]
        result = curves.action_curve(parsed([track(4, 5, keys=keys)], 5, 2), 0)
        self.assertEqual(result['source_end_position_raw'], [91, 92, 93, 0])
        self.assertEqual(result['normal_binder_loop_frame_derived'], 2)
        self.assertEqual(result['normal_binder_translation_base_derived']['end_minus_loop'], [0, 0, 4])
        negative = curves.action_curve(parsed([track()], loop=-1), 0)
        self.assertEqual(negative['source_loop_frame_raw'], -1)
        self.assertEqual(negative['normal_binder_loop_frame_derived'], 0)
        self.assertIsNone(negative['runtime_attributes'])

    def test_constant_nonzero_is_not_silently_rounded_to_zero(self):
        result = curves.action_curve(parsed([track(reference=[0, 0.0000045, 0, 1])]), 0)
        channel = result['channels']['translation']
        self.assertTrue(channel['constant'])
        self.assertFalse(channel['all_components_exactly_zero'])
        self.assertEqual(result['normal_binder_translation_base_derived']['end_minus_loop'], [0, 0, 0])

    def test_duplicate_model_components_fail(self):
        self.error('duplicate_model_channel', lambda: curves.action_curve(parsed([track(), track(bone=4)]), 0))

    def test_unknown_model_sampler_is_not_accepted_from_generic_decoder(self):
        self.error('unsupported_model_sampler', lambda: curves.action_curve(parsed([track(3, 6)]), 0))

    def test_outside_nonfinite_bool_and_oversized_queries_fail(self):
        t = track()
        a = parsed([t])['actions'][0]
        for frame in (-.1, 3.1, float('nan'), float('inf'), True):
            self.error('invalid_sample_frame', lambda: motion.sample_model_channel(t, a, [frame]))
        self.error('resource_limit', lambda: motion.sample_model_channel(t, a, [0]*(motion.MAX_KEYS+1)))
        self.error('resource_limit', lambda: curves.action_curve(parsed([t], frames=50002), 0))

    def test_empty_invalid_missing_slots_are_explicit(self):
        source = parsed([track()])
        self.error('invalid_slot', lambda: curves.action_curve(source, -1))
        self.error('invalid_slot', lambda: curves.action_curve(source, True))
        self.error('empty_slot', lambda: curves.action_curve(source, 1))
        self.error('missing_model_channels', lambda: curves.action_curve(parsed([track(1, 1, 255)]), 0))

    def test_single_frame_and_unterminated_buffers(self):
        self.assertEqual(curves.action_curve(parsed([track()], frames=1), 0)['sample_frames'], [0])
        keys = [{'frame': 0, 'value': [0, 0, 0], 'next_frame_delta': 4}]
        self.error('unterminated_keys', lambda: curves.action_curve(parsed([track(4, 4, keys=keys)]), 0))


if __name__ == '__main__':
    unittest.main()
