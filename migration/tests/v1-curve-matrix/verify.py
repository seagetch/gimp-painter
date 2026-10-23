#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re,tarfile

def validate(e,check_owned=False):
 read=lambda n:json.loads((e/n).read_text())
 for n,h in read('artifact-sha256.json').items():assert hashlib.sha256((e/n).read_bytes()).hexdigest()==h,n
 r=read('report.json');assert not r['production_changed'] and not r['old_runtime_executed'] and r['prerequisites_unfinished']
 assert (r['normal_cases'],r['selected_sanitizer_cases'],r['instrumented_units'],r['new_v1_curves'],r['malformed_point_lists'])==(10,10,3,405,14)
 for name in ['normal','asan']:
  row=read(name+'.json');assert row['returncode']==0
  assert len(re.findall(r'^ok \d+',row['stdout'],re.M))==10
  assert 'v1 setting/input curves=405; transformed aliases=3; malformed point lists=14' in row['stdout']
  assert '177 pinned brushes loaded/configured/round-tripped' in row['stdout']
 m=read('mutants.json');assert {x['mutation']for x in m}=={'skip-points','omit-y-transform','allow-descending'}
 for x in m:assert x['returncode']!=0 and ('assertion' in x['stderr']or 'Bail out' in x['stdout'])
 before=read('before-source.json');assert read('post-source-seal.json')=={'changed':[],'checked':len(before)}
 with tarfile.open(e/'tested-sources.tar.gz')as t:
  for n,h in before.items():assert hashlib.sha256(t.extractfile(n).read()).hexdigest()==h,n
 if check_owned:
  root=pathlib.Path(__file__).resolve().parents[3];n='app/paint/painter-mypaint/tests/test-resource.cpp';assert hashlib.sha256((root/n).read_bytes()).hexdigest()==before[n]
 print('PASS: v1 405 curve pairs,3 transformed aliases,14 malformed cases; normal10/3-unit sanitizer10;3 causal mutants; dependencies remain open')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--check-owned-source',action='store_true');a=p.parse_args();validate(pathlib.Path(__file__).resolve().parent,a.check_owned_source)
