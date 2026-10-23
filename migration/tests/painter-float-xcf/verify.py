#!/usr/bin/env python3
"""Verify bounded float capsule evidence; never infer whole-branch completion."""
import hashlib
import json
from pathlib import Path
import re
import tarfile


def verify(root):
    def read(name):
        return json.loads((root / name).read_text())
    def digest(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()
    def cases(result, key):
        assert result[key] == 0
        lines = result['stdout'].splitlines()
        assert not any(line.startswith('not ok ') for line in lines)
        passed = [line for line in lines if re.match(r'^ok \d+ ', line) and '# SKIP' not in line]
        return len(passed)
    m = read('manifest.json')
    assert m['task'] == '13.012' and m['parent_status'] == 'DOING'
    assert sum(cases(r, 'returncode') for r in read('normal.json')) == m['normal_cases'] == 115
    san = read('sanitizer-build.json')
    assert sum(cases(r, 'exit_code') for r in san['results'].values()) == m['sanitizer_cases'] == 102
    assert len(san['instrumented_sources']) == 41
    assert len(san['rtti_compatibility_only_sources']) == 41
    assert not san['changed_during_build'] and not san['changed_during_run']
    with tarfile.open(root / 'pre-loader-fix-inputs.tar.gz') as archive:
        hashes = {x.name: hashlib.sha256(archive.extractfile(x).read()).hexdigest()
                  for x in archive.getmembers() if x.isfile()}
    runner = 'migration/tests/run_xcf_field_checkpoint_sanitizers.py'
    assert hashes[runner] != san['sources_sha256'][runner]
    hashes[runner] = digest(root / 'runtime-loader-runner.py')
    for path, expected in san['sources_sha256'].items():
        assert hashes[path] == expected, path
    owned = root / 'owned-before-build'
    for path in owned.rglob('*'):
        if path.is_file():
            assert digest(path) == m['source_sha256'][str(path.relative_to(owned))]
    for name, expected in m['fixture_sha256'].items():
        assert digest(root / 'fixtures' / name) == expected
    assert len(m['fixture_sha256']) == 54
    assert read('prior-fixture-sha256.json') == m['fixture_sha256']
    prior = read('prior-report.json')
    assert prior['files'] == 27 and prior['resaves_per_file'] == 2
    assert 'no backward editing promise' in prior['preserved']
    mutations = read('mutations/mutations.json')
    assert {c['mutation'] for c in mutations['cases']} == {'native-mode-on-wire', 'accept-unknown-policy'}
    assert all(c['rejected'] for c in mutations['cases'])
    assert mutations['restored_sha256'] == m['source_sha256']['app/xcf/painter-xcf-preserve.cpp']
    assert len(read('paused-path-exclusion.json')) == 5
    assert not read('extension-acceptance.json')['original_185_field_checkpoint_changed']
    for name, expected in read('artifact-sha256.json').items():
        assert digest(root / name) == expected, name
    return {'normal_cases': 115, 'selected_sanitizer_cases': 102, 'transport_scenes': 27,
            'prior_reader_resaves': 54, 'production_mutants_rejected': 2, 'parent': 'DOING'}


if __name__ == '__main__':
    print(json.dumps(verify(Path(__file__).resolve().parent), sort_keys=True))
