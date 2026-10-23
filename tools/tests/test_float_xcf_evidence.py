"""Semantic controls for the bounded float-XCF receipt (not new native tests)."""
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest
import hashlib

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / 'migration/tests/painter-float-xcf'
spec = importlib.util.spec_from_file_location('float_xcf_evidence', EVIDENCE / 'verify.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class EvidenceControls(unittest.TestCase):
    def test_valid(self):
        self.assertEqual(module.verify(EVIDENCE)['parent'], 'DOING')

    def reject(self, name, mutate):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'evidence'
            shutil.copytree(EVIDENCE, root)
            path = root / name
            data = json.loads(path.read_text())
            mutate(data)
            path.write_text(json.dumps(data))
            inventory = json.loads((root / 'artifact-sha256.json').read_text())
            inventory[name] = hashlib.sha256(path.read_bytes()).hexdigest()
            (root / 'artifact-sha256.json').write_text(json.dumps(inventory))
            with self.assertRaises((AssertionError, KeyError)):
                module.verify(root)

    def test_parent_not_closed(self):
        self.reject('manifest.json', lambda d: d.update(parent_status='DONE'))

    def test_no_missing_native_case(self):
        self.reject('normal.json', lambda d: d[0].update(stdout=d[0]['stdout'].replace('ok 1 ', 'ok 1 # SKIP ', 1)))

    def test_sanitizer_failure(self):
        self.reject('sanitizer-build.json', lambda d: d['results']['xcf'].update(exit_code=1))

    def test_source_changed_during_run(self):
        self.reject('sanitizer-build.json', lambda d: d.update(changed_during_run=['app/xcf/painter-xcf-preserve.cpp']))

    def test_no_false_instrumentation(self):
        self.reject('sanitizer-build.json', lambda d: d['instrumented_sources'].pop())

    def test_mutant_must_fail(self):
        self.reject('mutations/mutations.json', lambda d: d['cases'][0].update(rejected=False))

    def test_original_checkpoint_unchanged(self):
        self.reject('extension-acceptance.json', lambda d: d.update(original_185_field_checkpoint_changed=True))

    def test_prior_fixtures_identical(self):
        self.reject('prior-fixture-sha256.json', lambda d: d.pop(next(iter(d))))

    def test_unknown_backward_editing_not_claimed(self):
        self.reject('prior-report.json', lambda d: d.update(preserved='backward semantics and editing guaranteed'))

    def test_source_archive_union(self):
        self.reject('sanitizer-build.json', lambda d: d['sources_sha256'].update({'app/xcf/painter-xcf-preserve.cpp': '0' * 64}))


if __name__ == '__main__':
    unittest.main()
