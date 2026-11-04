import importlib.util
import json
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    'phase_records', Path(__file__).with_name('phase_records.py'))
parser = importlib.util.module_from_spec(spec)
spec.loader.exec_module(parser)


def rows(owners=1, extra=(), clipped=0, missing=0, window=None):
    events = [[role, 16, 0, role * 10] for role in range(1, 13)]
    events[1:1] = list(extra)
    result = [f'summary {owners} {len(events)} {clipped} {missing}']
    for idx in range(owners):
        result.append('window ' + ' '.join(map(str, [idx] + (
            [1, 0, 0, 1, 65535, 0] if window is None else window))))
    result += ['event ' + ' '.join(map(str, event)) for event in events]
    return ['PHASE_DIAG ' + row for row in result + ['complete']]


def parse(lines):
    record = parser.PhaseRecords()
    for line in lines:
        record.consume(line)
    return record


class PhaseRecordTests(unittest.TestCase):
    def test_full_capture_and_all_roles(self):
        extra = [[role, 16 if role == 25 else 0,
                  1 if role == 20 else 65535 if role == 21 else 0, 10]
                 for role in range(20, 26)]
        self.assertEqual(parse(rows(extra=extra)).result()['status'], 'captured')

    def test_destroy_requires_dead_owner_and_is_inconclusive(self):
        inconsistent = parse(rows(extra=[[26, 0, 0, 10]]))
        self.assertTrue(inconsistent.invalid)
        self.assertEqual(inconsistent.result()['status'], 'inconclusive')
        destroyed = parse(rows(extra=[[26, 0, 0, 10], [20, 0, 1, 10]],
                               window=[0, 0, 0, 1, 0, 0]))
        self.assertFalse(destroyed.invalid)
        self.assertTrue(destroyed.complete)
        self.assertEqual(destroyed.result()['status'], 'inconclusive')
        self.assertIn('Observed owner destroyed', destroyed.result()['limitations'])

    def test_every_split_and_character_chunks(self):
        stream = '\n'.join(rows()) + '\n'
        expected = parse(rows()).result()
        for split in range(len(stream) + 1):
            self.assertEqual(parse(['@' + json.dumps(chunk) for chunk in
                                    (stream[:split], stream[split:])]).result(), expected)
        self.assertEqual(parse(['@' + json.dumps(c) for c in stream]).result(), expected)
        self.assertEqual(parse(['@' + json.dumps(stream.replace('\n', '\r\n'))]).result(), expected)

    def test_every_truncation_fails(self):
        stream = '\n'.join(rows()) + '\n'
        for end in range(len(stream)):
            self.assertEqual(parse(['@' + json.dumps(stream[:end])]).result()['status'], 'inconclusive')

    def test_malformed_numeric_and_shape(self):
        bad = ('summary -1 12 0 0', 'summary 1 12 0 0 extra',
               'summary 17 12 0 0', 'summary 1 257 0 0',
               'summary 1 12 2 0', 'summary 1 12 0 257',
               'summary 1 12 0 /private/secret', 'summary 1 12 0 1.0',
               'summary 1 12 0 +0', 'summary 1 12 0 ０',
               'summary 1 12 0 18446744073709551616',
               'unknown 1', 'complete extra', 'summary  1 12 0 0')
        for row in bad:
            with self.subTest(row=row):
                record = parse(['PHASE_DIAG ' + row] + rows())
                self.assertTrue(record.invalid)
                self.assertEqual(record.result()['status'], 'inconclusive')
                self.assertNotIn('/private/', json.dumps(record.result()))

    def test_wrong_windows(self):
        for field, value in ((0, 1), (1, 2), (2, 2), (3, 65536),
                             (4, 2), (5, 65536), (6, 2)):
            stream = rows()
            window = [0, 1, 0, 0, 1, 65535, 0]
            window[field] = value
            stream[1] = 'PHASE_DIAG window ' + ' '.join(map(str, window))
            self.assertTrue(parse(stream).invalid)

    def test_bad_event_roles_owners_values(self):
        for event in ([0, 16, 0, 10], [13, 16, 0, 10], [27, 0, 0, 10],
                      [20, 1, 0, 10], [20, 0, 2, 10], [21, 0, 65536, 10],
                      [22, 0, 1, 10], [25, 0, 0, 10], [25, 16, 1, 10],
                      [1, 0, 0, 10], [1, 16, 1, 10]):
            self.assertTrue(parse(rows(extra=[event])).invalid)

    def test_explicit_partial_limitations(self):
        for kwargs, limitation in (({'owners': 0}, 'No observed owners'),
                ({'clipped': 1}, 'Event attribution clipped'),
                ({'missing': 1}, 'Missing observation hooks'),
                ({'window': [0, 0, 0, 1, 0, 0]}, 'Observed owner destroyed'),
                ({'window': [1, 0, 0, 1, 0, 1]}, 'Observed frame clock changed')):
            record = parse(rows(**kwargs))
            self.assertFalse(record.invalid)
            self.assertTrue(record.complete)
            self.assertEqual(record.result()['status'], 'inconclusive')
            self.assertIn(limitation, record.result()['limitations'])

    def test_missing_duplicate_and_unordered_markers(self):
        stream = rows()
        for replacement in ('event 20 0 0 60', 'event 5 16 0 60', 'event 7 16 0 60'):
            changed = list(stream)
            changed[7] = 'PHASE_DIAG ' + replacement
            self.assertEqual(parse(changed).result()['status'], 'inconclusive')

    def test_monotonic_and_uint64(self):
        stream = rows()
        stream[-2] = f'PHASE_DIAG event 12 16 0 {parser.UINT64_MAX}'
        self.assertEqual(parse(stream).result()['status'], 'captured')
        stream[-2] = 'PHASE_DIAG event 12 16 0 0'
        self.assertIn('Nonmonotonic event timestamps', parse(stream).result()['limitations'])

    def test_exact_order_counts_duplicates_and_after_complete(self):
        stream = rows()
        variants = ([stream[0]] + stream, stream + [stream[-1]],
                    stream[:2] + [stream[1]] + stream[2:],
                    stream[:1] + stream[2:], stream[1:],
                    stream[:-2] + stream[-1:],
                    [stream[0], stream[2], stream[1]] + stream[3:])
        for changed in variants:
            self.assertEqual(parse(changed).result()['status'], 'inconclusive')
        self.assertEqual(parse(['PHASE_DIAG complete']).result()['status'], 'inconclusive')

    def test_transport_spoof_and_mixing(self):
        stream = '\n'.join(rows()) + '\n'
        record = parse(['~' + json.dumps(stream), '&' + json.dumps(stream), 'secret title'])
        self.assertEqual(record.rows, [])
        for lines in ((rows()[0], '@' + json.dumps('\n'.join(rows()[1:]) + '\n')),
                      ('@' + json.dumps(stream), 'PHASE_DIAG complete'),
                      ('@' + json.dumps('PHASE_DIAG '), 'PHASE_DIAG complete')):
            self.assertTrue(parse(lines).invalid)

    def test_bounded_malformed_transport(self):
        for line in ('@no-json', '@42', '@null', '@[]', None,
                     '@' + json.dumps('x' * (parser.MAX_TARGET_LINE + 1)),
                     '@' + ' ' * parser.MAX_MI_CHUNK,
                     'PHASE_DIAG ' + '9' * parser.MAX_TARGET_LINE):
            record = parse([line] + rows())
            self.assertTrue(record.invalid)
            self.assertLessEqual(len(record.pending), parser.MAX_TARGET_LINE)
            self.assertEqual(record.result()['status'], 'inconclusive')
        record = parse(['@' + json.dumps('x' * parser.MAX_TARGET_LINE), '@"x"'])
        self.assertTrue(record.invalid)
        self.assertEqual(record.pending, '')

    def test_maximum_events_retained_and_overflow_fails(self):
        stream = rows(owners=16, extra=[[22, 0, 0, 10]] * 244)
        record = parse(['@' + json.dumps('\n'.join(stream) + '\n')])
        self.assertEqual(record.result()['status'], 'captured')
        self.assertEqual(len(record.rows), parser.MAX_RECORDS)
        clipped = parse(rows(owners=16, clipped=1, extra=[[22, 0, 0, 10]] * 244))
        self.assertEqual(clipped.result()['status'], 'inconclusive')
        record.consume('PHASE_DIAG event 22 0 0 130')
        self.assertTrue(record.invalid)
        self.assertEqual(len(record.rows), parser.MAX_RECORDS)

    def test_snapshot_cannot_mutate_parser(self):
        record = parse(rows())
        result = record.result()
        result['records'][0]['values'][0] = 0
        result['records'].clear()
        self.assertEqual(record.result()['status'], 'captured')


if __name__ == '__main__':
    unittest.main()
