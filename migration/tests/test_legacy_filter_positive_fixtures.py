"""Captured original-runtime evidence checks, not a substitute for rerunning GIMP."""
import hashlib
import json
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'migration/fixtures/legacy-filter-positive'
MANIFEST = json.loads((BASE / 'manifest.json').read_text())
EXPECTED = json.loads((BASE / 'expectations.json').read_text())


class LegacyFilterPositiveFixtureTests(unittest.TestCase):
    def test_exact_evidence_hashes(self):
        self.assertEqual({p.name for p in BASE.iterdir() if p.is_file() and p.name != 'manifest.json'}, set(MANIFEST['files']))
        for name, info in MANIFEST['files'].items():
            data = (BASE / name).read_bytes()
            self.assertEqual(len(data), info['bytes'], name)
            self.assertEqual(hashlib.sha256(data).hexdigest(), info['sha256'], name)
        report = BASE / MANIFEST['build_report']
        self.assertEqual(hashlib.sha256(report.read_bytes()).hexdigest(), MANIFEST['build_report_sha256'])

    def test_original_positive_and_negative_are_separate(self):
        log = (BASE / 'capture.log').read_text()
        self.assertIn('LEGACY_BUILTIN_FILTER_CAPTURE_COMPLETE', log)
        self.assertIn('INDEPENDENT_ORIGINAL_READER_REOPEN_PASS', (BASE / 'independent-reopen.log').read_text())
        self.assertNotIn('batch command experienced an execution error', (BASE / 'independent-reopen.log').read_text())
        self.assertEqual(MANIFEST['capture_exit_code'], 0)
        self.assertEqual(MANIFEST['independent_reopen_exit_code'], 0)
        self.assertEqual(MANIFEST['original_edge_negative_recheck_exit_code'], 139)
        self.assertIn('XCF_FILTER_UNSUPPORTED', (BASE / 'original-edge-negative-recheck.log').read_text())

    def test_real_xcf_header_and_supported_wire_arguments(self):
        data = (BASE / 'filter-brightness-contrast.xcf').read_bytes()
        self.assertEqual(data[:14], b'gimp xcf v004\0')
        self.assertEqual(struct.unpack_from('>II', data, 14), (64, 64))
        name = b'gimp-brightness-contrast\0'
        self.assertEqual(data.count(name), 1)
        offset = data.index(name)
        self.assertEqual(struct.unpack_from('>I', data, offset - 4)[0], len(name))
        offset += len(name)
        self.assertEqual(struct.unpack_from('>II', data, offset), (10, 0))
        self.assertEqual(struct.unpack_from('>IIi', data, offset + 8), (2, 4, 25))
        self.assertEqual(struct.unpack_from('>IIi', data, offset + 20), (2, 4, -10))
        self.assertEqual(struct.unpack_from('>II', data, offset + 32), (0, 0))

    def test_every_cached_pixel_verified_by_original_control_and_reader(self):
        log = (BASE / 'capture.log').read_text()
        sha = EXPECTED['filter']['cache_rgba_sha256']
        for prefix in ('CONTROL_ORIGINAL_PDB_CACHE_SHA256', 'FILTER_CACHE_SHA256', 'FILTER_REOPEN_CACHE_SHA256'):
            self.assertIn(prefix + ' ' + sha, log)
        source_sha = EXPECTED['lower_layer']['rgba_sha256']
        self.assertIn('LOWER_LAYER_SHA256 ' + source_sha, log)
        self.assertIn('REOPEN_LOWER_LAYER_SHA256 ' + source_sha, log)
        self.assertIn('FILTER_CACHE_PIXEL x=12 y=12 rgba=200,78,118,255', log)

    def test_no_scheduler_completion_claim(self):
        self.assertFalse(MANIFEST['scheduler_acceptance_passed'])
        self.assertFalse(EXPECTED['scheduler_acceptance_passed'])
        self.assertEqual(EXPECTED['filter']['pending_after_execution'], 1)
        self.assertEqual(EXPECTED['filter']['progress_end_callbacks'], 0)
        log = (BASE / 'capture.log').read_text()
        self.assertIn('FILTER_SAVE_STATE pending=1 completions=0', log)
        self.assertNotIn('FILTER_COMPLETE time_us=', log)

    def test_pristine_replay_retains_known_dependency_failure(self):
        report_path = BASE / MANIFEST['build_report']
        report = json.loads(report_path.read_text())
        self.assertEqual(report['recipe']['exit_code'], 0)
        self.assertEqual(report['package_lock']['archives_verified'], 560)
        self.assertEqual(report['tests']['babl'], {'passed': 23, 'failed': 0})
        self.assertEqual(report['tests']['gegl_simple']['failed'], 1)
        self.assertEqual(report['reference_binary']['sha256'], MANIFEST['original_binary_sha256'])
        archive = report_path.parent / report['build_log_archive']['path']
        self.assertEqual(hashlib.sha256(archive.read_bytes()).hexdigest(), report['build_log_archive']['sha256'])


if __name__ == '__main__':
    unittest.main()
