#!/usr/bin/env python3
"""Reproduce the bounded original 04.013 routing correction from its checkpoint.

This is a historical correction check, not a current all-feature acceptance gate.
It preserves unrelated execution states at the recorded baseline and never marks
the missing MyPaint-selection API as implemented.
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tempfile

import audit_legacy_granularity as granularity
from assign_legacy_hunks import ROOT, INV, tsv, task_catalog
from legacy_assignment_rules import route


def digest(data):
    return hashlib.sha256(data).hexdigest()


def table(data):
    return list(csv.DictReader(io.StringIO(data.decode()), delimiter='\t'))


def outputs():
    review = json.loads((INV/'enum-generation-routing-review.json').read_text())
    revision = review['baseline_commit']
    names = ['changed-hunks.tsv', 'hunk-wbs.tsv', 'cleanup-candidate-review.tsv',
             'auxiliary-script-review.tsv', 'legacy-port-work-items.tsv',
             'granularity-review.tsv', 'granularity-summary.json',
             'wbs-assignment-summary.json', 'changed-hunks.json', 'asset-wbs.tsv']
    baseline = {name: subprocess.check_output(
        ['git', 'show', revision+':migration/inventory/'+name], cwd=ROOT)
        for name in names}
    rows = {name: table(data) for name, data in baseline.items() if name.endswith('.tsv')}
    h_by = {row['child_id']: row for row in rows['changed-hunks.tsv']}
    a_by = {row['hunk_id']: row for row in rows['hunk-wbs.tsv']}
    work = rows['legacy-port-work-items.tsv']
    ids = {row['source_id'] for row in review['source_registration_review']}
    assert len(ids) == len(review['source_registration_review']) == 4
    removed = set()
    for record in review['source_registration_review']:
        sid = record['source_id']; h = h_by[sid]; a = a_by[sid]
        assert h['path'] == a['path'] == record['source_path']
        assert a['source_blob'] == record['source_blob']
        assert h['old_new_lines'] == a['old_new_lines'] == record['source_range']
        assert digest(record['source_payload'].encode()) == a['payload_sha256'] == record['source_payload_sha256']
        old_duties = [row for row in work if row['source_id'] == sid]
        assert len(old_duties) == 4 and all(row['status'] == 'TODO' for row in old_duties)
        for old, frozen in zip(old_duties, record['existing_duties']):
            assert all(old[key] == value for key, value in frozen.items())
        assert record['implementation_tasks'] == ['30.012', '30.013']
        assert record['verification_tasks'] == ['34.013']
        lines = record['source_payload'].splitlines()
        routing = route(h, [line[1:] for line in lines if line.startswith('+')],
                        [line[1:] for line in lines if line.startswith('-')])
        assert routing['tasks'] == record['implementation_tasks']
        assert routing['tests'] == record['verification_tasks']
        assert routing['reason'] == record['reason']
        a.update(implementation_tasks=','.join(routing['tasks']), reason=routing['reason'])
        h.update(disposition_task=','.join(routing['tasks']))
        removed.add(record['removed_duplicate_work_id'])
    assert len(removed) == 4
    paths = {h_by[sid]['path'] for sid in ids}
    for row in rows['cleanup-candidate-review.tsv']:
        if row['path'] in paths:
            assigned = [a for a in a_by.values() if a['path'] == row['path']]
            for key in ['implementation_tasks', 'verification_tasks']:
                row[key] = ','.join(sorted({task for a in assigned for task in a[key].split(',')}))
    for row in rows['auxiliary-script-review.tsv']:
        if row['hunk_id'] in ids:
            a = a_by[row['hunk_id']]
            row.update(input_output_contract=a['feature']+'. '+a['reason'],
                       implementation_tasks=a['implementation_tasks'],
                       verification_tasks=a['verification_tasks'])
    result = {name: tsv(tuple(rows[name][0]), rows[name]).encode() for name in names[:4]}
    summary = json.loads(baseline['wbs-assignment-summary.json'])
    summary['hunk_ledger_sha256'] = digest(result['hunk-wbs.tsv'])
    result['wbs-assignment-summary.json'] = (json.dumps(summary, indent=2)+'\n').encode()
    summary = json.loads(baseline['changed-hunks.json'])
    summary['inventory_sha256'] = digest(result['changed-hunks.tsv'])
    result['changed-hunks.json'] = (json.dumps(summary, indent=2)+'\n').encode()
    with tempfile.TemporaryDirectory(prefix='enum-routing-') as directory:
        temporary = Path(directory)
        (temporary/'hunk-wbs.tsv').write_bytes(result['hunk-wbs.tsv'])
        (temporary/'asset-wbs.tsv').write_bytes(baseline['asset-wbs.tsv'])
        expected, checks = granularity.specification(task_catalog(), temporary)
    previous = {row['work_id']: row for row in work}
    assert set(previous)-{row['work_id'] for row in expected} == removed
    assert not ({row['work_id'] for row in expected}-set(previous))
    for row in expected:
        old = previous[row['work_id']]
        assert all(row[key] == old[key] for key in granularity.FIELDS if key not in granularity.MUTABLE)
        row.update({key: old[key] for key in granularity.MUTABLE})
    assert expected == [row for row in work if row['work_id'] not in removed]
    assert sum(row['status'] == 'DONE' for row in expected) == 256
    result['legacy-port-work-items.tsv'] = tsv(granularity.FIELDS, expected).encode()
    result['granularity-review.tsv'] = tsv(granularity.AUDIT_FIELDS, checks).encode()
    summary = json.loads(baseline['granularity-summary.json'])
    summary['implementation_actions'] = sum(row['phase'] == 'implementation' for row in expected)
    summary['verification_actions'] = sum(row['phase'] == 'verification' for row in expected)
    result['granularity-summary.json'] = (json.dumps(summary, indent=2)+'\n').encode()
    assert (INV/'asset-wbs.tsv').read_bytes() == baseline['asset-wbs.tsv']
    evidence = {'status': 'PASS', 'baseline_commit': revision, 'scope': 'Four source-specific duplicate generator assignments; historical checkpoint',
                'removed_duplicate_todo_duties': sorted(removed), 'retained_feature_todo_duties': 12,
                'unchanged_work_rows': len(expected), 'done_rows_unchanged': 256,
                'unchanged_work_sha256': digest(json.dumps(expected, sort_keys=True, ensure_ascii=False).encode()),
                'missing_mypaint_selection_api_completed': False,
                'full_legacy_tree_regeneration': False}
    return result, evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--regenerate', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    result, evidence = outputs()
    for name, data in result.items():
        if args.regenerate:
            (INV/name).write_bytes(data)
        else:
            assert (INV/name).read_bytes() == data, 'Historical routing checkpoint differs: '+name
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(evidence, indent=2)+'\n')
    print(json.dumps(evidence, sort_keys=True))


if __name__ == '__main__':
    main()
