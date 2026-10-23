#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');args=p.parse_args()
here=pathlib.Path(__file__).resolve().parent;root=here.parents[2]
audit=json.loads((here/'duty-audit.json').read_text());original=json.loads((root/'migration/contracts/drawable-buffer-read.json').read_text())
assert audit['parent_complete'] is False
assert len(audit['duties'])==48
rows={d['work_id']:d for d in audit['duties']};assert len(rows)==48
for d in original['duties']:
 r=rows[d['work_id']]
 for key in ('source_id','path','source_blob','source_range','source_sha256','remaining_tasks'):assert r[key]==d[key],key
 assert r['remaining_semantics'] and r['verification'] and '09.003' in r['remaining_tasks']
def no_env(value):
 if isinstance(value,dict):
  assert 'env' not in value
  for v in value.values():no_env(v)
 elif isinstance(value,list):
  for v in value:no_env(v)
for path in here.glob('*.json'):no_env(json.loads(path.read_text()))
fill=json.loads((here/'fill-normal-sanitizers.json').read_text())
assert len(fill['results'])==2
for r in fill['results']:assert r['exit_code']==0 and r['passed']==7 and r['duration']<60
report=json.loads((here/'report.json').read_text())
for path,sha in report['evidence_sha256'].items():assert hashlib.sha256((here/path).read_bytes()).hexdigest()==sha,path
native=json.loads((here/'registered-native.json').read_text())
assert len(native)==2
for r in native:
 assert r['result']=='OK' and r['returncode']==0
 count=len(re.findall(r'^ok \d+ /',r['stdout'],re.M))
 assert count==(12 if 'gimp-painter-surface' in r['name'] else 7)
 assert '# SKIP' not in r['stdout']
if args.check_source:
 for path,sha in report['source_sha256'].items():assert hashlib.sha256((root/path).read_bytes()).hexdigest()==sha,path
print('48 retained obligations; native 12+7 and standalone normal/sanitized 7+7 verified')
