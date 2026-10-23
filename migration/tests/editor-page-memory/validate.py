#!/usr/bin/env python3
"""Validate focused page-memory evidence; does not rerun native tests."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

here = Path(__file__).resolve().parent
repo = here.parents[2]
report = json.loads((here / 'report.json').read_text())

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check-source', action='store_true', help='also require current checkout to match the measured candidate')
args = parser.parse_args()
if args.check_source:
    for name, expected in report['source_sha256'].items():
        assert digest(repo / name) == expected, ('source changed', name)
for name, expected in report['evidence_sha256'].items():
    assert digest(here / name) == expected, ('evidence changed', name)

for path in here.rglob('*.json'):
    def check(value):
        if isinstance(value, dict):
            assert 'env' not in value, ('raw environment map', str(path))
            for child in value.values():
                check(child)
        elif isinstance(value, list):
            for child in value:
                check(child)
    check(json.loads(path.read_text()))

runs = json.loads((here / 'page-memory-final.json').read_text())
assert len(runs) == 2
assert [len(re.findall(r'^ok \d+', row['stdout'], re.M)) for row in runs] == [25, 3]
for row in runs:
    assert row['result'] == 'OK' and row['returncode'] == 0
    assert not re.search(r'^(not ok|ok .*# SKIP)', row['stdout'], re.M)
    assert row['duration'] < 120
for filename, mismatch in [('page-memory-expected-red.json', '(0 == 2)'),
                           ('page-popup-expected-red.json', '(0 == 1)')]:
    rows = json.loads((here / filename).read_text())
    assert any(row['result'] == 'FAIL' and mismatch in row['stdout'] for row in rows)
for path in sorted(here.glob('0[456]-*.png')):
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II', data[16:24]) == (1080, 1920)
assert len(list(here.glob('0[456]-*.png'))) == 3
visual = json.loads((here / 'result.json').read_text())
assert visual['geometry'] == [1080, 1920]
assert visual['painter_canvas_ui'] is True and visual['exit_code'] == 0
print('PASS: sanitized logs, 25+3 native cases, two expected-red cases, three portrait PNGs' + ('; current source matches' if args.check_source else ''))
