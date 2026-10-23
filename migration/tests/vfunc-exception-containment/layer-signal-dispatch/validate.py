#!/usr/bin/env python3
"""Verify the bounded dispatch fault matrix and retained source receipt."""
import argparse,hashlib,json,pathlib,re,tarfile
P=pathlib.Path(__file__).resolve().parent;ROOT=P.parents[3]
a=argparse.ArgumentParser();a.add_argument('--check-source',action='store_true');args=a.parse_args()
r=json.loads((P/'report.json').read_text());assert r['status']=='PASS_BOUNDED_DISPATCH'
expected={'painter-layer-dispatch':(4,60),'painter-layer-ui':(70,180),'painter-canvas-ui':(107,60)}
assert len(r['sanitizers'])==3 and {t['name'] for t in r['sanitizers']}==set(expected)
normal=json.loads((P/'normal-tests.json').read_text())
assert len(normal)==3 and {t['name'].split(' / ')[-1] for t in normal}==set(expected)
for test in normal:
 name=test['name'].split(' / ')[-1];assert test['result']=='OK' and test['returncode']==0
 assert len(re.findall(r'^ok \d+ ',test['stdout'],re.M))==expected[name][0]
 assert not re.search(r'^not ok|^ok .*#\s*SKIP',test['stdout'],re.M|re.I)
 if 'timeout' in test:assert test['timeout']==expected[name][1]
 assert 'env' not in test
for test in r['sanitizers']:
 count,budget=expected[test['name']];assert test['returncode']==0 and test['passed']==count and test['timeout']==budget
 log=(P/('sanitizer-'+test['name']+'.log')).read_text();assert len(re.findall(r'^ok \d+ ',log,re.M))==count
 assert not re.search(r'^not ok|ERROR: AddressSanitizer|runtime error:',log,re.M)
 assert not re.search(r'^ok .*#\s*SKIP',log,re.M|re.I)
with tarfile.open(P/'tested-source-snapshot.tar.gz') as archive:
 members=archive.getmembers()
 assert len(members)==len(r['source_sha256']) and {m.name for m in members}==set(r['source_sha256'])
 assert all(m.isfile() and not pathlib.PurePosixPath(m.name).is_absolute() and '..' not in pathlib.PurePosixPath(m.name).parts for m in members)
 for path,digest in r['source_sha256'].items():
  assert hashlib.sha256(archive.extractfile(path).read()).hexdigest()==digest
  if args.check_source:assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest
if args.check_source:
 for path,digest in r['sanitizer_recipe_sha256'].items():assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest
red=json.loads((P/'causal-baseline/results.json').read_text());assert [x['kind'] for x in red]==['kind1','kind2','kind3','kind4'];assert [x['returncode']for x in red]==[0,0,0,-6]
assert r['leak_detection'] is False
manifest=json.loads((P/'artifact-sha256.json').read_text())
assert set(manifest)=={str(p.relative_to(P)) for p in P.rglob('*') if p.is_file() and p.name!='artifact-sha256.json'}
for path,digest in manifest.items():assert hashlib.sha256((P/path).read_bytes()).hexdigest()==digest
print('PASS bounded dispatch4 + layer70 + canvas107; original111 duty remains open')
