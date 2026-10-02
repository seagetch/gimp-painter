#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import gzip
import hashlib
import json
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[2] / 'migration/fixtures/legacy-smudge'

class SmudgeFixtureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = gzip.decompress((ROOT / 'pixels.tsv.gz').read_bytes())
        lines = cls.raw.splitlines()
        assert len(lines) == 193 and lines[-1] == b'SMUDGE_CAPTURE_COMPLETE'
        cls.pixels = {}
        for line in lines[:-1]:
            _, case, phase, channels, encoded = line.split()
            case, channels = int(case), int(channels)
            assert channels == case // 12 + 1
            pixels = bytes.fromhex(encoded.decode())
            assert len(pixels) == 64 * 48 * channels
            cls.pixels[case, phase.decode()] = pixels
        assert len(cls.pixels) == 192

    def test_manifest_and_repeat(self):
        report = json.loads((ROOT / 'runtime.json').read_text())
        for path, expected in report['fixture_sha256'].items():
            self.assertEqual(hashlib.sha256((ROOT / path).read_bytes()).hexdigest(), expected, path)
        self.assertEqual(hashlib.sha256(self.raw).hexdigest(), report['independent_repeat']['normalized_sha256'])

    def test_complete_undo_redo(self):
        for case in range(48):
            self.assertEqual(self.pixels[case, 'initial'], self.pixels[case, 'undo'])
            self.assertEqual(self.pixels[case, 'finish'], self.pixels[case, 'redo'])

    def test_blending_is_not_inert(self):
        for shape in range(4):
            for rate in range(3):
                for dynamic in range(2):
                    case = shape * 12 + rate * 4 + dynamic
                    self.assertNotEqual(self.pixels[case, 'finish'], self.pixels[case + 2, 'finish'])

    def test_raw_normalization(self):
        lines = gzip.decompress((ROOT / 'runtime-capture.log.gz').read_bytes()).splitlines()
        normalized = b'\n'.join(x for x in lines if x.startswith(b'SMUDGE ') or x == b'SMUDGE_CAPTURE_COMPLETE') + b'\n'
        self.assertEqual(normalized, self.raw)

if __name__ == '__main__':
    unittest.main()
