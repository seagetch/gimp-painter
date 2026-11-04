"""Strict numeric icon request records, no GUI/debugger or target process."""
import importlib.util
import json
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('icon_records', Path(__file__).with_name('icon_records.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


def row(kind, values):
    return 'ICON_DIAG ' + kind + ((' ' + ' '.join(map(str, values))) if values else '')


def group(ident=1, name=1, status=1, tracked=1):
    return [ident, status, name, 1, 5, 1, -1, 1, 128, 1, 0, 3, 1, tracked,
            2, 2, 0, 0, 40, 25]


def lines(groups=None, flags=0, names=1, enabled=1, overflow=None, dropped=0):
    if groups is None:
        groups = [group()]
    overflow = [0] * 6 if overflow is None else overflow
    metrics = [g[14:] for g in groups] + [overflow]
    totals = [sum(v[i] for v in metrics) for i in range(3)]
    summary = [1, enabled, flags, names, len(groups)] + totals + [dropped]
    return [row('summary', summary)] + [row('group', g) for g in groups] + [row('overflow', overflow), row('complete', [])]


def parse(records):
    parser = m.IconRecords()
    for record in records:
        parser.consume(record)
    return parser.result()


class IconRecordsTests(unittest.TestCase):
    def assert_bad(self, records):
        result = parse(records)
        self.assertEqual(result['status'], 'inconclusive')
        self.assertIn('failure_category', result)
        self.assertTrue(result['diagnostic_only'])
        self.assertFalse(result['acceptance_eligible'])
        self.assertFalse(result['name_equality_coverage_complete'])

    def test_exact_run_local_names_only(self):
        result = parse(lines())
        self.assertEqual(result['status'], 'captured')
        self.assertTrue(result['name_equality_coverage_complete'])
        self.assertFalse(result['acceptance_eligible'])
        self.assertTrue(result['diagnostic_only'])
        for disclaimer in ('styles', 'theme', 'resolved-asset', 'cache-hit', 'cold-call'):
            self.assertIn(disclaimer, result['coverage_scope'])
        self.assertIn('Run-local', result['name_equality_scope'])

    def test_empty_enabled_and_disabled(self):
        self.assertEqual(parse(lines([], names=0))['status'], 'captured')
        result = parse(lines([], flags=1, names=0, enabled=0))
        self.assertEqual(result['status'], 'inconclusive')
        self.assertNotIn('failure_category', result)
        self.assert_bad(lines(enabled=0))
        self.assert_bad(lines(flags=1))

    def test_null_and_nonicon_aggregate_never_prove_equality(self):
        for status in (2, 3):
            g = group(status=status, name=0)
            if status == 3:
                g[4:7] = [1, -1, -1]
            result = parse(lines([g], names=0))
            self.assertEqual(result['status'], 'captured')
            self.assertFalse(result['name_equality_coverage_complete'])
            self.assertEqual(result['unknown_name_calls'], 2)

    def test_all_fault_flags_are_inconclusive(self):
        for flag in (2, 4, 8, 16, 32, 64, 128, 254):
            result = parse(lines(flags=flag))
            self.assertEqual(result['status'], 'inconclusive')
            self.assertNotIn('failure_category', result)
            self.assertFalse(result['name_equality_coverage_complete'])
        self.assert_bad(lines(flags=256))

    def test_unknown_status_flags_and_canonical_invalid_metadata(self):
        for status, flag in ((4, 2), (6, 128)):
            g = group(status=status, name=0)
            if status == 6:
                g[3:13] = [0, 0, -1, -1, 0, 0, 0, 0, 0, 0]
            self.assert_bad(lines([g], names=0))
            result = parse(lines([g], flags=flag, names=0))
            self.assertNotIn('failure_category', result)
            self.assertFalse(result['name_equality_coverage_complete'])
        g[4] = 5
        self.assert_bad(lines([g], flags=128, names=0))

    def test_order_shape_missing_duplicate_extra_and_raw_names(self):
        original = lines()
        for i in range(len(original)):
            self.assert_bad(original[:i] + original[i+1:])
            self.assert_bad(original[:i] + [original[i]] + original[i:])
        for bad in ('ICON_DIAG', 'ICON_DIAG complete private-name',
                    'ICON_DIAG unknown', 'ICON_DIAG group ' + ' '.join(['1'] * 21),
                    'ICON_DIAG summary 1 1 0 1 1 2 2 0 0 raw-icon',
                    'ICON_DIAG complete\n', 'ICON_DIAG  complete', 'ICON_DIAGX complete'):
            self.assert_bad([bad] + original)
            self.assertNotIn('raw-icon', json.dumps(parse([bad] + original)))
        swapped = original[:]; swapped[1:3] = reversed(swapped[1:3]); self.assert_bad(swapped)

    def test_count_and_numeric_ranges(self):
        for index, bad in ((0, 2), (1, 2), (2, 256), (3, 257), (4, m.MAX_GROUPS + 1),
                           (5, m.MAX_COUNT + 1), (6, 1), (7, 1), (8, m.MAX_COUNT + 1)):
            records = lines(); values = list(map(int, records[0].split()[2:])); values[index] = bad
            records[0] = row('summary', values); self.assert_bad(records)
        for index, bad in ((0, 2), (1, 0), (1, 7), (2, 0), (2, 2), (3, 0), (4, 8),
                           (5, -2), (5, m.INT_MAX + 1), (6, m.INT_MAX + 1),
                           (7, 0), (7, m.INT_MAX + 1), (8, m.UINT_MAX + 1),
                           (9, 3), (10, 2), (11, 0), (11, 7), (12, 0), (12, 19), (13, 2)):
            g = group(); g[index] = bad; self.assert_bad(lines([g]))
        for index in range(20):
            g = group(); g[index] = -1
            if index in (5, 6):
                self.assertEqual(parse(lines([g]))['status'], 'captured')
            else:
                self.assert_bad(lines([g]))

    def test_id_and_storage_consistency(self):
        for status in range(1, 7):
            g = group(status=status)
            if status != 1:
                self.assert_bad(lines([g], flags=254))
        for status in (1, 2, 4, 5):
            g = group(status=status, name=1 if status == 1 else 0); g[4] = 6
            self.assert_bad(lines([g], flags=254))
        g = group(status=3, name=0)
        self.assert_bad(lines([g]))
        for storage in (0, 1, 4, 7):
            g = group(status=3, name=0); g[4] = storage
            self.assert_bad(lines([g]))

    def test_full_config_and_tracked_are_group_keys(self):
        for index in range(2, 14):
            if index in (3, 4):
                continue
            g1, g2 = group(), group(ident=2)
            g2[index] = {2: 2, 5: 2, 6: 32, 7: 2, 8: 129, 9: 2,
                         10: 1, 11: 4, 12: 2, 13: 0}[index]
            self.assertEqual(parse(lines([g1, g2], names=2 if index == 2 else 1))['status'], 'captured')
        self.assert_bad(lines([group(), group(ident=2)]))

    def test_metrics_summary_nesting_and_duration_consistency(self):
        for values in ([2, 1, 0, 0, 40, 25], [2, 2, 0, 3, 40, 25],
                       [2, 2, 0, 0, 40, 41], [2, 2, 0, 0, 51, 25],
                       [1, 1, 0, 0, 40, 25], [1, 0, 1, 0, 1, 1],
                       [0, 0, 0, 0, 0, 0], [2, 2, 0, 0, m.INT64_MAX+1, m.INT64_MAX+1]):
            g = group(); g[14:] = values; self.assert_bad(lines([g], flags=64))
        g = group(); g[17] = 2; self.assert_bad(lines([g]))
        g = group(); g[14:] = [2, 1, 1, 1, 10, 10]
        self.assert_bad(lines([g]))
        self.assertNotIn('failure_category', parse(lines([g], flags=64)))
        records = lines(); records[0] = 'ICON_DIAG summary 1 1 0 1 1 3 3 0 0'; self.assert_bad(records)

    def test_overflow_and_dropped_are_distinct(self):
        overflow = [3, 3, 0, 1, 70, 30]
        self.assert_bad(lines(overflow=overflow))
        self.assert_bad(lines(overflow=overflow, flags=8))
        groups = [group(ident=i, name=1) for i in range(1, m.MAX_GROUPS + 1)]
        for i, g in enumerate(groups):
            g[8] = i
        result = parse(lines(groups, overflow=overflow, flags=8))
        self.assertNotIn('failure_category', result)
        self.assertEqual(result['overflow_calls'], 3)
        self.assertFalse(result['name_equality_coverage_complete'])
        self.assert_bad(lines(dropped=1))
        self.assert_bad(lines(dropped=1, flags=16))
        g = group(); g[14:16] = [m.MAX_COUNT] * 2
        result = parse(lines([g], dropped=1, flags=16))
        self.assertNotIn('failure_category', result)
        self.assertEqual(result['dropped_calls'], 1)
        # Interned names can first occur in overflow and have no group.
        self.assertNotIn('failure_category', parse(lines(groups, names=256, flags=8, overflow=[255, 255, 0, 1, 70, 30])))

    def test_name_ordinals_must_match_first_observations(self):
        self.assert_bad(lines(names=2))
        self.assert_bad(lines([], names=1))
        self.assert_bad(lines([group(name=2)], names=2))
        self.assert_bad(lines([group(name=2), group(ident=2, name=1)], names=2))
        self.assert_bad(lines([group(status=5, name=0)], names=0, flags=4))
        self.assertEqual(parse(lines([group(), group(ident=2, name=2)], names=2))['status'], 'captured')

    def test_every_target_transport_split(self):
        stream = '\n'.join(lines()) + '\n'
        for i in range(len(stream)+1):
            self.assertEqual(parse(['@'+json.dumps(stream[:i]), '@'+json.dumps(stream[i:])])['status'], 'captured')
        stream = '\r\n'.join(lines()) + '\r\n'
        self.assertEqual(parse(['@'+json.dumps(stream)])['status'], 'captured')

    def test_transport_is_bounded_and_fail_closed(self):
        for bad in (None, 42, '@null', '@42', '@broken', '@'+json.dumps('x'*4097),
                    '@'+json.dumps('x'*m.MAX_MI_CHUNK),
                    'ICON_DIAG ' + 'x'*m.MAX_TARGET_LINE):
            self.assert_bad([bad] + lines())
        self.assert_bad(['~'+json.dumps('\n'.join(lines()))])
        self.assert_bad([lines()[0], '@'+json.dumps('\n'.join(lines()[1:])+'\n')])
        self.assert_bad(['@'+json.dumps('\n'.join(lines()))])
        self.assert_bad(['@'+json.dumps('\u2028'.join(lines())+'\n')])
        self.assert_bad(['@'+json.dumps('unfinished'), *lines()])
        for number in ('+1', '01x', '0x1', '1.0', '１', '9'*21, str(m.UINT64_MAX+1)):
            records = lines(); records[0] = records[0].replace('summary 1', 'summary '+number)
            self.assert_bad(records)

    def test_maximum_payload_within_existing_report_limit(self):
        groups = []
        for i in range(1, m.MAX_GROUPS + 1):
            g = group(ident=i, name=(i - 1) % m.MAX_NAMES + 1, tracked=int(i <= m.MAX_NAMES))
            g[5:9] = [m.INT_MAX, m.INT_MAX, m.INT_MAX, m.UINT_MAX]
            count = m.MAX_COUNT // m.MAX_GROUPS
            g[14:18] = [count, count, 0, count - 1]
            g[18:] = [m.INT64_MAX, m.INT64_MAX]
            groups.append(g)
        result = parse(lines(groups, names=min(m.MAX_GROUPS, m.MAX_NAMES)))
        self.assertEqual(result['status'], 'captured')
        self.assertEqual(len(result['records']), m.MAX_RECORDS)
        self.assertLess(len(json.dumps(result, indent=2).encode()), 512 * 1024)


if __name__ == '__main__':
    unittest.main()
