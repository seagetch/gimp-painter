#!/usr/bin/env python3
"""Verify bounded native/source-language evidence; no old runtime parity claim."""
from pathlib import Path
import json,hashlib,tarfile,re,argparse
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args()
d=Path(__file__).resolve().parent;r=d.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
old=json.loads((d/'source-contract.json').read_text())
for name,h in old['sha256'].items():assert sha((d/'old-source'/name).read_bytes())==h,name
assert old['exact_body'] in (d/'probe.cpp').read_text()
assert '&typeid(config)' in old['exact_body'] and 'typeid(*config)' not in old['exact_body']
for f in ('normal.log','sanitizer.log'):assert 'PASS exact original register_config body:' in (d/f).read_text()
rows=json.loads((d/'native-results.json').read_text());assert len(rows)==2
assert sorted(int(re.search(r'^1\.\.(\d+)$',x['stdout'],re.M)[1]) for x in rows)==[6,13]
for x in rows:assert x['returncode']==0 and x['result']=='OK' and 'not ok' not in x['stdout']
rows=json.loads((d/'asan-results.json').read_text());assert len(rows)==2
assert sorted(int(re.search(r'^1\.\.(\d+)$',x['stdout'],re.M)[1]) for x in rows)==[6,13]
for x in rows:assert x['returncode']==0 and 'not ok' not in x['stdout']
b=json.loads((d/'asan-build.json').read_text());assert len(b['instrumented_sources'])==14 and not b['changed_during_build']
with tarfile.open(d/'native-tested-sources.tar.gz') as archive:
 for name,h in b['sources_sha256'].items():
  assert sha(archive.extractfile(name).read())==h,name
  if a.check_source:assert sha((r/name).read_bytes())==h,name
mapping=json.loads((d/'native-replacement-map.json').read_text());assert len(mapping['rows'])==10
assert 'DOING' in (d/'README.md').read_text()
assert '1080x1920x24' in (d/'native-tests.log').read_text()
print('PASS native13+6/selected14-unit ASan13+6; pinned static-key probe;10explicit native mappings; old runtime/arbitrary parity NOT claimed')
