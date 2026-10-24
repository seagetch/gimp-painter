#!/usr/bin/env python3
import json,hashlib,tarfile
from pathlib import Path
p=Path(__file__).resolve().parent
def sha(b):return hashlib.sha256(b).hexdigest()
a=json.loads((p/'artifact-sha256.json').read_text());assert set(a)=={x.name for x in p.iterdir() if x.is_file() and x.name!='artifact-sha256.json'}
for x,h in a.items():assert sha((p/x).read_bytes())==h,x
j=json.loads((p/'results.json').read_text());assert j['changed_after_run']==[] and set(j['runs'])=={'normal','sanitizer'}
for v in j['runs'].values():
 assert v['exit_code']==0 and not v['stderr']
 assert 'Red Eye:117440512 rational raw comparisons;60 vector/spill ROI scenes;4 unsafe thresholds;' in v['stdout']
 assert 'Value Invert Every read/write/flush' in v['stdout']
assert any('float-cast-overflow' in x for x in j['runs']['sanitizer']['compile'])
with tarfile.open(p/'sources.tar.gz') as t:
 names=t.getnames();assert len(names)==len(set(names)) and set(names)==set(j['source_sha256'])
 for name in names:assert not Path(name).is_absolute() and '..' not in Path(name).parts and sha(t.extractfile(name).read())==j['source_sha256'][name]
for x,h in json.loads((p/'owned-source-sha256.json').read_text()).items():assert j['source_sha256'][x]==h
print('PASS normal+san raw117440512/60 ROI;full point regressions;owned/source seals;no native owner route claim')
