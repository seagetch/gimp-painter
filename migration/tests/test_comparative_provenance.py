"""Read-only provenance evidence checks; no application or legal acceptance claim.

Normal use: python3 -m unittest discover -s migration/tests -p test_comparative_provenance.py -v
Review-only overrides: COMPARATIVE_REPO, COMPARATIVE_FIXTURES,
COMPARATIVE_TIMELINE_STAGING. No writes, fetches, or live capture execution.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import unittest

REPO = Path(os.environ.get('COMPARATIVE_REPO', Path(__file__).resolve().parents[2]))
FIXTURES = REPO / 'migration/fixtures'
REGISTER = Path(os.environ.get('COMPARATIVE_FIXTURES', FIXTURES))
NOTICES = REGISTER / 'provenance-notices'
PIN = 'afa43fae3e920210146abed514f136fd49f671b5'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def source(path):
    # Explicitly refuse lazy fetching: verification is local and read-only.
    env = dict(os.environ, GIT_NO_LAZY_FETCH='1', GIT_TERMINAL_PROMPT='0')
    return subprocess.check_output(['git', '-C', str(REPO), 'show', PIN + ':' + path], env=env)

def package_path(name):
    if name == 'legacy-filter-timeline' and os.environ.get('COMPARATIVE_TIMELINE_STAGING'):
        return Path(os.environ['COMPARATIVE_TIMELINE_STAGING'])
    return FIXTURES / name

class ComparativeProvenance(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.notices = json.loads((NOTICES / 'manifest.json').read_text())
        cls.assets = json.loads((REGISTER / 'comparative-assets.json').read_text())

    def test_source_pin_matches_baseline(self):
        baseline = json.loads((REPO / 'migration/baseline/baseline.json').read_text())
        self.assertEqual(baseline['source']['commit'], PIN)
        self.assertEqual(self.notices['source_commit'], PIN)
        self.assertEqual(self.assets['source_commit'], PIN)

    def test_exact_source_notice_bytes_and_ranges(self):
        for row in self.notices['records']:
            with self.subTest(path=row['source_path']):
                raw = source(row['source_path'])
                self.assertEqual(sha(raw), row['source_sha256'])
                self.assertEqual(len(raw), row['source_bytes'])
                if row['notice_path'] is None:
                    self.assertIsNone(re.match(rb'/\*.*?\*/', raw, re.S))
                    continue
                data = (NOTICES / row['notice_path']).read_bytes()
                self.assertEqual(data, raw[row['start_byte']:row['end_byte_exclusive']])
                self.assertEqual(len(data), row['notice_bytes'])
                self.assertEqual(sha(data), row['notice_sha256'])
                if row['notice_path'].startswith('headers/'):
                    self.assertEqual(data, re.match(rb'/\*.*?\*/(?:\r?\n)?', raw, re.S).group())

    def test_complete_primary_notice_files(self):
        for name in ['LICENSE', 'COPYING', 'libgimp/COPYING',
                     'data/mypaint-brushes/deevad/readme.txt',
                     'data/mypaint-brushes/kaerhon/ReadMe.txt']:
            self.assertEqual((NOTICES / 'original' / name).read_bytes(), source(name))

    def test_per_file_license_distinctions_preserved(self):
        brush = (NOTICES / 'headers/app/paint/mypaintbrush-brush.hpp.notice').read_bytes()
        self.assertIn(b'Martin Renold', brush)
        self.assertIn(b'copyright notice and this permission notice appear in all copies.', brush)
        self.assertIn(b'either version 2', (NOTICES / 'headers/app/base/glib-cxx-impl.hpp.notice').read_bytes())
        self.assertIn(b'either version 3', (NOTICES / 'headers/app/tests/test-xcf.c.notice').read_bytes())

    def test_original_resource_copies_are_not_relabelled_synthetic(self):
        self.assertEqual(len(self.assets['original_resource_copies']), 12)
        for row in self.assets['original_resource_copies']:
            with self.subTest(path=row['path']):
                data = (package_path(row['package']) / row['path']).read_bytes()
                self.assertEqual(data, source(row['source_path']))
                self.assertEqual(sha(data), row['sha256'])
                self.assertTrue(row['notice_scope'])

    def test_existing_brush_package_notices_match_shared_bundle(self):
        folder = package_path('legacy-brush-strokes')
        for local, original in [('LICENSE', 'LICENSE'), ('COPYING', 'COPYING'),
                                ('deevad-readme.txt', 'data/mypaint-brushes/deevad/readme.txt')]:
            self.assertEqual((folder / local).read_bytes(), (NOTICES / 'original' / original).read_bytes())

    def test_capture_overlay_original_files_have_notice_inventory(self):
        covered = {r['source_path'] for r in self.notices['records']}
        for row in self.assets['package_evidence']:
            folder = package_path(row['package'])
            self.assertTrue(folder.is_dir(), str(folder))
            self.assertTrue((folder / row['integrity_record']).is_file())
            self.assertEqual(sha((folder / row['integrity_record']).read_bytes()), row['integrity_record_sha256'])
            for patch in folder.glob('*.patch'):
                for fragment in re.split(r'(?=^diff --git )', patch.read_text(), flags=re.M):
                    match = re.match(r'diff --git a/(.*?) b/', fragment)
                    if not match or re.search(r'^new file mode ', fragment, re.M):
                        continue
                    self.assertIn(match.group(1), covered, str(patch))

if __name__ == '__main__':
    unittest.main()
