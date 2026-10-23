#!/usr/bin/env python3
"""Verify pinned source excerpts and expectation-contract coverage, not runtime."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--legacy-source', required=True, type=Path)
args = parser.parse_args()
evidence = json.loads((HERE / 'source-evidence.json').read_text())
for item in evidence['sources']:
    raw = subprocess.check_output(['git', '-C', str(args.legacy_source), 'show', evidence['source_commit'] + ':' + item['path']])
    assert hashlib.sha256(raw).hexdigest() == item['full_source_sha256']
    lines = raw.decode().splitlines(True)
    for excerpt in item['excerpts']:
        assert ''.join(lines[excerpt['start_line']-1:excerpt['end_line']]) == excerpt['text']
root = HERE.parents[2]
contract = (root / 'migration/contracts/legacy-clone-name-resolution.md').read_text()
for term in ['First matching container index', 'Group wins', 'existing pointer', 'pending name remains', 'does not exclude self', 'not** claims a new']:
    assert term in contract, term
source = (root / 'app/core/gimpclonelayer.cpp').read_text()
assert 'Legacy pre-order: compare parent before descending' in source
assert 'return ref ? GIMP_LAYER (ref.get ()) : nullptr' in source
native = (root / 'app/tests/test-gimp-clone-layer.c').read_text()
for name in ['deferred_name', 'recursive_first_match']:
    assert 'static void ' + name + ' (void)' in native
print('Pinned-source hashes/excerpts and recorded expectation coverage PASS; no native runtime claim.')
