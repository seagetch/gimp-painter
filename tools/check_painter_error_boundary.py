#!/usr/bin/env python3
"""Verify the original05.013 common contract without closing feature gates."""
import csv
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    d = ROOT / 'migration/tests/error-boundary'
    report = json.loads((d / 'report.json').read_text())
    assert report['task'] == '05.013' and report['status'] == 'DEFINED'
    codes = re.findall(r'^\s+GIMP_PAINTER_ERROR_(\w+),?$',
                       (ROOT / 'app/painter/gimp-painter-error.h').read_text(), re.M)
    assert codes == ['WRONG_TYPE', 'MISSING_SLOT', 'DUPLICATE_SLOT', 'INVALID_STATE',
                     'CLOSED', 'WRONG_THREAD', 'EXCEPTION']
    for path, digest in report['source_sha256'].items():
        assert sha(ROOT / path) == digest, path
    results = json.loads((d / 'native-results.json').read_text())
    assert [(r['variant'], r['group'], r['exit_code'], r['passed']) for r in results['results']] == [
        (variant, group, 0, count) for variant in ('native', 'sanitized')
        for group, count in [('interop', 5), ('boundary', 2), ('store', 5)]]
    assert results['LSan'] is False and results['vptr'] is False
    for path, digest in results['source_sha256'].items():
        assert sha(ROOT / path) == digest, path
    for path, digest in report['evidence_sha256'].items():
        assert sha(ROOT / path) == digest, path
    tasks = (ROOT / 'tasks.md').read_text()
    for task in report['remaining_implementation_tasks']:
        assert re.search(r'^\| ' + re.escape(task) + r' \| \[ \]', tasks, re.M), task
    assert '契約定義行を必須先行 ID に持つ独立した実装・検証行' in tasks
    with (ROOT / 'migration/inventory/legacy-port-work-items.tsv').open() as stream:
        assert not any(r['wbs_task'] == '05.013' for r in csv.DictReader(stream, delimiter='\t'))
    print('PASS: original05.013 7 common codes, native/sanitized12 cases each; implementation gates remain open')


if __name__ == '__main__':
    main()
