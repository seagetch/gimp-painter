"""Fail-closed bounded numeric ICON_DIAG parser.

Names and addresses are never serialized. Name ordinals establish exact requested
byte equality only within one run, not resolved assets or cache/cold behavior.
"""
import json
import re

MAX_TARGET_LINE = 4096
MAX_MI_CHUNK = 65536
MAX_NAMES = 256
MAX_GROUPS = 128
MAX_RECORDS = 1 + MAX_GROUPS + 1 + 1
MAX_COUNT = 1000000000
INT_MAX = (1 << 31) - 1
UINT_MAX = (1 << 32) - 1
INT64_MAX = (1 << 63) - 1
UINT64_MAX = (1 << 64) - 1
FIELDS = {'summary': 9, 'group': 20, 'overflow': 6, 'complete': 0}
PREFIX = 'ICON_DIAG'


class IconRecords:
    def __init__(self):
        self.rows = []
        self.complete = False
        self.invalid = False
        self.pending = ''
        self.transport = None
        self._summary = None
        self._expected = []

    def consume(self, line):
        if self.invalid:
            return
        if not isinstance(line, str):
            self.invalid = True
            return
        if line.startswith('@'):
            if len(line) > MAX_MI_CHUNK:
                self.invalid = True
                self.pending = ''
                return
            try:
                chunk = json.loads(line[1:])
            except (ValueError, TypeError):
                self.invalid = True
                return
            if not isinstance(chunk, str):
                self.invalid = True
                return
            # Split only on LF: other Unicode line separators are not records.
            start = 0
            while start < len(chunk):
                end = chunk.find('\n', start)
                stop = len(chunk) if end < 0 else end + 1
                if len(self.pending) + stop - start > MAX_TARGET_LINE:
                    self.invalid = True
                    self.pending = ''
                    return
                self.pending += chunk[start:stop]
                if end >= 0:
                    row = self.pending[:-1].removesuffix('\r')
                    self.pending = ''
                    self._consume_row(row, 'target')
                    if self.invalid:
                        return
                start = stop
            return
        if line.startswith(PREFIX):
            if self.pending or len(line) > MAX_TARGET_LINE:
                self.invalid = True
                self.pending = ''
                return
            self._consume_row(line, 'raw')

    def _consume_row(self, text, transport):
        if not text.startswith(PREFIX):
            return
        if self.transport is not None and self.transport != transport:
            self.invalid = True
            return
        self.transport = transport
        tokens = text.split(' ')
        kind = tokens[1] if len(tokens) > 1 else ''
        if (self.complete or len(self.rows) >= MAX_RECORDS or
                tokens[0] != PREFIX or kind not in FIELDS or
                len(tokens) != 2 + FIELDS[kind] or
                any(not re.fullmatch(r'(?:[0-9]{1,20}|-1)', x) for x in tokens[2:])):
            self.invalid = True
            return
        values = [int(x) for x in tokens[2:]]
        if any(x > UINT64_MAX for x in values) or not self._valid_row(kind, values):
            self.invalid = True
            return
        self.rows.append({'kind': kind, 'values': values})
        if kind == 'complete':
            self.complete = True

    @staticmethod
    def _valid_metric(values):
        entries, exits, depth, nested, total, longest = values
        return (all(0 <= x <= MAX_COUNT for x in (entries, exits, depth, nested)) and
                entries == exits + depth and nested <= entries and
                0 <= longest <= total <= INT64_MAX and
                total <= longest * exits and (exits != 1 or total == longest))

    def _valid_row(self, kind, values):
        if kind == 'summary':
            if self.rows:
                return False
            version, enabled, flags, names, groups, entries, exits, depth, dropped = values
            if not (version == 1 and enabled in (0, 1) and 0 <= flags <= 255 and
                    0 <= names <= MAX_NAMES and 0 <= groups <= MAX_GROUPS and
                    all(0 <= x <= MAX_COUNT for x in (entries, exits, depth, dropped)) and
                    entries == exits + depth):
                return False
            self._summary = values
            self._expected = [('group', i) for i in range(1, groups + 1)]
            self._expected += [('overflow',), ('complete',)]
            return True
        if self._summary is None or len(self.rows) - 1 >= len(self._expected):
            return False
        expected = self._expected[len(self.rows) - 1]
        if kind != expected[0] or tuple(values[:len(expected)-1]) != expected[1:]:
            return False
        if kind == 'group':
            (ident, status, name, valid, storage, logical, pixel, scale, state,
             direction, fallback, ancestor, host, tracked) = values[:14]
            if not (1 <= status <= 6 and tracked in (0, 1) and
                    self._valid_metric(values[14:]) and values[14] > 0):
                return False
            if status == 6:
                return name == 0 and values[3:13] == [0, 0, -1, -1, 0, 0, 0, 0, 0, 0]
            return (valid == 1 and 0 <= storage <= 7 and
                    -1 <= logical <= INT_MAX and -1 <= pixel <= INT_MAX and
                    1 <= scale <= INT_MAX and 0 <= state <= UINT_MAX and
                    0 <= direction <= 2 and fallback in (0, 1) and
                    1 <= ancestor <= 6 and 1 <= host <= 18 and
                    (storage in (2, 3, 5, 6) or logical == -1) and
                    ((status == 1 and 1 <= name <= self._summary[3]) or
                     (status != 1 and name == 0)) and
                    ((status == 3 and storage != 5) or (status != 3 and storage == 5)))
        if kind == 'overflow':
            return self._valid_metric(values)
        return kind == 'complete'

    def result(self):
        valid = (self.complete and not self.invalid and not self.pending and
                 self._summary is not None and len(self.rows) == 1 + len(self._expected))
        groups = []
        overflow = [0] * 6
        if valid:
            version, enabled, flags, names, group_count, entries, exits, depth, dropped = self._summary
            groups = [r['values'] for r in self.rows if r['kind'] == 'group']
            overflow = self.rows[-2]['values']
            metrics = [g[14:] for g in groups] + [overflow]
            valid = (all(sum(m[i] for m in metrics) == total
                         for i, total in enumerate((entries, exits, depth))) and
                     enabled == (not bool(flags & 1)) and
                     (enabled or (entries == names == group_count == dropped == 0)) and
                     (not depth or bool(flags & 64)) and
                     (not dropped or (bool(flags & 16) and entries == MAX_COUNT)) and
                     (not overflow[0] or (bool(flags & 8) and group_count == MAX_GROUPS)))
            # Numeric configurations form unique aggregation keys. Unknown name
            # statuses aggregate calls, but MUST NOT imply requested-name equality.
            keys = [tuple(g[1:14]) for g in groups]
            valid = valid and len(set(keys)) == len(keys)
            seen_names = set()
            for group in groups:
                if group[1] == 1 and group[2] not in seen_names:
                    valid = valid and group[2] == len(seen_names) + 1
                    seen_names.add(group[2])
                if group[1] == 5:
                    valid = valid and names == MAX_NAMES
                status = group[1]
                required = {4: 2, 5: 4, 6: 128}.get(status, 0)
                valid = valid and (not required or bool(flags & required))
            valid = valid and (bool(overflow[0]) or len(seen_names) == names)
            if not flags and entries:
                valid = valid and sum(m[3] for m in metrics) < entries
        base = {
            'diagnostic_only': True,
            'acceptance_eligible': False,
            'name_equality_scope': 'Run-local exact requested-name byte equality only for status 1; unknown-name aggregates never prove equality',
            'coverage_scope': 'Observed request configuration only; tracked means existing weak snapshot membership only; parent image_observation governs ancestry scope and missing semantic types may broaden ancestor buckets; styles and theme are not captured; not resolved-asset identity, cache-hit evidence, or cold-call proof; inclusive durations may overlap',
            'records': self.rows,
        }
        if not valid:
            return dict(base, status='inconclusive',
                        name_equality_coverage_complete=False,
                        failure_category='Incomplete or invalid bounded icon records')
        safe = bool(enabled and flags == 0)
        return dict(base, status='captured' if safe else 'inconclusive',
                    name_equality_coverage_complete=bool(safe and all(g[1] == 1 for g in groups)),
                    unknown_name_calls=sum(g[14] for g in groups if g[1] != 1),
                    overflow_calls=overflow[0], dropped_calls=dropped)
