#!/usr/bin/env python3
import hashlib,json,pathlib,tarfile,re
P=pathlib.Path(__file__).resolve().parent
def check(p=P):
 load=lambda n:json.loads((p/n).read_text());r=load('report.json');n=load('native.json');s=load('sanitizer.json');b=load('sanitizer-build.json');m=load('mutation-results.json')
 assert r['task']=='09.011' and r['native_tap_count']==r['sanitizer_tap_count']==21
 assert len(n)==1 and n[0]['result']=='OK' and n[0]['returncode']==s['exit_code']==0 and s['duration_seconds']<60
 for t in (n[0]['stdout'],s['stdout']):
  assert sum(x.startswith('ok ')for x in t.splitlines())==21 and '1..21' in t.splitlines()
  assert '/undo/native-operation-history' in t and not re.search(r'(?im)^not ok |#\s*(skip|todo)\b',t)
 assert not re.search(r'AddressSanitizer:|runtime error:',s['stderr'])
 assert len(b['instrumented_sources'])==11 and not b['vptr_sanitizer'] and not b['leak_detection'] and not b['changed_during_build']
 assert not r['whole_application'] and not r['engine_stroke_state_verified']
 assert len(m)==2 and {x['mutation']for x in m}=={'drop-undo-record','ignore-no-undo-request'}
 for x in m:assert x['compile_exit_code']==0 and x['result']=='FAIL' and 'gimp_undo_stack_get_depth' in x['stdout'] and 'assertion failed' in x['stdout']
 with tarfile.open(p/'tested-sources.tar.gz')as t:
  for f,h in r['source_sha256'].items():assert hashlib.sha256(t.extractfile(f).read()).hexdigest()==h
if __name__=='__main__':
 check()
 for f,h in json.loads((P/'artifact-sha256.json').read_text()).items():assert hashlib.sha256((P/f).read_bytes()).hexdigest()==h,f
 print('PASS native21+focused sanitizer21, six histories, two actual Undo request mutants')
