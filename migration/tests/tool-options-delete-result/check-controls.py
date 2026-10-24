#!/usr/bin/env python3
"""Causal receipt negatives; no filesystem deletion or held candidate execution."""
import copy
import importlib.util
import json
from pathlib import Path
p = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('proof', p / 'validate.py')
proof = importlib.util.module_from_spec(spec); spec.loader.exec_module(proof)
original = json.loads((p / 'results.json').read_text())
controls = []
for label in ('changed-source', 'changed-link', 'missing-permission-case', 'missing-asan-unit', 'false-red'):
    data = copy.deepcopy(original)
    if label == 'changed-source': data['clean_source_changed'] = ['synthetic-unreviewed.c']
    if label == 'changed-link': data['linked_inputs_changed'] = ['synthetic-unreviewed.o']
    if label == 'missing-permission-case':
        row = next(r for r in data['results'] if r['name'] == 'normal-run')
        row['stdout'] = row['stdout'].replace('caller-permission', 'removed-case')
    if label == 'missing-asan-unit':
        row = next(r for r in data['results'] if r['name'].startswith('asan-compile-'))
        data['results'].remove(row)
    if label == 'false-red':
        row = next(r for r in data['results'] if r['name'] == 'red-run'); row['returncode'] = 0
    try: proof.validate_results(data)
    except AssertionError: controls.append(label)
    else: raise AssertionError('accepted false evidence: ' + label)
print('PASS: five receipt controls rejected: ' + ', '.join(controls))
