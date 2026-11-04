"""Fail-closed, bounded PHASE_DIAG transport and numeric record parser.

No debugger console, raw strings, pointers, or arbitrary diagnostic text are
included in results. A structurally valid partial observation is inconclusive.
"""
import json
import re

MAX_TARGET_LINE = 4096
MAX_MI_CHUNK = 65536
MAX_OWNERS = 16
MAX_EVENTS = 256
MAX_MISSING = 256
MAX_RECORDS = 2 + MAX_OWNERS + MAX_EVENTS
UINT64_MAX = (1 << 64) - 1
FIELDS = {'summary': 4, 'window': 7, 'event': 4, 'complete': 0}
PREFIX = 'PHASE_DIAG'


class PhaseRecords:
    def __init__(self):
        self.rows = []
        self.complete = False
        self.invalid = False
        self.pending = ''
        self.transport = None
        self._owners = None
        self._events = None
        self._windows_seen = 0
        self._events_seen = 0

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
        if kind == 'summary':
            if self.rows:
                return False
            owners, events, clipped, missing = values
            if not (owners <= MAX_OWNERS and events <= MAX_EVENTS and
                    clipped in (0, 1) and missing <= MAX_MISSING):
                return False
            self._owners, self._events = owners, events
            return True
        if self._owners is None:
            return False
        if kind == 'window':
            idx, alive, ia, initial_flags, fa, final_flags, changed = values
            if not (self._events_seen == 0 and idx == self._windows_seen and
                    idx < self._owners and alive in (0, 1) and
                    ia in (0, 1) and fa in (0, 1) and changed in (0, 1) and
                    initial_flags <= 65535 and final_flags <= 65535):
                return False
            self._windows_seen += 1
            return True
        if self._windows_seen != self._owners:
            return False
        if kind == 'event':
            role, owner, value, elapsed = values
            if self._events_seen >= self._events:
                return False
            if 1 <= role <= 12 or role == 25:
                valid = owner == MAX_OWNERS and value == 0
            elif 20 <= role <= 26:
                valid = owner < self._owners and (
                    value in (0, 1) if role == 20 else
                    value <= 65535 if role == 21 else value == 0)
            else:
                valid = False
            if not valid:
                return False
            if role == 26 and self.rows[1 + owner]['values'][1] != 0:
                return False
            self._events_seen += 1
            return True
        return kind == 'complete' and self._events_seen == self._events

    def result(self):
        reasons = []
        if self.invalid or not self.complete or self.pending:
            reasons.append('Incomplete or invalid bounded phase records')
        if self._owners is not None:
            owners, events, clipped, missing = self.rows[0]['values']
            if owners == 0:
                reasons.append('No observed owners')
            if clipped:
                reasons.append('Event attribution clipped')
            if missing:
                reasons.append('Missing observation hooks')
            windows = [r['values'] for r in self.rows if r['kind'] == 'window']
            es = [r['values'] for r in self.rows if r['kind'] == 'event']
            if any(not w[1] for w in windows):
                reasons.append('Observed owner destroyed')
            if any(w[6] for w in windows):
                reasons.append('Observed frame clock changed')
            if [e[0] for e in es if 1 <= e[0] <= 12] != list(range(1, 13)):
                reasons.append('Incomplete or unordered phase markers')
            if any(a[3] > b[3] for a, b in zip(es, es[1:])):
                reasons.append('Nonmonotonic event timestamps')
        elif not reasons:
            reasons.append('Incomplete or invalid bounded phase records')
        # Return a detached, finite numeric snapshot, never transport payloads.
        result = {'status': 'inconclusive' if reasons else 'captured',
                  'records': [{'kind': r['kind'], 'values': list(r['values'])}
                              for r in self.rows]}
        if reasons:
            result['failure_category'] = reasons[0]
            result['limitations'] = reasons
        return result
