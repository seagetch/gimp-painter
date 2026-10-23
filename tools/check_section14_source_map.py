#!/usr/bin/env python3
"""Validate original Clone duties and committed-source mapping (not runtime proof)."""
import csv
import hashlib
import io
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]

def committed(revision, path):
    return subprocess.check_output(['git', 'show', revision + ':' + path], cwd=ROOT)

def main():
    data = json.loads((ROOT / 'migration/contracts/section14-source-duty-map.json').read_text())
    original = list(csv.DictReader(io.StringIO(committed('HEAD', 'migration/inventory/legacy-port-work-items.tsv').decode()), delimiter='\t'))
    original = [row for row in original if row['wbs_task'].startswith('14.')]
    assert data['work_items'] == original, 'Original inventory identity/action/criterion columns changed'
    assert len(original) == 106
    source_ids = {row['source_id'] for row in original}
    assert len(source_ids) == 38
    assert {row['source_id'] for row in data['source_resolutions']} == source_ids
    baseline = data['baseline_commit']
    for row in data['source_resolutions']:
        old = next(item for item in original if item['source_id'] == row['source_id'])
        for key in ['path', 'source_blob', 'source_range', 'source_sha256', 'source_scope']:
            assert row[key] == old[key], (row['source_id'], key)
        assert set(item['wbs_task'] for item in original if item['source_id'] == row['source_id']) <= set(row['owners'])
        for provider in row['current_mapping']:
            content = committed(baseline, provider['path'])
            assert hashlib.sha256(content).hexdigest() == provider['committed_sha256'], provider['path']
            assert provider['anchor'], 'Missing reviewed semantic anchor'
        for path in row['evidence']:
            assert (ROOT / path).exists(), path
    tasks = {}
    for line in committed('HEAD', 'tasks.md').decode().splitlines():
        cells = [part.strip() for part in line.split('|')]
        if len(cells) >= 7 and cells[1].startswith('14.'):
            tasks[cells[1]] = cells
    for task, evidence in data['per_task_artifact_index'].items():
        cells = tasks[task]
        assert evidence['original_action'] == cells[3], task
        assert evidence['original_criterion'] == cells[5], task
        assert evidence['dependencies'] == cells[4], task
        for path in evidence['additional_current_evidence']:
            assert (ROOT / path).exists(), path
    print('PASS:106 exact inventory rows,38 original hunks/owner mappings,28 task criteria/dependencies; provider hashes match committed ' + baseline)
    print('Semantic anchors are human-reviewed descriptions, not an automatic behavioral proof; runtime receipts retain separate source scopes.')

if __name__ == '__main__':
    main()
