#!/usr/bin/env python3
import hashlib,json
from pathlib import Path
p=Path(__file__).resolve().parent
hashes=json.loads((p/'artifacts.json').read_text())
assert set(hashes)=={f.name for f in p.iterdir() if f.is_file() and f.name!='artifacts.json'}
for name,h in hashes.items():assert hashlib.sha256((p/name).read_bytes()).hexdigest()==h,name
r=json.loads((p/'metadata-report.json').read_text());assert r['result']=='PASS'
assert (r['counts']['contract_records'],r['counts']['syntax_candidates'],r['counts']['routes'])==(1379,873,16)
assert r['negative_controls']==[]
assert len(json.loads((p/'immutable-field-inputs.json').read_text()))==8
r=json.loads((p/'source-tests.json').read_text());assert len(r)==4 and all(x['returncode']==0 for x in r)
print('PASS: exact catalog persistence successor; 1379/873/16 unchanged; no new runtime claim')
