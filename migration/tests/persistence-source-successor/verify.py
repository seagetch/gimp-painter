#!/usr/bin/env python3
"""Verify retained proof artifacts; source acceptance remains the ordinary gate."""
import hashlib
import json
from pathlib import Path
p = Path(__file__).resolve().parent
records = json.loads((p / 'artifacts.json').read_text())
assert set(records) == {str(f.relative_to(p)) for f in p.rglob('*') if f.is_file() and f.name != 'artifacts.json'}
for name, digest in records.items():
    assert hashlib.sha256((p / name).read_bytes()).hexdigest() == digest, name
report = json.loads((p / 'metadata-report.json').read_text())
assert report['result'] == 'PASS'
assert report['counts']['contract_records'] == 1379
assert report['counts']['syntax_candidates'] == 873
assert report['counts']['routes'] == 16
assert report['negative_controls'] == []
assert len(json.loads((p / 'transitions.json').read_text())['transitions']) == 7
print('PASS: immutable successor artifacts, 1379 records / 873 candidates / 16 routes; source-only frozen baseline')
