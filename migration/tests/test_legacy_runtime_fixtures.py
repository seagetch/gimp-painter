"""Integrity and observation checks; never substitutes for a legacy app rerun."""
import hashlib
import json
from pathlib import Path
import struct
import unittest

try:
    from PIL import Image
except ImportError:
    Image = None

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'migration/fixtures/legacy-runtime'
MANIFEST = json.loads((BASE/'manifest.json').read_text())
EXPECTED = json.loads((BASE/'expectations.json').read_text())
# Independent pins for the two genuine captures used by original task 10.002.
# The fixture manifest remains the package-wide integrity check.
CLONE_V4_FIXTURES = {
    'clone-normal-in-group.xcf': (1148, '0fb12d0345bf6c6685562944dae91fb3cc79e84122bb2e67080164d79846d038'),
    'clone-group.xcf': (1164, '6953b26108bb89196c96c2d1a5d38ddd73883c43ba4918a8693d48f867add11e'),
}


class LegacyRuntimeFixtureTests(unittest.TestCase):
    def test_every_file_has_exact_size_and_hash(self):
        actual = {p.name for p in BASE.iterdir() if p.is_file() and p.name != 'manifest.json'}
        self.assertEqual(actual, set(MANIFEST['files']))
        for name,info in MANIFEST['files'].items():
            data=(BASE/name).read_bytes()
            self.assertEqual(len(data),info['bytes'],name)
            self.assertEqual(hashlib.sha256(data).hexdigest(),info['sha256'],name)
        report=(BASE/MANIFEST['build_report']).read_bytes()
        self.assertEqual(hashlib.sha256(report).hexdigest(),MANIFEST['build_report_sha256'])

    def test_success_markers_and_negative_reader_are_distinct(self):
        for name,marker in (('ordinary','LEGACY_ORDINARY_CAPTURE_COMPLETE'),
                            ('clone','LEGACY_CLONE_CAPTURE_COMPLETE')):
            capture=MANIFEST['captures'][name]
            log=(BASE/capture['log']).read_text()
            self.assertIn(marker,log)
            self.assertNotIn('batch command experienced an execution error',log)
            self.assertEqual(capture['exit_code'],0)
        capture=MANIFEST['captures']['filter']
        self.assertEqual(capture['exit_code'],139)
        self.assertEqual(capture['independent_uninstrumented_reopen']['exit_code'],139)
        self.assertIn('FILTER_COMPLETE ',(BASE/capture['log']).read_text())
        self.assertNotIn('LEGACY_FILTER_CAPTURE_COMPLETE',(BASE/capture['log']).read_text())
        self.assertFalse(EXPECTED['filter']['original_reader_reopen_succeeded'])

    def test_pinned_legacy_headers_and_dimensions(self):
        for name in ('ordinary-layers.xcf','clone-normal-in-group.xcf','clone-group.xcf','filter-edge.xcf'):
            data=(BASE/name).read_bytes()
            self.assertEqual(data[:9],b'gimp xcf ')
            self.assertEqual(data[13],0)
            width,height=struct.unpack_from('>II',data,14)
            self.assertEqual((width,height),(96,80) if name.startswith('ordinary') else (64,64))
        self.assertEqual((BASE/'clone-group.xcf').read_bytes()[9:13],b'v004')
        self.assertEqual((BASE/'filter-edge.xcf').read_bytes()[9:13],b'v004')

    def _assert_clone_v4_header(self, data):
        # Fixture-specific assertions, not a dialect classifier or XCF parser.
        # Keep hash checking separate so mutation controls exercise the offsets.
        self.assertEqual(data[:14], b'gimp xcf v004\0')
        self.assertEqual(struct.unpack_from('>III', data, 14), (64, 64, 0))
        self.assertEqual(struct.unpack_from('>I', data, 26)[0], 17,
                         'PROP_COMPRESSION is at 26; no precision word')
        self.assertEqual(struct.unpack_from('>I', data, 30)[0], 1,
                         'compression payload length is at 30')
        self.assertEqual(data[34], 1, 'RLE compression byte is at 34')

    def test_clone_v4_properties_start_at_26_without_precision(self):
        prefix = bytes.fromhex(
            '67696d7020786366207630303400 '
            '00000040 00000040 00000000 00000011 00000001 01')
        for name, (size, digest) in CLONE_V4_FIXTURES.items():
            with self.subTest(fixture=name):
                data = (BASE/name).read_bytes()
                self._assert_clone_v4_header(data)
                self.assertEqual(data[:35], prefix)
                self.assertEqual(len(data), size)
                self.assertEqual(hashlib.sha256(data).hexdigest(), digest)

    def test_clone_v4_header_rejects_shifted_or_changed_fixture_bytes(self):
        for name in CLONE_V4_FIXTURES:
            data = (BASE/name).read_bytes()
            self._assert_clone_v4_header(data)
            controls = {
                'properties moved to 27': data[:26] + b'\0' + data[26:],
                'precision inserted at 26': data[:26] + struct.pack('>I', 0) + data[26:],
            }
            for label, offset in (('version', 12), ('signature terminator', 13),
                                  ('width', 17), ('height', 21), ('base type', 25),
                                  ('property tag', 29), ('property size', 33),
                                  ('compression payload', 34)):
                controls[label] = data[:offset] + bytes([data[offset] ^ 1]) + data[offset+1:]
            for label, changed in controls.items():
                with self.subTest(fixture=name, mutation=label):
                    with self.assertRaises(AssertionError):
                        self._assert_clone_v4_header(changed)

    @unittest.skipIf(Image is None,'Pillow is required for decoded PNG checks')
    def test_measured_png_pixels(self):
        for name,expected in EXPECTED['ordinary']['pngs'].items():
            with Image.open(BASE/name) as source:
                image=source.convert('RGBA')
                data=image.tobytes()
                self.assertEqual(image.size,(expected['width'],expected['height']))
                self.assertEqual(hashlib.sha256(data).hexdigest(),expected['pixel_sha256'])
                pixels={data[i:i+4] for i in range(0,len(data),4)}
                self.assertEqual(len(pixels),78)
                self.assertGreater(len(pixels),1)

    def test_clone_observations_are_backed_by_log(self):
        log=(BASE/'clone-capture.log').read_text()
        for label,fields in EXPECTED['clone_pixels'].items():
            self.assertIn('CLONE_PIXEL '+label+' ',log)
            line=next(x for x in log.splitlines() if x.startswith('CLONE_PIXEL '+label+' '))
            for key,values in fields.items():
                self.assertIn(key+'='+','.join(map(str,values)),line)
        self.assertEqual(EXPECTED['clone_pixels']['show_mask']['clone_rgba'],[128,128,128,255])
        self.assertEqual(EXPECTED['clone_delete_undo']['after_remove']['source_attached'],0)
        self.assertEqual(EXPECTED['clone_delete_undo']['after_undo']['source_attached'],1)

    def test_dissolve_exact_bytes(self):
        record=EXPECTED['dissolve'];data=(BASE/record['file']).read_bytes()
        self.assertEqual(len(data),256)
        self.assertEqual(set(data),{0,255})
        self.assertEqual(sum(value != 0 for value in data),35)
        self.assertEqual(hashlib.sha256(data).hexdigest(),record['sha256'])
        self.assertIn(record['sha256'],(BASE/'clone-capture.log').read_text())


if __name__ == '__main__':
    unittest.main()
