#!/usr/bin/env python3
import hashlib,json,pathlib,re,tarfile
P=pathlib.Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def check(p=P):
 load=lambda n:json.loads((p/n).read_text());r=load('report.json');n=load('normal.json');s=load('sanitizers.json')
 assert r['status']=='DOING' and r['cases']=={'normal':97,'sanitizer':97} and r['valid_property_permutations']==6
 red=load('expected-red.json');assert len(red)==1 and red[0]['returncode']==-6 and '0.270000219' in red[0]['stdout']+red[0]['stderr']
 expected={'xcf':4,'painter-xcf-open':31,'painter-xcf-roundtrip':50,'painter-provenance':12}
 def tap(t,k):
  assert '1..'+str(k) in t.splitlines();assert sum(x.startswith('ok ') for x in t.splitlines())==k
  assert not re.search(r'(?im)^not ok |#\s*(skip|todo)\b',t)
 for v in n:
  name=v['name'].rsplit(' / ',1)[-1];assert v['result']=='OK' and v['returncode']==0;tap(v['stdout'],expected[name])
 assert not s['changed_during_build'] and not s['changed_during_run']
 for name,v in s['results'].items():
  assert v['exit_code']==0 and v['duration_seconds']<v['timeout_seconds'];tap(v['stdout'],expected[name]);assert not re.search(r'AddressSanitizer:|runtime error:',v['stderr'])
  if name=='painter-xcf-roundtrip':
   for i in range(6):
    for j in range(2):assert f'property order{i} round{j} alpha' in v['stdout']
 assert len(s['instrumented_sources'])==41 and len(s['rtti_compatibility_only_sources'])==40
 with tarfile.open(p/'owned-tested-source.tar.gz') as archive:
  for name,h in load('owned-source-sha256.json').items():assert hashlib.sha256(archive.extractfile(name).read()).hexdigest()==h
  code=archive.extractfile('app/xcf/xcf-load.c').read().decode();body=code[code.index('xcf_load_channel_props (XcfInfo',code.index('xcf_load_channel_props (XcfInfo')+1):]
  part=body[body.index('case PROP_OPACITY:'):body.index('case PROP_FLOAT_OPACITY:')]
  assert 'gimp_channel_set_opacity' not in part and 'gegl_color_get_pixel' in part and 'rgba[3] = CLAMP (opacity / 255.0' in part and 'gegl_color_set_pixel' in part
 gate=load('source-contract.json');assert gate['result']=='PASS' and len(gate['negative_controls'])==46
 assert r['original185_row_ledger_unchanged'] and not r['whole_app_instrumented'] and not r['lsan']
if __name__=='__main__':
 check()
 for name,h in json.loads((P/'artifact-sha256.json').read_text()).items():assert sha(P/name)==h,name
 print('PASS channel RGB/alpha ordering: genuine red,6orders×2rounds,97normal+97focused sanitizer; parent remains DOING')
