#!/usr/bin/env python3
"""Verify original05.012 ownership domains and source-matched native evidence."""
import csv
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    contract = json.loads((ROOT / 'migration/contracts/cpp-main-context.json').read_text())
    assert contract['task'] == '05.012' and contract['status'] == 'DEFINED'
    routes = contract['routes']
    assert len(routes) == len({r['id'] for r in routes}) == 9
    for row in routes:
        assert row['owner'] and row['operations'] and row['boundary'] and row['close_and_final_release']
        for source in row['source_evidence']:
            path, line = source.rsplit(':', 1)
            assert path in contract['source_sha256'] and 0 < int(line) <= len((ROOT / path).read_text().splitlines())
    for path, digest in contract['source_sha256'].items():
        assert sha(ROOT / path) == digest, path
    report = json.loads((ROOT / 'migration/tests/main-context/report.json').read_text())
    assert [(r['variant'], r['exit_code'], r['passed']) for r in report['results']] == [('native', 0, 1), ('sanitized', 0, 1)]
    assert report['LSan'] is False and report['vptr'] is False
    for path, digest in report['source_sha256'].items():
        assert sha(ROOT / path) == digest, path
    native = json.loads((ROOT / 'migration/tests/main-context/native-results.json').read_text())
    assert [(r['name'], r['exit_code'], r['passed']) for r in native] == [
        ('painter-filter-owner-gates', 0, 5), ('painter-work-admission', 0, 13),
        ('painter-filter-raster', 0, 7), ('painter-filter-spool', 0, 11)]
    for path, digest in contract['evidence_sha256'].items():
        assert sha(ROOT / path) == digest, path
    with (ROOT / 'migration/inventory/legacy-port-work-items.tsv').open() as stream:
        assert not any(r['wbs_task'] == '05.012' for r in csv.DictReader(stream, delimiter='\t'))
    print('PASS: original05.012 9 ownership domains, native/sanitized context distinction and36 worker-boundary cases')


if __name__ == '__main__':
    main()
