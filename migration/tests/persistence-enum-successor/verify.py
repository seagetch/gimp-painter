#!/usr/bin/env python3
"""Verify retained enum-successor receipt artifacts, not native runtime."""
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
transition = json.loads((p / 'transition.json').read_text())
assert len(transition['providers']) == 7
assert len(transition['evidence_successors']) == 2
assert transition['successor_commit'] == '18c43bbe7c2157484201e127c558e77f7c683086'
print('PASS: immutable enum-successor proof, 1379/873/16 source metadata; no native runtime claim')
