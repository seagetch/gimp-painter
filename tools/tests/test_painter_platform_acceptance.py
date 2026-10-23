#!/usr/bin/env python3
"""Negative controls for the original 07.014 persisted native acceptance."""
import copy
import gzip
import json
from pathlib import Path
import sys
import unittest
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_painter_platform_acceptance import ROOT, DIRECTORY, validate


class NativeAcceptance(unittest.TestCase):
    def setUp(self):
        self.index = json.loads((ROOT / DIRECTORY / 'native-acceptance.json').read_text())
        self.reports = {r['target']: json.loads(gzip.decompress((ROOT / r['report']).read_bytes())) for r in self.index['targets']}

    def test_actual_native_snapshots(self):
        self.assertEqual(validate(), [])

    def test_incomplete_and_duplicate_targets(self):
        for rows in [self.index['targets'][:-1], self.index['targets'][:3] + [self.index['targets'][0]]]:
            bad = copy.deepcopy(self.index);bad['targets'] = rows
            self.assertTrue(validate(index=bad, reports=self.reports))

    def test_report_byte_drift(self):
        self.index['targets'][0]['report_sha256'] = '0' * 64
        self.assertTrue(validate(index=self.index))

    def test_source_drift_and_undocumented_conversion(self):
        self.reports['windows-x86_64']['source_sha256']['app/painter/binding-store.cpp'] = '0' * 64
        self.assertTrue(validate(index=self.index, reports=self.reports))
        self.setUp()
        next(r for r in self.index['targets'] if r['target'] == 'windows-x86_64')['exact_LF_to_CRLF_checkout_matches'] = []
        self.assertTrue(validate(index=self.index, reports=self.reports))

    def test_each_native_execution_gate(self):
        mutations = [
            lambda d: d.update(status='FAIL'),
            lambda d: d['host'].update(architecture='arm64'),
            lambda d: d['ci'].update(GITHUB_SHA='0' * 40),
            lambda d: d['commands'][0].update(exit_code=1),
            lambda d: d['foundation_tests'].pop(),
            lambda d: d.update(abi_result='PLATFORM_ABI_PASS pointer=4'),
            lambda d: d['exports'].update(private_cpp_exports=['GimpPainter::leak()']),
            lambda d: d.update(negative_export_control=[]),
            lambda d: d.update(unmangled_c_symbols=[]),
            lambda d: next(iter(d['binaries'].values())).update(format='PE'),
            lambda d: d.update(runtime_dependencies=''),
        ]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                reports = copy.deepcopy(self.reports);mutation(reports['linux-x86_64'])
                self.assertTrue(validate(index=self.index, reports=reports))


if __name__ == '__main__':
    unittest.main()
