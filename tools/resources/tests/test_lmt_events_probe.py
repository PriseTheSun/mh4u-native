"""Synthetic event-boundary checks; shipped ARM verification is separate."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import animation_probe as motion
import lmt_events_probe as events


def fixture(rows=None, flags=0x800000, pointer=80):
    rows = rows if rows is not None else [(1, 2), (0, 1), (0x80000002, 2)]
    data = bytearray(80+288+4*len(rows)*8)
    struct.pack_into('<4sHHI', data, 0, b'LMT\0', 67, 1, 16)
    struct.pack_into('<IIIi8fIII', data, 16, 80, 0, 5, -1,
                     0,0,0,0, 0,0,0,1, flags,pointer,0)
    for group in range(4):
        offset = 80+72*group; records = 80+288+group*len(rows)*8
        struct.pack_into('<32HII', data, offset, *range(32), len(rows), records)
        for ordinal, row in enumerate(rows):
            struct.pack_into('<II', data, records+8*ordinal, *row)
    return bytes(data)


def parsed(data, limit=events.MAX_RECORDS):
    return events.parse_events(data, motion.parse_lmt(data), limit)


class EventTests(unittest.TestCase):
    def assert_error(self, code, callback):
        with self.assertRaises(motion.ProbeError) as raised:
            callback()
        self.assertEqual(raised.exception.code, code)

    def rejected(self, data, code, limit=events.MAX_RECORDS):
        result = parsed(data, limit)['actions'][0]['events']
        self.assertEqual(result['status'], 'rejected')
        self.assertEqual(result['error']['code'], code)
        self.assertEqual(result['groups'], [])

    def test_source_fields_offsets_and_roundtrip(self):
        data = fixture(); result = parsed(data)
        group = result['actions'][0]['events']['groups'][0]
        self.assertEqual(group['record_count'], 3)
        self.assertEqual(group['mask_union'], 0x80000003)
        self.assertEqual(group['prefix_u16_raw'], list(range(32)))
        self.assertEqual(group['records'][2]['set_bits'], [1,31])
        self.assertEqual(group['records'][2]['prefix_u16_for_set_bits'], [1,31])
        self.assertEqual(group['records'][2]['start_frame_derived'], 3)
        self.assertEqual(result['roundtrip_region_bytes_checked'], 288+4*24)
        self.assertEqual(data, fixture())

    def test_query_boundaries_inclusive_and_zero_mask(self):
        group = parsed(fixture())['actions'][0]['events']['groups'][0]
        self.assertEqual([events.range_mask(group, x, x) for x in range(5)],
                         [1,1,0,0x80000002,0x80000002])
        self.assertEqual(events.range_mask(group, 1,3), 0x80000003)
        self.assertEqual(events.range_mask(group, 2,2), 0)
        self.assertEqual(events.range_mask(group, 0,4), 0x80000003)
        for begin,end in ((-1,0),(0,5),(2,1),(0.0,1),(True,2),(0,float('nan'))):
            self.assert_error('invalid_event_interval', lambda: events.range_mask(group,begin,end))

    def test_null_action_pointer_is_explicit_not_empty_gameplay(self):
        result = parsed(fixture(pointer=0))['actions'][0]['events']
        self.assertEqual(result, {'status':'absent_pointer','groups':[],'block':None})

    def test_raw_kernel_keeps_signed_reversed_and_outside_inputs(self):
        group = parsed(fixture())['actions'][0]['events']['groups'][0]
        self.assertEqual(events.kernel_mask(group,-1,-1),1)
        self.assertEqual(events.kernel_mask(group,4,1),0)
        self.assertEqual(events.kernel_mask(group,-0x80000000,0x7fffffff),0x80000003)
        self.assertEqual(events.kernel_mask(group,0x7fffffff,-0x80000000),0)
        self.assertEqual(events.kernel_mask(group,5,5),0)
        for begin,end in ((-0x80000001,0),(0,0x80000000),(False,0),(0.5,1)):
            self.assert_error('invalid_event_kernel_query',lambda: events.kernel_mask(group,begin,end))

    def test_relocation_variant_rejected_preserves_pointer(self):
        data = fixture(flags=0x2800000)
        self.rejected(data, 'unsupported_event_relocation')
        self.assertEqual(parsed(data)['actions'][0]['events_pointer_raw'],80)

    def test_header_and_record_bounds_fail_before_allocation(self):
        self.rejected(fixture()[:90], 'invalid_bounds')
        data = bytearray(fixture()); struct.pack_into('<I',data,80+0x40,0xffffffff)
        self.rejected(bytes(data), 'resource_limit')
        data = bytearray(fixture()); struct.pack_into('<I',data,80+0x44,len(data)-4)
        self.rejected(bytes(data), 'invalid_bounds')
        data = bytearray(fixture()); struct.pack_into('<I',data,80+0x44,0)
        self.rejected(bytes(data), 'invalid_event_pointer')
        self.rejected(fixture(), 'resource_limit', 11)

    def test_empty_count_preserves_nonzero_pointer(self):
        data = bytearray(fixture()); struct.pack_into('<I',data,80+0x40,0)
        group = parsed(bytes(data))['actions'][0]['events']['groups'][0]
        self.assertEqual(group['records_pointer_raw'],368)
        self.assertIsNone(group['records_region'])
        self.assertEqual(events.range_mask(group,0,4),0)
        self.assertIn('duration_sum_differs_from_action_frame_count',group['warnings'])

    def test_duration_deviation_zero_and_signed_overflow_remain_visible(self):
        group = parsed(fixture([(1,1),(2,0),(4,4)]))['actions'][0]['events']['groups'][0]
        self.assertEqual(events.range_mask(group,0,1),7)
        self.assertIn('zero_duration_record_not_an_ordinary_half_open_interval',group['warnings'])
        group = parsed(fixture([(1,0xffffffff)]))['actions'][0]['events']['groups'][0]
        self.assertFalse(group['integer_kernel_supported'])
        self.assertEqual(group['duration_sum'],0xffffffff)
        self.assert_error('unsupported_event_accumulator',lambda: events.range_mask(group,0,1))

    def test_owner_identity_and_field_reconstruction_reject_divergence(self):
        data = fixture(); owner = motion.parse_lmt(data)
        self.assert_error('source_identity_mismatch', lambda: events.parse_events(data+b'x',owner))
        result=parsed(data); result['actions'][0]['events']['groups'][0]['records'][0]['mask_raw']=5
        self.assert_error('event_roundtrip_mismatch',lambda: events.verify_raw_roundtrip(data,result))

    def test_untrusted_manifest_shape_fails_before_index_or_output(self):
        with tempfile.TemporaryDirectory() as directory:
            base=Path(directory); path=base/'manifest.json'; path.write_bytes(b'[]')
            self.assert_error('unsupported_manifest',lambda: events.build(base/'missing.sqlite',path,motion.sha(b'[]'),base/'new'))
            self.assertFalse((base/'new').exists())
            self.assert_error('paired_manifest_identity_mismatch',lambda: events.build(base/'missing.sqlite',path,'0'*64,base/'new'))


if __name__ == '__main__':
    unittest.main()
