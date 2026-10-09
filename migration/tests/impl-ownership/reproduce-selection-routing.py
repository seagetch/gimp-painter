#!/usr/bin/env python3
"""Reproduce one pinned selection-helper routing correction, not a full-tree audit."""
import argparse
from collections import Counter
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from assign_legacy_hunks import tsv, task_catalog
from legacy_assignment_rules import route
import audit_legacy_granularity as granularity

BASE = 'ea5dc194d17cdbfcda753b0b613e2af797e76ab3'
SID = '01.002/000079'
SOURCE = 'app/base/selectcase-utils.hpp'
REMOVED = {'legacy-c472f83aef6fe6246e89', 'legacy-8ddcc7c0964a9622e9ce'}
COMPLETED = {'legacy-bb33363ce7f20440f007', 'legacy-6ea0a43254bd0aaf640c'}
INV = ROOT / 'migration/inventory'

def digest(data):
    return hashlib.sha256(data).hexdigest()

def prior(name):
    return subprocess.check_output(['git', 'show', BASE + ':migration/inventory/' + name], cwd=ROOT)

def rows(data):
    return list(csv.DictReader(io.StringIO(data.decode()), delimiter='\t'))

def reproduce():
    names = ['changed-hunks.tsv', 'hunk-wbs.tsv', 'cleanup-candidate-review.tsv',
             'legacy-port-work-items.tsv']
    tables = {name: rows(prior(name)) for name in names}
    old_work = {r['work_id']: r for r in tables['legacy-port-work-items.tsv']}
    assignment = next(r for r in tables['hunk-wbs.tsv'] if r['hunk_id'] == SID)
    hunk = next(r for r in tables['changed-hunks.tsv'] if r['child_id'] == SID)
    assert assignment['path'] == hunk['path'] == SOURCE and assignment['base_blob'] == ''
    with tarfile.open(Path(__file__).parent / 'evidence.tar.gz') as archive:
        source = archive.extractfile('legacy/selectcase-utils.hpp').read()
    blob = hashlib.sha1(b'blob ' + str(len(source)).encode() + b'\0' + source).hexdigest()
    payload = b''.join(b'+' + line for line in source.splitlines(keepends=True))
    assert blob == assignment['source_blob'] == '35ef728e6cba705cd168247a27660d1f0656d7da'
    assert digest(payload) == assignment['payload_sha256']
    assert len(source.splitlines()) == 123 and hunk['old_new_lines'] == '-0,0 -> 1,123'
    result = route(hunk, source.decode().splitlines(), [])
    assert result['profile'] == 'cpp-selection'
    assert result['tasks'] == ['06.021', '06.029'] and result['tests'] == ['07.013', '07.016']
    assert set(assignment['implementation_tasks'].split(',')) - set(result['tasks']) == {'06.010', '06.022'}
    hunk.update(feature_or_base=result['feature'], disposition_task=','.join(result['tasks']))
    assignment.update(profile=result['profile'], feature=result['feature'],
                      implementation_tasks=','.join(result['tasks']), reason=result['reason'])
    candidate = next(r for r in tables['cleanup-candidate-review.tsv'] if r['path'] == SOURCE)
    candidate['implementation_tasks'] = ','.join(result['tasks'])
    outputs = {name: tsv(tuple(tables[name][0]), tables[name]).encode() for name in names[:3]}
    with tempfile.TemporaryDirectory(prefix='painter-selection-routing-') as directory:
        target = Path(directory)
        (target / 'hunk-wbs.tsv').write_bytes(outputs['hunk-wbs.tsv'])
        (target / 'asset-wbs.tsv').write_bytes(prior('asset-wbs.tsv'))
        expected, checks = granularity.specification(task_catalog(), target)
    assert set(old_work) - {r['work_id'] for r in expected} == REMOVED
    assert len(expected) == len(old_work) - 2
    for work in REMOVED:
        assert old_work[work]['source_id'] == SID and old_work[work]['status'] == 'TODO'
    for row in expected:
        old = old_work[row['work_id']]
        assert all(row[k] == old[k] for k in granularity.FIELDS if k not in granularity.MUTABLE)
        row.update({k: old[k] for k in granularity.MUTABLE})
    assert all(r == old_work[r['work_id']] for r in expected)
    outputs['legacy-port-work-items.tsv'] = tsv(granularity.FIELDS, expected).encode()
    outputs['granularity-review.tsv'] = tsv(granularity.AUDIT_FIELDS, checks).encode()
    summary = json.loads(prior('wbs-assignment-summary.json'))
    summary.update(profiles=dict(sorted(Counter(r['profile'] for r in tables['hunk-wbs.tsv']).items())),
                   hunk_ledger_sha256=digest(outputs['hunk-wbs.tsv']))
    outputs['wbs-assignment-summary.json'] = (json.dumps(summary, indent=2) + '\n').encode()
    summary = json.loads(prior('changed-hunks.json'))
    summary['inventory_sha256'] = digest(outputs['changed-hunks.tsv'])
    outputs['changed-hunks.json'] = (json.dumps(summary, indent=2) + '\n').encode()
    summary = json.loads(prior('granularity-summary.json'))
    summary['implementation_actions'] -= 2
    outputs['granularity-summary.json'] = (json.dumps(summary, indent=2) + '\n').encode()
    for name in ['changed-files.tsv', 'asset-wbs.tsv', 'auxiliary-script-review.tsv',
                 'auxiliary-generator-relations.tsv']:
        assert (INV / name).read_bytes() == prior(name), 'Unrelated inventory changed: ' + name
    report = {'baseline': BASE, 'source': SOURCE, 'hunk': SID, 'source_blob': blob,
              'payload_sha256': digest(payload), 'removed_todo_work_ids': sorted(REMOVED),
              'preserved_selection_work_ids': [r['work_id'] for r in expected if r['source_id'] == SID],
              'preserved_work_items': len(expected),
              'done_before_implementation_acceptance': sum(r['status'] == 'DONE' for r in expected),
              'assignment_correction_added_done': 0, 'full_historical_generator_pass': False,
              'limitation': 'Only the byte-verified affected added file is rerouted; absent complete historical trees are not claimed regenerated.',
              'corrected_table_sha256_before_implementation_acceptance': {n: digest(b) for n, b in outputs.items()}}
    return outputs, report

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    outputs, report = reproduce()
    if args.write:
        for name, data in outputs.items():
            assert (INV / name).read_bytes() in (prior(name), data), 'Refusing to overwrite later work: ' + name
        for name, data in outputs.items():
            (INV / name).write_bytes(data)
    else:
        for name, data in outputs.items():
            actual = (INV / name).read_bytes()
            if name == 'legacy-port-work-items.tsv':
                expected_rows, actual_rows = rows(data), rows(actual)
                assert len(expected_rows) == len(actual_rows)
                for old, new in zip(expected_rows, actual_rows):
                    assert old['work_id'] == new['work_id']
                    for key in granularity.FIELDS:
                        if old['work_id'] in COMPLETED and key in granularity.MUTABLE:
                            continue
                        assert old[key] == new[key], (old['work_id'], key)
            else:
                assert actual == data, 'Routing output differs: ' + name
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
