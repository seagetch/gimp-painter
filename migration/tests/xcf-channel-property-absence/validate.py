#!/usr/bin/env python3
import hashlib,json,pathlib,re,tarfile
P=pathlib.Path(__file__).resolve().parent
def check(p=P):
 load=lambda f:json.loads((p/f).read_text());r=load('report.json');rows=load('related-exact.json');red=load('expected-red.json');b=load('sanitizer-build.json')
 assert r['status']=='DOING' and not r['full_current99_case_run'] and r['presence_subsets']==16 and r['rounds_per_subset']==2
 assert red['returncode']==-6 and '0.368627459 == 0.368627451' in red['stdout']+red['stderr']
 assert len(rows)==6 and {(x['sanitized'],x['name'])for x in rows}=={(s,n)for s in (False,True)for n in r['related_groups']}
 for x in rows:
  t=x['stdout'];assert x['exit_code']==0 and x['duration_seconds']<60 and '1..1'in t.splitlines() and sum(v.startswith('ok ')for v in t.splitlines())==1
  assert not re.search(r'(?im)^not ok |#\s*(skip|todo)\b',t)
  assert not re.search(r'AddressSanitizer:|runtime error:',x['stderr'])
  if x['name']=='channel_property_absence':
   for mask in range(16):
    for repeat in range(2):assert f'channel presence mask{mask} round{repeat} alpha' in t
 assert not b['changed_during_build'] and len(b['instrumented_sources'])==41 and len(b['rtti_compatibility_only_sources'])==40
 assert load('source-contract.json')['result']=='PASS' and len(load('source-contract.json')['negative_controls'])==55
 assert '1..0'in load('excluded-zero-prefix.json')['stdout'].splitlines()
 with tarfile.open(p/'owned-tested-source.tar.gz')as t:
  for f,h in load('owned-source-sha256.json').items():assert hashlib.sha256(t.extractfile(f).read()).hexdigest()==h
  code=t.extractfile('app/xcf/xcf-load.c').read().decode();assert code.count('opacity = current[3];')==2
if __name__=='__main__':
 check()
 for f,h in json.loads((P/'artifact-sha256.json').read_text()).items():assert hashlib.sha256((P/f).read_bytes()).hexdigest()==h,f
 print('PASS16presence subsets×2rounds,3exact normal+3focused sanitizer channel groups; genuine alpha red retained')
