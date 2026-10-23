#!/usr/bin/env python3
"""Mutation checks for historical/current error-boundary evidence separation."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import runpy
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class EvidenceSeparation(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        evidence = Path('migration/tests/error-boundary')
        shutil.copytree(ROOT / evidence, self.root / evidence)
        self.evidence = self.root / evidence
        current = json.loads((self.evidence / 'current-foundation/normal.json').read_text())
        for name in set(current['source_sha256']) | {
            'tasks.md', 'migration/inventory/legacy-port-work-items.tsv'}:
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)
        namespace = runpy.run_path(str(ROOT / 'tools/check_painter_error_boundary.py'))
        self.check = namespace['main']
        self.check.__globals__['ROOT'] = self.root

    def verify(self):
        with contextlib.redirect_stdout(io.StringIO()):
            self.check()

    def reseal_current(self):
        current = self.evidence / 'current-foundation'
        manifest = {str(p.relative_to(current)): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in current.rglob('*') if p.is_file() and p.name != 'artifact-sha256.json'}
        (current / 'artifact-sha256.json').write_text(json.dumps(manifest))

    def test_valid_completed_successors_and_distinct_history(self):
        self.verify()

    def test_historical_archive_corruption_rejected(self):
        with (self.evidence / 'historical-source.tar.gz').open('ab') as stream:
            stream.write(b'corrupt')
        with self.assertRaises(AssertionError):
            self.verify()

    def test_current_source_drift_rejected(self):
        with (self.root / 'app/painter/boundary.hpp').open('a') as stream:
            stream.write('\n// changed after the native checkpoint\n')
        with self.assertRaises(AssertionError):
            self.verify()

    def test_missing_current_test_rejected_even_after_manifest_reseal(self):
        path = self.evidence / 'current-foundation/normal.json'
        result = json.loads(path.read_text())
        result['output'][-1] = '\n'.join(line for line in result['output'][-1].splitlines()
                                        if '/painter/boundary/exception' not in line)
        path.write_text(json.dumps(result))
        self.reseal_current()
        with self.assertRaises(AssertionError):
            self.verify()

    def test_successor_dependency_removal_rejected(self):
        path = self.root / 'tasks.md'
        lines = path.read_text().splitlines(keepends=True)
        for i, line in enumerate(lines):
            if line.startswith('| 05.013/class-init-error |'):
                fields = line.split('|')
                fields[4] = fields[4].replace('05.013, ', '')
                lines[i] = '|'.join(fields)
        path.write_text(''.join(lines))
        with self.assertRaises(AssertionError):
            self.verify()

    def test_historical_log_tamper_rejected(self):
        (self.evidence / 'native-boundary.log').write_text('forged pass\n')
        with self.assertRaises(AssertionError):
            self.verify()


if __name__ == '__main__':
    unittest.main()
