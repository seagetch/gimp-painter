#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path
p = Path(__file__).resolve().parent
hashes = json.loads((p / 'artifacts.json').read_text())
assert set(hashes) == {f.name for f in p.iterdir() if f.is_file() and f.name != 'artifacts.json'}
for name, digest in hashes.items():
    assert hashlib.sha256((p / name).read_bytes()).hexdigest() == digest, name
r = json.loads((p / 'metadata-report.json').read_text())
assert r['result'] == 'PASS'
assert (r['counts']['contract_records'], r['counts']['syntax_candidates'], r['counts']['routes']) == (1379, 873, 16)
assert r['negative_controls'] == []
print('PASS: immutable four-provider readiness successor metadata proof; 1379/873/16; no new runtime claim')
