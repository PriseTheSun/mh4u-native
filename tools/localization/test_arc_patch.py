import struct
import unittest
import zlib

from arc_patch import replace_member, resource


class PatchTests(unittest.TestCase):
    def archive(self):
        first, second = zlib.compress(b'first'), zlib.compress(b'unchanged second')
        header = struct.pack('<4sHHI', b'ARC\0', 19, 2, 0x1234)
        offset = 12+2*80+7
        rows = struct.pack('<64s4I', b'a\0opaque', 0x62440501, len(first), 0x20000005, offset)
        rows += struct.pack('<64s4I', b'b', 123, len(second), 0xa0000010, offset+len(first)+3)
        return header+rows+b'padding'+first+b'gap'+second+b'opaque trailer'

    def test_append_preserves_unknown_bytes_and_other_stream(self):
        original = self.archive()
        output = replace_member(original, 0, 'Opções de jogo'.encode('utf-16le'))
        self.assertEqual(output[:80], original[:80])
        self.assertEqual(output[92:len(original)], original[92:])
        records = resource.arc_members(output)['entries']
        self.assertEqual(records[0]['offset'], len(original))
        self.assertEqual(records[0]['size_flags_raw'] & 0xf0000000, 0x20000000)
        self.assertEqual(resource.inflate_member(output, records[0]), 'Opções de jogo'.encode('utf-16le'))
        self.assertEqual(resource.inflate_member(output, records[1]), b'unchanged second')

    def test_unchanged_is_byte_exact(self):
        source = self.archive()
        self.assertEqual(replace_member(source, 0, b'first'), source)

    def test_bad_ordinal(self):
        for ordinal in (-1, 2, True):
            with self.assertRaises(ValueError):
                replace_member(self.archive(), ordinal, b'x')

    def test_truncated_stream_rejected(self):
        source = bytearray(self.archive())
        struct.pack_into('<I', source, 12+68, 1)
        with self.assertRaises(ValueError):
            replace_member(bytes(source), 0, b'x')


if __name__ == '__main__':
    unittest.main()
