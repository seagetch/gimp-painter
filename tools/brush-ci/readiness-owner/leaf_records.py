"""Fail-closed, bounded LEAF_DIAG transport and numeric record parser.

No debugger console, raw strings, pointers, or arbitrary diagnostic text are
included in results. Unknown overrides alone permit explicitly partial capture;
unsafe or incomplete restoration is always inconclusive.
"""
import json
import re

MAX_TARGET_LINE = 4096
MAX_MI_CHUNK = 65536
MAX_RECORDS = 21
MAX_COUNT = 1000000000
MAX_TYPES = 128
INT64_MAX = (1 << 63) - 1
UINT64_MAX = (1 << 64) - 1
FIELDS = {'summary': 5, 'family': 11, 'metric': 9, 'union': 7, 'complete': 0}
PREFIX = 'LEAF_DIAG'


class LeafRecords:
    def __init__(self):
        self.rows = []
        self.complete = False
        self.invalid = False
        self.pending = ''
        self.transport = None
        self._summary = None

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
                any(not re.fullmatch(r'[0-9]{1,20}', x) for x in tokens[2:])):
            self.invalid = True
            return
        values = [int(x) for x in tokens[2:]]
        if any(x > UINT64_MAX for x in values) or not self._valid_row(kind, values):
            self.invalid = True
            return
        self.rows.append({'kind': kind, 'values': values})
        if kind == 'complete':
            self.complete = True

    def _valid_row(self, kind, values):
        n = len(self.rows)
        if kind == 'summary':
            if n != 0:
                return False
            version, enabled, coverage, restored, flags = values
            if version != 1 or any(x not in (0, 1) for x in (enabled, coverage, restored)) or flags > 1023:
                return False
            self._summary = values
            return True
        if self._summary is None:
            return False
        if kind == 'family':
            if n not in (1, 2, 3):
                return False
            ident, available, begin, end, installed, restored, late, unknown, ub, ue, dynamic = values
            return (ident == n and available in (0, 1) and
                    all(x <= MAX_TYPES for x in (begin, end, installed, late, ub, ue)) and
                    restored <= 2 * MAX_TYPES and unknown <= 2 * MAX_TYPES and dynamic <= 2 * MAX_TYPES and
                    installed <= begin and late <= end and ub <= begin and ue <= end)
        if kind == 'metric':
            if not 4 <= n <= 18:
                return False
            family, owner, *metric = values
            return family == 1 + (n - 4) // 5 and owner == (n - 4) % 5 and self._valid_metric(metric)
        if kind == 'union':
            return n == 19 and self._valid_metric(values)
        return kind == 'complete' and n == 20

    @staticmethod
    def _valid_metric(values):
        entries, exits, depth, nested, outer, total, longest = values
        return (all(x <= MAX_COUNT for x in values[:5]) and
                total <= INT64_MAX and longest <= total and exits <= entries and
                depth == entries - exits and nested + outer == entries and
                (entries != 0 or total == 0))

    def result(self):
        valid = self.complete and not self.invalid and not self.pending and len(self.rows) == MAX_RECORDS
        if valid:
            version, enabled, coverage, restored, flags = self._summary
            families = [r['values'] for r in self.rows[1:4]]
            metrics = [r['values'][2:] for r in self.rows[4:19]]
            union = self.rows[19]['values']
            valid = (sum(v[0] for v in metrics) == union[0] and
                     sum(v[1] for v in metrics) == union[1] and
                     union[5] <= sum(v[5] for v in metrics))
            valid = valid and coverage == bool(enabled and flags == 0)
            if enabled:
                valid = valid and all(f[1] == 1 and f[4] >= 1 for f in families)
            else:
                valid = valid and bool(flags) and all(f[4] == 0 and f[5] == 0 and f[6] == 0 for f in families)
            if any(not f[1] for f in families):
                valid = valid and bool(flags & 1)
            if any(f[10] for f in families):
                valid = valid and bool(flags & 8)
            if restored and enabled:
                valid = valid and all(f[5] == f[4] + f[6] for f in families) and not (flags & 16)
            if any(f[7] for f in families):
                valid = valid and bool(flags & 4) and not coverage
            if not enabled:
                valid = valid and union[0] == 0
            if any(v[2] for v in metrics) or union[2]:
                valid = valid and bool(flags & 256)
            if coverage:
                valid = valid and not (flags & 15)
        if not valid:
            return {'status': 'inconclusive', 'failure_category': 'Incomplete or invalid bounded leaf records',
                    'records': self.rows}
        safe = bool(enabled and restored and not (flags & ~4))
        return {'status': 'captured' if safe else 'inconclusive',
                'coverage_complete': bool(coverage), 'restoration_complete': bool(restored),
                'diagnostic_only': True, 'acceptance_eligible': False,
                'coverage_scope': 'Pointer-equal static leaf width/get_size slots only; excludes height, pre-vfunc style/resize and direct static render calls',
                'records': self.rows}
