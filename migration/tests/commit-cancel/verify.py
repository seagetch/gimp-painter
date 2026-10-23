#!/usr/bin/env python3
import hashlib,json,pathlib,tarfile,re,argparse
P=pathlib.Path(__file__).resolve().parent

def check(p=P,check_source=False):
 load=lambda n:json.loads((p/n).read_text());r=load('report.json');n=load('native.json');s=load('sanitizer.json');b=load('sanitizer-build.json');m=load('mutation-results.json')
 assert r['task']=='09.012' and r['native_tap_count']==r['sanitizer_tap_count']==22 and r['precision_mode_exit_cases']==48
 assert len(n)==1 and n[0]['result']=='OK' and n[0]['returncode']==s['exit_code']==0 and s['duration_seconds']<60
 for t in (n[0]['stdout'],s['stdout']):
  assert sum(x.startswith('ok ')for x in t.splitlines())==22 and '1..22' in t.splitlines()
  assert '/commit-cancel/native-pixels-and-history' in t and not re.search(r'(?im)^not ok |#\s*(skip|todo)\b',t)
  cases=re.findall(r'^# precision=(\d+) floating=(\d) exit=(\d)$',t,re.M)
  assert len(cases)==len(set(cases))==48
 assert not re.search(r'AddressSanitizer:|runtime error:',s['stderr'])
 assert len(b['instrumented_sources'])==11 and not b['vptr_sanitizer'] and not b['leak_detection'] and not b['changed_during_build'] and not b['dependencies_instrumented']
 assert not r['whole_application'] and not r['engine_stroke_state_verified'] and not r['rendered_UI_evidence']
 assert len(m)==3 and {x['mutation']for x in m}=={'cancel-keeps-pixels','commit-rolls-back','external-finish-commits-batch'}
 for x in m:
  assert x['compile_exit_code']==0 and x['result']=='FAIL' and 'snapshot_bytes(target,extent)==' in x['stdout'] and 'should be TRUE' in x['stdout']
  assert 'before!=redo_pixels' not in x['stdout'] # final mutation must reach target
 assert r['source_sha256']==b['sources_sha256']
 with tarfile.open(p/'tested-sources.tar.gz')as t:
  assert set(t.getnames())==set(r['source_sha256'])
  for f,h in r['source_sha256'].items():
   assert hashlib.sha256(t.extractfile(f).read()).hexdigest()==h
   if check_source:assert hashlib.sha256((P.parents[2]/f).read_bytes()).hexdigest()==h,f
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--check-source',action='store_true');args=parser.parse_args();check(check_source=args.check_source)
 for f,h in json.loads((P/'artifact-sha256.json').read_text()).items():assert hashlib.sha256((P/f).read_bytes()).hexdigest()==h,f
 print('PASS native22+selected sanitizer22,48 exit cases,three target-specific native mutants')
