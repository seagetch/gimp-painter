#!/usr/bin/env python3
import hashlib,json,pathlib,re,tarfile
P=pathlib.Path(__file__).resolve().parent
def check(p=P):
 load=lambda n:json.loads((p/n).read_text())
 r=load('report.json');n=load('normal.json');s=load('sanitizers.json')
 assert r['status']=='DOING' and not r['production_changed']
 assert r['cases']=={'normal':98,'sanitizer':98} and r['declared_length_scenes']==40
 expected={'xcf':4,'painter-xcf-open':31,'painter-xcf-roundtrip':51,'painter-provenance':12}
 def tap(t,k):
  assert '1..'+str(k) in t.splitlines()
  assert sum(x.startswith('ok ')for x in t.splitlines())==k
  assert not re.search(r'(?im)^not ok |#\s*(skip|todo)\b',t)
 def scenes(t):
  for prop in (6,33,16,38):
   for size in (0,1,2,3,4,5,11,12,13,4294967295):
    assert f'channel length property{prop} declared{size} ' in t
 for v in n:
  name=v['name'].rsplit(' / ',1)[-1]
  assert v['result']=='OK' and v['returncode']==0
  tap(v['stdout'],expected[name])
  if name=='painter-xcf-roundtrip':scenes(v['stdout'])
 assert not s['changed_during_build'] and not s['changed_during_run']
 for name,v in s['results'].items():
  assert v['exit_code']==0 and v['duration_seconds']<v['timeout_seconds']
  tap(v['stdout'],expected[name])
  assert not re.search(r'AddressSanitizer:|runtime error:',v['stderr'])
  if name=='painter-xcf-roundtrip':scenes(v['stdout'])
 assert len(s['instrumented_sources'])==41 and len(s['rtti_compatibility_only_sources'])==40
 with tarfile.open(p/'owned-tested-source.tar.gz')as archive:
  for name,h in load('owned-source-sha256.json').items():
   assert hashlib.sha256(archive.extractfile(name).read()).hexdigest()==h
if __name__=='__main__':
 check()
 for name,h in json.loads((P/'artifact-sha256.json').read_text()).items():
  assert hashlib.sha256((P/name).read_bytes()).hexdigest()==h,name
 print('PASS40 declared-length compatibility scenes,98native+98focused sanitizer; parent DOING')
