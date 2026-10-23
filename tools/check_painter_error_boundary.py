#!/usr/bin/env python3
"""Verify pinned original05.013 receipts separately from current foundation tests."""
import csv
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import tarfile

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
    results = json.loads((d / 'native-results.json').read_text())
    assert [(r['variant'], r['group'], r['exit_code'], r['passed']) for r in results['results']] == [
        (variant, group, 0, count) for variant in ('native', 'sanitized')
        for group, count in [('interop', 5), ('boundary', 2), ('store', 5)]]
    assert results['LSan'] is False and results['vptr'] is False
    for path, digest in report['evidence_sha256'].items():
        assert sha(ROOT / path) == digest, path
    # These old results attest the original source, never today's different code.
    pin = json.loads((d / 'historical-pin.json').read_text())
    assert pin['commit'] == 'f299d8a5f3982e8bfcae366dc0d2e5a573f6090b'
    assert sha(d / 'historical-source.tar.gz') == pin['archive_sha256']
    maps = [report['source_sha256'], results['source_sha256']]
    paths = set().union(*(set(m) for m in maps)) | {'tasks.md'}
    assert set(pin['source_paths']) == paths
    with tarfile.open(d / 'historical-source.tar.gz') as archive:
        members = archive.getmembers()
        assert len(members) == len(paths) and {m.name for m in members} == paths
        assert all(m.isfile() and not PurePosixPath(m.name).is_absolute() and
                   '..' not in PurePosixPath(m.name).parts for m in members)
        for source_map in maps:
            for path, digest in source_map.items():
                assert hashlib.sha256(archive.extractfile(path).read()).hexdigest() == digest, path
        old_tasks = archive.extractfile('tasks.md').read().decode()
    tasks = (ROOT / 'tasks.md').read_text()
    for task in report['remaining_implementation_tasks']:
        assert re.search(r'^\| ' + re.escape(task) + r' \| \[ \]', old_tasks, re.M), task
        # Successors may legitimately complete; each remains a separate ledger duty.
        rows = re.findall(r'^\| ' + re.escape(task) + r' \| (.*)$', tasks, re.M)
        assert len(rows) == 1 and rows[0].startswith(('[ ] |', '[x] |')), task
        assert '05.013' in rows[0].split('|')[2], task
    assert '契約定義行を必須先行 ID に持つ独立した実装・検証行' in tasks
    with (ROOT / 'migration/inventory/legacy-port-work-items.tsv').open() as stream:
        assert not any(r['wbs_task'] == '05.013' for r in csv.DictReader(stream, delimiter='\t'))
    # Current evidence has its own source hashes; no historical resealing.
    current = d / 'current-foundation'
    manifest = json.loads((current / 'artifact-sha256.json').read_text())
    assert set(manifest) == {str(p.relative_to(current)) for p in current.rglob('*')
                             if p.is_file() and p.name != 'artifact-sha256.json'}
    for path, digest in manifest.items():
        assert sha(current / path) == digest, path
    names = []
    for variant, sanitizers in [('normal', []), ('sanitized', ['address', 'undefined'])]:
        result = json.loads((current / (variant + '.json')).read_text())
        assert result['status'] == 'PASS' and result['sanitizers'] == sanitizers
        assert result['leak_check'] == 'not run'
        for path, digest in result['source_sha256'].items():
            assert sha(ROOT / path) == digest, path
        log = result['output'][-1]
        assert not re.search(r'^not ok|^ok .*#\s*SKIP|ERROR: AddressSanitizer|runtime error:', log, re.M|re.I)
        plan = re.findall(r'^1\.\.(\d+)$', log, re.M)
        tests = re.findall(r'^ok \d+ (\S+)', log, re.M)
        assert len(plan) == 1 and len(tests) == int(plan[0]) and len(set(tests)) == len(tests)
        assert sum(t.startswith('/painter/interop/') for t in tests) == 6
        assert sum(t.startswith('/painter/boundary/') for t in tests) == 2
        assert sum(t.startswith('/painter/store/') for t in tests) == 12
        names.append(tests)
    assert names[0] == names[1]
    print('PASS: pinned original05.013 receipts plus current native/ASan foundation; successor statuses independent')


if __name__ == '__main__':
    main()
