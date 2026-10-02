"""Validate sealed executable observations without rerunning the old binary."""
import hashlib
import json
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[2]
FIX = ROOT / 'migration/fixtures/legacy-modes'
class ModeFixtures(unittest.TestCase):
    def test_sealed_files(self):
        for name, record in json.loads((FIX/'manifest.json').read_text()).items():
            data = (FIX/name).read_bytes()
            self.assertEqual(len(data), record['size'], name)
            self.assertEqual(hashlib.sha256(data).hexdigest(), record['sha256'], name)
    def test_complete_executable_capture(self):
        report = json.loads((FIX/'capture-report.json').read_text())
        self.assertEqual(report['source_commit'], 'afa43fae3e920210146abed514f136fd49f671b5')
        self.assertEqual(report['exit_code'], 0)
        self.assertEqual(len(report['outputs']), 112)
        log = (FIX/'capture.log').read_text()
        cases = json.loads((FIX/'cases.json').read_text())
        self.assertEqual(len(cases), 56)
        self.assertEqual({c['mode'] for c in cases}, set(range(23,30)))
        for case in cases:
            self.assertEqual(log.count('MODE_CASE_DONE='+case['id']+'\n'), 1)
        for output in report['outputs']:
            data = (FIX/output['output']).read_bytes()
            self.assertEqual(len(data), 8*8*4)
            self.assertEqual(hashlib.sha256(data).hexdigest(), output['sha256'])
    def test_normal_multiply_capture(self):
        folder = FIX.parent / 'legacy-normal-multiply'
        report = json.loads((folder/'capture-report.json').read_text())
        self.assertEqual(report['exit_code'], 0)
        self.assertEqual(len(report['outputs']), 32)
        self.assertEqual({r['mode'] for r in report['outputs']}, {0,3})
        for name, record in json.loads((folder/'manifest.json').read_text()).items():
            data = (folder/name).read_bytes()
            self.assertEqual(len(data), record['size'], name)
            self.assertEqual(hashlib.sha256(data).hexdigest(), record['sha256'], name)
    def test_projection_and_merge_are_not_conflated(self):
        name = 'mode-25-o100-mnone'
        self.assertNotEqual((FIX/(name+'.rgba')).read_bytes(),
                            (FIX/(name+'-projection.rgba')).read_bytes())
if __name__ == '__main__': unittest.main()
