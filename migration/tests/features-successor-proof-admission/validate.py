#!/usr/bin/env python3
"""Replay immutable bounded proofs and check only their exact successor union."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tarfile

p = argparse.ArgumentParser()
p.add_argument('--check-source', action='store_true')
a = p.parse_args()
e = Path(__file__).resolve().parent
r = e.parents[2]
read = lambda path: json.loads(path.read_text())
sha = lambda data: hashlib.sha256(data).hexdigest()
for filename, digest in read(e / 'support-sha256.json').items():
    assert sha((r / filename).read_bytes()) == digest, filename
plan = read(e / 'transitions.json')
old = r / plan['old_directory']
inputs = {x['path']: x['sha256'] for x in read(old / 'source-manifest.json')}
assert inputs == plan['old_inputs'] and len(inputs) == 26
original = dict(inputs)
subprocess.run([sys.executable, '-I', str(old / 'validate.py')], check=True)
expected = [('preset-legacy-metadata', 'aa12595aa6'),
            ('features-legacy-contract', '857e3ea97a'),
            ('resource-factory-wait', '73b78e3554')]
assert [(Path(t['directory']).name, t['commit']) for t in plan['transitions']] == expected
for t in plan['transitions']:
    directory = r / t['directory']
    incoming = read(directory / t['input_manifest'])
    if t['input_manifest'] == 'asan-build.json':
        incoming = incoming['sources_sha256']
    changes = {f: {'before': inputs.get(f), 'after': h}
               for f, h in incoming.items() if inputs.get(f) != h}
    assert changes == t['changes'], t['directory']
    if t['source_archive']:
        with tarfile.open(directory / t['source_archive']) as archive:
            for f, h in incoming.items():
                assert sha(archive.extractfile(f).read()) == h, f
    else:
        for f, h in incoming.items():
            assert sha((directory / 'owned-before-build' / f).read_bytes()) == h, f
    subprocess.run([sys.executable, '-I', str(directory / 'verify.py')], check=True)
    inputs.update(incoming)
assert sorted(f for f in original if original[f] != inputs[f]) == plan['allowed_changed_original_inputs']
assert sorted(set(inputs) - set(original)) == plan['new_inputs']
assert len(plan['allowed_changed_original_inputs']) == 6 and len(plan['new_inputs']) == 2
manifest = read(e / 'source-manifest.json')
assert len(manifest) == len(inputs) == 28
assert {x['path']: x['sha256'] for x in manifest} == inputs
with tarfile.open(e / 'tested-successor-sources.tar.gz') as archive:
    for f, h in inputs.items():
        assert sha(archive.extractfile(f).read()) == h, f
        if a.check_source:
            assert sha((r / f).read_bytes()) == h, f
print('PASS: immutable original + three exact transitions; six changed/two added inputs; 28 source identities; source-only admission, not new runtime or full parity')
