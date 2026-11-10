#!/usr/bin/env python3
"""Verify the narrow runtime receipts and unchanged tested source inputs."""
import hashlib, json
from pathlib import Path
here = Path(__file__).resolve().parent; root = here.parents[2]
seal = json.loads((here / 'final-inputs.json').read_text())
for path, expected in seal['sha256'].items():
    assert hashlib.sha256((root / path).read_bytes()).hexdigest() == expected, path
native = json.loads((here / 'native-results.json').read_text())['runs']
assert len(native) == 8 and all(r['exit_code'] == 0 for r in native)
for r in native:
    assert hashlib.sha256((here / r['log']).read_bytes()).hexdigest() == r['sha256'], r['log']
controls = json.loads((here / 'controls.json').read_text())['controls']
assert len(controls) == 2
for r in controls:
    assert r['mutant_exit'] == -6 and r['restored_exit'] == 0 and r['source_restored']
    assert seal['sha256'][r['source']] == r['fixed_sha256']
san = json.loads((here / 'sanitizers.json').read_text())
assert not san['changed_during_run'] and len(san['runs']) == 3
assert all(r['exit_code'] == 0 for r in san['runs'])
assert len([c for c in san['commands'] if '-c' in c]) == 23
for path, expected in san['sources_sha256'].items():
    assert hashlib.sha256((root / path).read_bytes()).hexdigest() == expected, path
for route in ['raw', 'path', 'boundary']:
    runs = json.loads((here / ('generic-' + route + '.json')).read_text())['runs']
    assert len(runs) == 1 and runs[0]['exact'] and runs[0]['exit_code'] == 0
for name, count in [('gimp-painter-smudge', 28), ('painter-brush-geometry', 7), ('painter-smudge-ui', 26)]:
    lines = (here / ('final-' + name + '.log')).read_text().splitlines()
    assert len([line for line in lines if line.startswith('ok ')]) == count, name
assert '| 23.004 | [ ] |' in (root / 'tasks.md').read_text()
integration = json.loads((here / 'integration.json').read_text())
for path, expected in integration['preset_sources_sha256'].items():
    assert hashlib.sha256((root / path).read_bytes()).hexdigest() == expected, path
preset = json.loads((here / 'integration-preset-results.json').read_text())['runs']
assert len(preset) == 5 and sum(r['passed_cases'] for r in preset) == 72
assert all(r['exit_code'] == 0 for r in preset)
for r in preset:
    assert hashlib.sha256((here / r['log']).read_bytes()).hexdigest() == r['log_sha256']
print('Smudge defaults: 8 native groups, 2 causal red/restores, 23-unit sanitizers and current source hashes PASS; parent 23.004 remains open.')
