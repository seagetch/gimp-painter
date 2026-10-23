#!/usr/bin/env python3
"""Check the native property failure/recovery checkpoint and its source seals."""
import csv
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import tarfile

ROOT = Path(__file__).resolve().parents[1]
D = ROOT / 'migration/tests/property-boundary'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    report = json.loads((D / 'report.json').read_text())
    assert report['task'] == '05.013/legacy-exit-removal' and report['status'] == 'PASS'
    for path, digest in report['source_sha256'].items():
        assert sha((ROOT / path).read_bytes()) == digest, path
    spec = importlib.util.spec_from_file_location('vfunc', ROOT / 'tools/check_painter_vfunc_order.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    current = module.discover()
    assert current == json.loads((D / 'initializer-inventory.json').read_text())
    assert len(current) == 31 and sum(len(t['initializers']) for t in current.values()) == 39
    slots = [c['slot'] for t in current.values() for i in t['initializers'] for c in i['assigned_callbacks']]
    assert slots.count('set_property') == 7 and slots.count('get_property') == 8
    owners = [t for t in current.values() if any(c['slot'] in ('set_property', 'get_property')
              for i in t['initializers'] for c in i['assigned_callbacks'])]
    assert len(owners) == 9
    manifest = json.loads((D / 'evidence-manifest.json').read_text())
    archive = (D / 'evidence.tar.gz').read_bytes()
    assert len(archive) == report['evidence_bytes'] and sha(archive) == report['evidence_sha256']
    data = {}
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        members = tar.getmembers()
        assert len(members) == len(manifest) == report['evidence_members']
        assert len({m.name for m in members}) == len(members)
        for member in members:
            assert member.isfile() and not Path(member.name).is_absolute() and '..' not in Path(member.name).parts
            raw = tar.extractfile(member).read(); raw.decode('utf-8')
            assert len(raw) == manifest[member.name]['size'] and sha(raw) == manifest[member.name]['sha256']
            data[member.name] = raw
    def text(path): return data[path].decode()
    def load(path): return json.loads(data[path])
    for path, digest in report['source_sha256'].items():
        assert sha(data['current-source/' + path]) == digest
    for variant in ('baseline', 'baseline-san', 'fixed', 'fixed-san'):
        result = load('native/options/' + variant + '-result.json')
        log = text('native/options/' + variant + '.run.log')
        assert result['exit_code'] == 0 and not result['timed_out']
        assert sha(log.encode()) == result['log_sha256']
        assert 'outcome=pass cases=14 ' in log and 'owners_created=14 owners_finalized=14' in log
        rows = [line for line in log.splitlines() if line.startswith('postcommit ')]
        assert len(rows) == 3
        if variant.startswith('baseline'):
            assert all('settings_changed=0' in line and 'dirty_notify=0' in line for line in rows)
            assert 'path=checked checked_return=0' in rows[0] and 'changed=1' in rows[0]
        else:
            assert all('settings_changed=1' in line and 'dirty_notify=1' in line for line in rows)
            assert 'path=checked checked_return=1 error=none' in rows[0]
            assert log.count('hits=0 attempts=0 allocation_size=0 frames=0 reached_observer=1') == 3
    for site in range(9):
        log = text(f'native/guide/baseline-{site}.log')
        assert f'fail_at={site}' in log and 'escaped=0' in log
        assert ('after_undo_null=1' in log) == (site >= 5)
    baseline = load('native/guide/baseline-results.json') + load('native/guide/baseline-failure-results.json')
    assert {r['site'] for r in baseline} == set(range(9)) and len(baseline) == 9
    assert all(r['exit_code'] == 0 for r in baseline)
    for variant in ('fixed', 'fixed-san'):
        results = load('native/guide/' + variant + '-results.json')
        assert len(results) == 12 and all(r['exit_code'] == 0 for r in results)
        for row in results:
            log = text('native/guide/' + row['log'])
            assert 'assertions=pass recovery=pass' in log
            assert not re.search(r'ERROR: AddressSanitizer|runtime error:', log)
            if row['site'] and len(row['arguments']) == 1:
                assert 'depth=0 before_undo_unchanged=1 after_undo_null=0 after_undo_id=99' in log
                assert 'redo_before=1 redo_after=1' in log
            if 'group' in row['arguments']:
                assert 'group_depth=0' in log
    for variant in ('controls', 'controls-san'):
        rows = load('native/guide/' + variant + '-results.json')
        assert len(rows) == 1 and rows[0]['exit_code'] == 0
        log = text('native/guide/' + rows[0]['log'])
        assert 'outcome=pass controls=8' in log and 'diagnostic owner release=pass' in log
        assert not re.search(r'ERROR: AddressSanitizer|runtime error:', log)
    native = load('native/guide/native-tests.json')
    assert len(native) == 4 and sum(r['passed'] for r in native) == 35
    for row in native:
        assert row['exit_code'] == 0 and sha(data['native/guide/' + row['log']]) == row['log_sha256']
    old = data['legacy-glib-cxx-impl.hpp']
    assert hashlib.sha1(b'blob ' + str(len(old)).encode() + b'\0' + old).hexdigest() == 'a2ad35e37a3e7a481698c1f8b78b71adb385f7d3'
    original = list(csv.DictReader(io.StringIO(text('baseline-review/original-header-ledger.tsv')), delimiter='\t'))
    with (ROOT / 'migration/inventory/legacy-port-work-items.tsv').open() as stream:
        ledger = list(csv.DictReader(stream, delimiter='\t'))
    by_id = {r['work_id']: r for r in ledger}
    assert len(original) == 20 and all(by_id[r['work_id']] == r for r in original)
    assert not any(r['wbs_task'] == report['task'] for r in ledger)
    tasks = (ROOT / 'tasks.md').read_text()
    assert '| 05.013/legacy-exit-removal | [x] |' in tasks
    assert '| 38.004/all-vfunc-exception-containment | [ ] |' in tasks
    print('PASS: property boundary;31 types/15 handlers;Options 4x14;Guide 9 baseline+2x12;controls2x8;native35')


if __name__ == '__main__':
    main()
