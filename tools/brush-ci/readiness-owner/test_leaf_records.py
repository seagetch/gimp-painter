"""Strict numeric leaf records; no GUI or debugger execution."""
import importlib.util
import json
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('leaf_records', Path(__file__).with_name('leaf_records.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

def valid_lines():
    return (['LEAF_DIAG summary 1 1 1 1 0'] +
            [f'LEAF_DIAG family {i} 1 1 1 1 1 0 0 0 0 0' for i in range(1, 4)] +
            [f'LEAF_DIAG metric {i} {j} 0 0 0 0 0 0 0' for i in range(1, 4) for j in range(5)] +
            ['LEAF_DIAG union 0 0 0 0 0 0 0', 'LEAF_DIAG complete'])

def parse(lines):
    r = m.LeafRecords()
    for line in lines:
        r.consume(line)
    return r.result()

class LeafRecords(unittest.TestCase):
    def test_complete_numeric_zero_observation(self):
        result = parse(valid_lines())
        self.assertEqual(result['status'], 'captured')
        self.assertTrue(result['coverage_complete'])
        self.assertFalse(result['acceptance_eligible'])

    def test_exact_nested_nonadditive_metrics(self):
        lines = valid_lines()
        lines[5] = 'LEAF_DIAG metric 1 1 2 2 0 1 1 50 50'
        lines[11] = 'LEAF_DIAG metric 2 2 1 1 0 0 1 20 20'
        lines[-2] = 'LEAF_DIAG union 3 3 0 2 1 50 50'
        self.assertEqual(parse(lines)['status'], 'captured')

    def test_late_copied_slots_restored(self):
        lines = valid_lines()
        lines[1] = 'LEAF_DIAG family 1 1 1 2 1 2 1 0 0 0 0'
        self.assertEqual(parse(lines)['status'], 'captured')
        lines[1] = 'LEAF_DIAG family 1 1 1 2 1 1 1 0 0 0 0'
        self.assertEqual(parse(lines)['status'], 'inconclusive')

    def test_unknown_overrides_are_explicit_partial_coverage(self):
        lines = valid_lines()
        lines[0] = 'LEAF_DIAG summary 1 1 0 1 4'
        lines[1] = 'LEAF_DIAG family 1 1 2 2 1 1 0 1 0 0 0'
        result = parse(lines)
        self.assertEqual(result['status'], 'captured')
        self.assertFalse(result['coverage_complete'])
        lines[0] = 'LEAF_DIAG summary 1 1 1 1 4'
        self.assertEqual(parse(lines)['status'], 'inconclusive')

    def test_unsafe_flags_never_capture(self):
        for flag in (1, 2, 8, 16, 32, 64, 128, 256, 512, 1023):
            with self.subTest(flag=flag):
                lines = valid_lines()
                lines[0] = f'LEAF_DIAG summary 1 1 0 0 {flag}'
                self.assertEqual(parse(lines)['status'], 'inconclusive')

    def test_disabled_or_failed_restore_inconclusive(self):
        for enabled, restored in ((0, 1), (1, 0), (0, 0)):
            lines = valid_lines()
            lines[0] = f'LEAF_DIAG summary 1 {enabled} 0 {restored} 1'
            self.assertEqual(parse(lines)['status'], 'inconclusive')

    def test_every_target_chunk_split(self):
        stream = '\n'.join(valid_lines()) + '\n'
        for i in range(len(stream) + 1):
            with self.subTest(split=i):
                self.assertEqual(parse(['@' + json.dumps(stream[:i]), '@' + json.dumps(stream[i:])])['status'], 'captured')

    def test_missing_duplicate_reordered_rows(self):
        original = valid_lines()
        for i in range(len(original)):
            for lines in (original[:i] + original[i+1:], original[:i] + [original[i]] + original[i:]):
                self.assertEqual(parse(lines)['status'], 'inconclusive')
        lines = valid_lines(); lines[4], lines[5] = lines[5], lines[4]
        self.assertEqual(parse(lines)['status'], 'inconclusive')

    def test_malformed_or_private_payload_rejected(self):
        for bad in ('@null', '@not-json', '@42', 'LEAF_DIAG summary 1 1 1 1 /secret',
                    'LEAF_DIAG summary 1 1 1 1 0x12', 'LEAF_DIAG summary 1 1 1 1 -1',
                    'LEAF_DIAG summary 1 1 1 1 512', 'LEAF_DIAG summary 1 1 1 1 0 extra',
                    'LEAF_DIAGX complete', '@' + json.dumps('x' * 4097)):
            self.assertEqual(parse([bad] + valid_lines())['status'], 'inconclusive')
        self.assertEqual(parse(['~' + json.dumps('\n'.join(valid_lines()))])['status'], 'inconclusive')

    def test_transport_mixing_and_unterminated_chunk(self):
        lines = valid_lines()
        for mixed in ([lines[0]] + ['@' + json.dumps('\n'.join(lines[1:]) + '\n')],
                      ['@' + json.dumps('\n'.join(lines) + '\ntrailing')]):
            self.assertEqual(parse(mixed)['status'], 'inconclusive')

    def test_invalid_metric_invariants(self):
        for fields in ('1 0 0 0 1 5 5', '1 1 0 1 1 5 5', '0 0 0 0 0 1 1',
                       '1 1 0 0 1 4 5', '1 1 0 0 1 9223372036854775808 0',
                       '1000000001 1000000001 0 0 1000000001 1 1'):
            lines = valid_lines(); lines[4] = 'LEAF_DIAG metric 1 0 ' + fields
            self.assertEqual(parse(lines)['status'], 'inconclusive')

    def test_family_summary_consistency(self):
        for row in ('LEAF_DIAG family 1 0 1 1 1 1 0 0 0 0 0',
                    'LEAF_DIAG family 1 1 1 1 0 0 0 0 0 0 0',
                    'LEAF_DIAG family 1 1 1 1 1 1 0 0 0 0 1'):
            lines = valid_lines(); lines[1] = row
            self.assertEqual(parse(lines)['status'], 'inconclusive')
        lines = valid_lines(); lines[0] = 'LEAF_DIAG summary 1 1 0 1 0'
        self.assertEqual(parse(lines)['status'], 'inconclusive')

    def test_union_cannot_contradict_buckets(self):
        lines = valid_lines(); lines[-2] = 'LEAF_DIAG union 1 1 0 0 1 5 5'
        self.assertEqual(parse(lines)['status'], 'inconclusive')

if __name__ == '__main__':
    unittest.main()
