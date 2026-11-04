"""Fail-closed, bounded IMAGE_DIAG transport and numeric record parser.

No debugger console, raw strings, pointers, or arbitrary diagnostic text are
included in results. Missing semantic ancestor types permit explicitly partial capture;
unsafe or incomplete image observation is always inconclusive.
"""
import json
import re

MAX_TARGET_LINE = 4096
MAX_MI_CHUNK = 65536
MAX_WINDOWS = 16
MAX_IMAGES = 256
MAX_NODES = 4096
MAX_RECORDS = 1 + MAX_WINDOWS + 6 * MAX_IMAGES + 3 + 1
MAX_COUNT = 1000000000
INT_MAX = (1 << 31) - 1
UINT_MAX = (1 << 32) - 1
INT64_MAX = (1 << 63) - 1
UINT64_MAX = (1 << 64) - 1
FIELDS = {'summary': 9, 'window': 2, 'image': 5, 'meta': 14, 'metric': 8, 'complete': 0}
PREFIX = 'IMAGE_DIAG'


class ImageRecords:
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

    def _valid_row(self, kind, values):
        if kind == 'summary':
            if self.rows:
                return False
            version, enabled, flags, windows, nodes, images, entries, exits, depth = values
            if not (version == 1 and enabled in (0, 1) and 0 <= flags <= 8191 and
                    0 <= windows <= MAX_WINDOWS and 0 <= nodes <= MAX_NODES and
                    0 <= images <= MAX_IMAGES and images <= nodes and
                    all(0 <= x <= MAX_COUNT for x in (entries, exits, depth)) and entries == exits + depth):
                return False
            self._summary = values
            self._expected = [('window', i) for i in range(1, windows + 1)]
            for i in range(1, images + 1):
                self._expected += [('image', i)] + [('meta', i, phase) for phase in range(3)] + [('metric', i, k) for k in (1, 2)]
            self._expected += [('meta', 0, 1), ('meta', 0, 2), ('metric', 0, 0), ('complete',)]
            return True
        if self._summary is None or len(self.rows) - 1 >= len(self._expected):
            return False
        expected = self._expected[len(self.rows) - 1]
        if kind != expected[0] or tuple(values[:len(expected)-1]) != expected[1:]:
            return False
        if kind == 'window':
            return values[1] in (0, 1)
        if kind == 'image':
            ident, window, alive, destroyed, changes = values
            return (1 <= window <= self._summary[3] and alive in (0, 1) and
                    destroyed in (0, 1) and 0 <= changes <= MAX_COUNT)
        if kind == 'meta':
            ident, phase, valid, storage, logical, pixel, visible, mapped, child, realized, state, scale, ancestor, host = values
            if valid == 0:
                return values[3:] == [0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0]
            return (valid == 1 and 0 <= storage <= 7 and -1 <= logical <= INT_MAX and
                    -1 <= pixel <= INT_MAX and all(x in (0, 1) for x in (visible, mapped, child, realized)) and
                    0 <= state <= UINT_MAX and 1 <= scale <= INT_MAX and 1 <= ancestor <= 6 and
                    1 <= host <= 18 and (host >= 17 or host <= self._summary[3]) and
                    (storage in (2, 3, 5, 6) or logical == -1))
        if kind == 'metric':
            ident, metric, entries, exits, depth, nested, total, longest = values
            return (all(0 <= x <= MAX_COUNT for x in (entries, exits, depth, nested)) and
                    entries == exits + depth and nested <= entries and
                    0 <= longest <= total <= INT64_MAX and (entries != 0 or total == 0) and
                    (metric != 1 or entries <= 1) and (entries != 1 or total == longest))
        return kind == 'complete'

    def result(self):
        valid = (self.complete and not self.invalid and not self.pending and
                 self._summary is not None and len(self.rows) == 1 + len(self._expected))
        if valid:
            version, enabled, flags, windows, nodes, images, entries, exits, depth = self._summary
            metrics = [r['values'] for r in self.rows if r['kind'] == 'metric']
            metas = {(r['values'][0], r['values'][1]): r['values'] for r in self.rows if r['kind'] == 'meta'}
            identity = [r['values'] for r in self.rows if r['kind'] == 'image']
            valid = (sum(m[2] for m in metrics) == entries and sum(m[3] for m in metrics) == exits and
                     sum(m[4] for m in metrics) == depth and (enabled or entries == 0))
            unknown = metrics[-1][2]
            for i, row in enumerate(identity, 1):
                first, repeat = metrics[2*(i-1):2*i]
                calls = first[2] + repeat[2]
                valid = valid and row[4] <= calls and (not repeat[2] or first[2] == 1)
                if not (flags & (32 | 512)):
                    valid = valid and metas[(i, 0)][2] == 1 and metas[(i, 1)][2] == bool(calls) and metas[(i, 2)][2] == bool(calls)
                    if calls:
                        snapshot = metas[(i, 0)][2:]
                        first_meta = metas[(i, 1)][2:]
                        last_meta = metas[(i, 2)][2:]
                        minimum_changes = int(snapshot != first_meta) + int(first_meta != last_meta)
                        valid = valid and row[4] >= minimum_changes
                        if calls == 1:
                            valid = valid and first_meta == last_meta and row[4] == int(snapshot != first_meta)
            if not (flags & (32 | 512)):
                valid = valid and metas[(0, 1)][2] == bool(unknown) and metas[(0, 2)][2] == bool(unknown)
                if unknown == 1:
                    valid = valid and metas[(0, 1)][2:] == metas[(0, 2)][2:]
            if not (flags & ~2048):
                valid = valid and all(metas[(row[0], 0)][-1] == row[1] for row in identity)
                if entries:
                    valid = valid and sum(row[5] for row in metrics) < entries
            valid = valid and enabled == (not bool(flags & 1))
            if depth:
                valid = valid and bool(flags & 256)
        if not valid:
            return {'status': 'inconclusive', 'failure_category': 'Incomplete or invalid bounded image records', 'records': self.rows}
        safe = bool(enabled and not (flags & ~2048))
        return {'status': 'captured' if safe else 'inconclusive',
                'identity_coverage_complete': bool(safe and not flags and unknown == 0),
                'untracked_calls': unknown, 'diagnostic_only': True, 'acceptance_eligible': False,
                'coverage_scope': 'Existing weak GtkImage snapshot only; first observed is not cold; untracked calls are not unique/new images; inclusive durations may overlap',
                'records': self.rows}
