#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');args=p.parse_args();here=pathlib.Path(__file__).resolve().parent;root=here.parents[2]
def no_env(value):
 if isinstance(value,dict):
  assert 'env' not in value
  for x in value.values():no_env(x)
 elif isinstance(value,list):
  for x in value:no_env(x)
for path in here.glob('*.json'):no_env(json.loads(path.read_text()))
report=json.loads((here/'report.json').read_text())
for path,sha in report['evidence_sha256'].items():assert hashlib.sha256((here/path).read_bytes()).hexdigest()==sha,path
native=json.loads((here/'registered-native.json').read_text());counts={'painter-mypaint-native-precision':8,'painter-mypaint-coordinate-boundaries':7,'gimp-painter-surface':12,'painter-roi-transfers':3}
assert len(native)==4
for row in native:
 name=row['name'].split(' / ')[-1];assert row['result']=='OK' and row['returncode']==0 and row['duration']<60
 assert len(re.findall(r'^ok \d+ /',row['stdout'],re.M))==counts[name]
 assert '# SKIP' not in row['stdout']
single=json.loads((here/'standalone.json').read_text());assert len(single['results'])==2
for row in single['results']:assert row['exit_code']==0 and row['passed']==7 and row['duration']<60
if args.check_source:
 for path,sha in report['source_sha256'].items():assert hashlib.sha256((root/path).read_bytes()).hexdigest()==sha,path
print('Native30TAP and standalone coordinate7+7 verified; raw Meson environments absent')
