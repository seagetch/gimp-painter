#!/usr/bin/env python3
"""Check retained genuine-old-parser evidence; does not execute the old parser."""
import hashlib
import json
from pathlib import Path
import struct
import unittest

HERE = Path(__file__).resolve().parent


class Evidence(unittest.TestCase):
    def test_omitted_input_values_are_not_serialized(self):
        for kind, tag in [('int8array', 6), ('floatarray', 7), ('drawable', 10)]:
            expected = struct.pack('>7I', tag, 0, 2, 4, 305419896, 0, 0)
            for variant in range(3):
                self.assertEqual((HERE / 'fixtures' / f'{kind}-input{variant}.bin').read_bytes(), expected)

    def test_null_string_ambiguity(self):
        self.assertEqual((HERE / 'fixtures/floatarray-input0.bin').read_bytes(),
                         (HERE.parent / 'legacy-string-reader/fixtures/null-writer-tag7.bin').read_bytes())

    def test_actual_native_capture_manifest(self):
        manifest = json.loads((HERE / 'manifest.json').read_text())
        self.assertEqual(manifest['source_commit'], 'afa43fae3e920210146abed514f136fd49f671b5')
        self.assertEqual(manifest['patch_sha256'], hashlib.sha256((HERE / 'capture.patch').read_bytes()).hexdigest())
        self.assertTrue(all(run['exit'] == 0 for run in manifest['runs']))
        for name, digest in manifest['files'].items():
            if name == 'capture.log' or name.startswith('fixtures/'):
                self.assertEqual(hashlib.sha256((HERE / name).read_bytes()).hexdigest(), digest)
        log = (HERE / 'capture.log').read_text()
        self.assertEqual(log.count('ok=1 argc=2 cp=28 ftell=28 bytes=28'), 9)
        self.assertIn('ORIGINAL_OMITTED_ARGUMENT_READER_ALIGNMENT_PASS cases=9', log)


if __name__ == '__main__':
    unittest.main()
