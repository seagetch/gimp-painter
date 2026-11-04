"""Strict bounded image identity parser; no native GUI or debugger."""
import importlib.util
import json
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('image_records', Path(__file__).with_name('image_records.py'))
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)

def meta(i, phase, valid=True, host=1):
    fields = [i, phase, 1, 5, 1, -1, 1, 1, 1, 1, 128, 1, 3, host] if valid else [i, phase, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0]
    return 'IMAGE_DIAG meta ' + ' '.join(map(str, fields))

def valid_lines(images=1, windows=1):
    lines = [f'IMAGE_DIAG summary 1 1 0 {windows} {windows+images} {images} {images*2} {images*2} 0']
    lines += [f'IMAGE_DIAG window {i} 1' for i in range(1, windows+1)]
    for i in range(1, images+1):
        lines += [f'IMAGE_DIAG image {i} 1 1 0 0', meta(i, 0), meta(i, 1), meta(i, 2),
                  f'IMAGE_DIAG metric {i} 1 1 1 0 0 20 20', f'IMAGE_DIAG metric {i} 2 1 1 0 0 35 35']
    return lines + [meta(0, 1, False), meta(0, 2, False), 'IMAGE_DIAG metric 0 0 0 0 0 0 0 0', 'IMAGE_DIAG complete']

def parse(lines):
    records = m.ImageRecords()
    for line in lines: records.consume(line)
    return records.result()

class ImageRecords(unittest.TestCase):
    def test_complete_first_and_repeat_observation(self):
        result = parse(valid_lines())
        self.assertEqual(result['status'], 'captured')
        self.assertTrue(result['identity_coverage_complete'])
        self.assertFalse(result['acceptance_eligible'])
        self.assertEqual(len(result['records']), 12)

    def test_empty_snapshot_is_not_an_exact_one_window_assumption(self):
        self.assertEqual(parse(valid_lines(0, 0))['status'], 'captured')

    def test_unknown_calls_are_not_unique_images(self):
        lines = valid_lines(0, 0)
        lines[0] = 'IMAGE_DIAG summary 1 1 0 0 0 0 3 3 0'
        lines[1:4] = [meta(0, 1, host=17), meta(0, 2, host=18), 'IMAGE_DIAG metric 0 0 3 3 0 1 60 30']
        result = parse(lines)
        self.assertEqual(result['status'], 'captured')
        self.assertEqual(result['untracked_calls'], 3)
        self.assertFalse(result['identity_coverage_complete'])

    def test_destroy_and_alive_are_independent(self):
        for alive, destroyed in ((0, 0), (0, 1), (1, 1)):
            lines = valid_lines(); lines[2] = f'IMAGE_DIAG image 1 1 {alive} {destroyed} 0'
            self.assertEqual(parse(lines)['status'], 'captured')

    def test_no_calls_have_canonical_empty_first_last(self):
        lines = valid_lines(); lines[0] = 'IMAGE_DIAG summary 1 1 0 1 2 1 0 0 0'
        lines[4:8] = [meta(1, 1, False), meta(1, 2, False), 'IMAGE_DIAG metric 1 1 0 0 0 0 0 0', 'IMAGE_DIAG metric 1 2 0 0 0 0 0 0']
        self.assertEqual(parse(lines)['status'], 'captured')

    def test_snapshot_origin_must_match(self):
        lines = valid_lines(); lines[3] = meta(1, 0, host=17)
        self.assertEqual(parse(lines)['status'], 'inconclusive')

    def test_metadata_change_and_reparent_are_observed_not_rejected(self):
        lines = valid_lines(); lines[2] = 'IMAGE_DIAG image 1 1 1 0 1'; lines[5] = meta(1, 2, host=18)
        self.assertEqual(parse(lines)['status'], 'captured')

    def test_semantic_missing_is_partial_only(self):
        lines = valid_lines(); lines[0] = 'IMAGE_DIAG summary 1 1 2048 1 2 1 2 2 0'
        result = parse(lines)
        self.assertEqual(result['status'], 'captured')
        self.assertFalse(result['identity_coverage_complete'])

    def test_fault_flags_never_capture(self):
        for flag in (1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 4096, 8191):
            lines = valid_lines(); lines[0] = f'IMAGE_DIAG summary 1 1 {flag} 1 2 1 2 2 0'
            self.assertEqual(parse(lines)['status'], 'inconclusive')

    def test_record_order_missing_duplicate_and_ranges(self):
        original = valid_lines()
        for i in range(len(original)):
            self.assertEqual(parse(original[:i] + original[i+1:])['status'], 'inconclusive')
            self.assertEqual(parse(original[:i] + [original[i]] + original[i:])['status'], 'inconclusive')
        for row in ('IMAGE_DIAG summary 1 1 0 17 2 1 2 2 0', 'IMAGE_DIAG summary 1 1 0 1 4097 1 2 2 0',
                    'IMAGE_DIAG summary 1 1 0 1 2 257 2 2 0', 'IMAGE_DIAG summary 1 1 8192 1 2 1 2 2 0'):
            lines = valid_lines(); lines[0] = row
            self.assertEqual(parse(lines)['status'], 'inconclusive')

    def test_first_repeat_and_summary_invariants(self):
        for row in ('IMAGE_DIAG metric 1 1 2 2 0 0 20 20', 'IMAGE_DIAG metric 1 1 1 0 0 0 20 20',
                    'IMAGE_DIAG metric 1 1 1 1 0 2 20 20', 'IMAGE_DIAG metric 1 1 1 1 0 0 20 21'):
            lines = valid_lines(); lines[6] = row
            self.assertEqual(parse(lines)['status'], 'inconclusive')
        lines = valid_lines(); lines[0] = 'IMAGE_DIAG summary 1 1 0 1 2 1 3 3 0'
        self.assertEqual(parse(lines)['status'], 'inconclusive')

    def test_only_size_fields_allow_minus_one(self):
        for field in (0, 1, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13):
            tokens = meta(1, 0).split(); tokens[field+2] = '-1'
            lines = valid_lines(); lines[3] = ' '.join(tokens)
            self.assertEqual(parse(lines)['status'], 'inconclusive')
        for field in (4, 5):
            tokens = meta(1, 0).split(); tokens[field+2] = str(m.INT_MAX)
            lines = valid_lines(); lines[3] = ' '.join(tokens); lines[2] = 'IMAGE_DIAG image 1 1 1 0 1'
            self.assertEqual(parse(lines)['status'], 'captured')

    def test_single_duration_and_metadata_consistency(self):
        lines = valid_lines(); lines[6] = 'IMAGE_DIAG metric 1 1 1 1 0 0 20 10'
        self.assertEqual(parse(lines)['status'], 'inconclusive')
        lines = valid_lines(); lines[5] = meta(1, 2, host=18)
        self.assertEqual(parse(lines)['status'], 'inconclusive')
        lines = valid_lines(); lines[6] = 'IMAGE_DIAG metric 1 1 1 1 0 1 20 20'; lines[7] = 'IMAGE_DIAG metric 1 2 1 1 0 1 35 35'
        self.assertEqual(parse(lines)['status'], 'inconclusive')

    def test_valid_metadata_has_known_ancestry_and_storage_semantics(self):
        for index in (14, 15):
            fields = meta(1, 0).split(); fields[index] = '0'
            lines = valid_lines(); lines[3] = ' '.join(fields)
            self.assertEqual(parse(lines)['status'], 'inconclusive')
        for storage in (0, 1, 4, 7):
            fields = meta(1, 0).split(); fields[5] = str(storage)
            lines = valid_lines(); lines[3] = ' '.join(fields)
            self.assertEqual(parse(lines)['status'], 'inconclusive')

    def test_every_target_transport_split(self):
        stream = '\n'.join(valid_lines()) + '\n'
        for i in range(len(stream)+1):
            self.assertEqual(parse(['@'+json.dumps(stream[:i]), '@'+json.dumps(stream[i:])])['status'], 'captured')

    def test_private_or_malformed_payloads_not_accepted(self):
        for bad in ('@null', '@42', '@broken', '@'+json.dumps('x'*4097), 'IMAGE_DIAG complete /private',
                    'IMAGE_DIAG summary 1 1 0 1 2 1 0x12 2 0', 'IMAGE_DIAG summary 1 1 0 1 2 1 -2 2 0'):
            self.assertEqual(parse([bad]+valid_lines())['status'], 'inconclusive')
        self.assertEqual(parse(['~'+json.dumps('\n'.join(valid_lines()))])['status'], 'inconclusive')
        lines = valid_lines()
        self.assertEqual(parse([lines[0], '@'+json.dumps('\n'.join(lines[1:])+'\n')])['status'], 'inconclusive')

    def test_maximum_declared_records_fit_existing_report_limit(self):
        lines = valid_lines(m.MAX_IMAGES, m.MAX_WINDOWS)
        for i, row in enumerate(lines):
            if row.startswith('IMAGE_DIAG meta ') and row.split()[4] == '1':
                fields = row.split(); fields[6] = fields[7] = str(m.INT_MAX)
                fields[12] = str(m.UINT_MAX); fields[13] = str(m.INT_MAX)
                lines[i] = ' '.join(fields)
            elif row.startswith('IMAGE_DIAG metric ') and row.split()[2] != '0':
                fields = row.split(); fields[-2:] = [str(m.INT64_MAX)] * 2
                lines[i] = ' '.join(fields)
        result = parse(lines)
        self.assertEqual(result['status'], 'captured')
        self.assertEqual(len(result['records']), m.MAX_RECORDS)
        # Leave at least half the unchanged1MiB report limit for existing bounded
        # stacks and owner/phase/leaf records; do not enlarge that limit.
        self.assertLess(len(json.dumps(result, indent=2).encode()), 512*1024)

if __name__ == '__main__': unittest.main()
