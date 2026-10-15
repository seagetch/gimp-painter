"""Offline recovery evidence checks; no build, GUI run, or task acceptance."""
import hashlib
import json
import os
from pathlib import Path
import re
import tarfile
import unittest

ROOT = Path(os.environ.get('LEGACY_RECOVERY_REPO', Path(__file__).resolve().parents[2]))
DOC = Path(os.environ.get('LEGACY_RECOVERY_DOC', ROOT / 'migration/baseline/legacy-build.md'))
BASE = ROOT / 'migration/baseline/legacy'
REPLAY = BASE / 'replay-20261015'
REPORT = json.loads((REPLAY / 'report.json').read_text())


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class LegacyRecoveryProcedureTests(unittest.TestCase):
    def test_replayed_recipe_and_lock_match_current_bytes(self):
        self.assertEqual(sha(ROOT / 'tools/build_legacy_reference.sh'), REPORT['recipe']['sha256'])
        self.assertEqual(sha(BASE / 'debian13-package-lock.json'), REPORT['package_lock']['sha256'])
        self.assertEqual(REPORT['recipe']['exit_code'], 0)
        self.assertEqual(REPORT['source'], {
            'gimp': 'afa43fae3e920210146abed514f136fd49f671b5',
            'babl': 'e92ced2c54250af1dddb58f76ff1678d74a30dcf',
            'gegl': '29254dcd875d271043aba57d0e6ebc48ec63c013'})

    def test_all_cached_archive_hashes_match_lock(self):
        lock = json.loads((BASE / 'debian13-package-lock.json').read_text())['packages']
        cache = json.loads((REPLAY / 'cache-verification.json').read_text())
        expected = {x['filename']: x['sha256'] for x in lock}
        actual = {x['filename']: x['sha256'] for x in cache}
        self.assertEqual(len(cache), len(actual))
        self.assertEqual(actual, expected)
        self.assertEqual(len(actual), REPORT['package_lock']['archives_verified'])
        self.assertEqual(len(actual), 560)

    def test_archived_commands_and_observed_dependency_outcomes(self):
        archive = REPLAY / REPORT['build_log_archive']['path']
        self.assertEqual(sha(archive), REPORT['build_log_archive']['sha256'])
        with tarfile.open(archive) as tf:
            def read(name):
                return tf.extractfile('logs/' + name).read().decode()
            for prefix in ('babl', 'gegl', 'gimp'):
                for stage in ('bootstrap', 'configure', 'build', 'install'):
                    self.assertTrue(read(prefix + '-' + stage + '.command').strip())
                    self.assertTrue(read(prefix + '-' + stage + '.log').strip())
            babl = read('babl-tests-isolated.log')
            for key, value in [('TOTAL', 23), ('PASS', 23), ('SKIP', 0), ('FAIL', 0)]:
                self.assertRegex(babl, rf'# {key}:\s+{value}\b')
            gegl = read('gegl-tests-simple.log')
            for key, value in [('passed', 28), ('skipped', 2), ('failed', 1)]:
                self.assertRegex(gegl, rf'tests {key}:\s+{value}\b')
            self.assertIn('FAIL test-gegl-tile', gegl)
            self.assertEqual(read('gegl-tests-simple.exit-code').strip(), '2')
            self.assertIn(REPORT['reference_binary']['version'], read('gimp-version.log'))
            self.assertEqual(read('gimp-binary.sha256').split()[0], REPORT['reference_binary']['sha256'])

    def test_original_reader_success_does_not_replace_negative(self):
        base = ROOT / 'migration/fixtures/legacy-filter-positive'
        manifest = json.loads((base / 'manifest.json').read_text())
        self.assertEqual(manifest['original_binary_sha256'], REPORT['reference_binary']['sha256'])
        self.assertEqual(manifest['independent_reopen_exit_code'], 0)
        self.assertEqual(manifest['original_edge_negative_recheck_exit_code'], 139)
        self.assertFalse(manifest['scheduler_acceptance_passed'])
        self.assertIn('INDEPENDENT_ORIGINAL_READER_REOPEN_PASS', (base / 'independent-reopen.log').read_text())
        self.assertIn('XCF_FILTER_UNSUPPORTED', (base / 'original-edge-negative-recheck.log').read_text())
        self.assertEqual(sha(ROOT / 'migration/fixtures/legacy-runtime/filter-edge.xcf'),
                         REPORT['tests']['independent_original_edge_negative_reader']['fixture_sha256'])
        self.assertEqual(REPORT['tests']['builtin_capture']['scheduler_pending'], 1)
        self.assertEqual(REPORT['tests']['builtin_capture']['progress_end_callbacks'], 0)

    def test_historical_failure_and_current_caveats_remain_visible(self):
        probe = json.loads((BASE / 'probe-2026-10-01.json').read_text())
        self.assertEqual(probe['bootstrap']['exit_code'], 1)
        self.assertFalse(probe['legacy_application_executed'])
        doc = DOC.read_text()
        for text in ('test-gegl-tile', 'exit 139', 'command_exit=-11',
                     '02.014 and', 'static-fallback', 'test-xcf',
                     'physical mouse/tablet', 'no modern-port',
                     'Existing environment: verify before reuse',
                     'INDEPENDENT_ORIGINAL_READER_REOPEN_PASS'):
            self.assertIn(text, doc)
        self.assertIn('command_exit=-11', (ROOT / 'migration/fixtures/legacy-rotation-operations/README.md').read_text())
        self.assertIn('pending=1', (ROOT / 'migration/fixtures/legacy-filter-timeline/README.md').read_text())


if __name__ == '__main__':
    unittest.main()
