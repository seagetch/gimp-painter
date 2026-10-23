#!/usr/bin/env python3
import hashlib,json,pathlib,tarfile,re,argparse,struct
P=pathlib.Path(__file__).resolve().parent

def check(p=P,check_source=False):
 load=lambda n:json.loads((p/n).read_text());r=load('report.json');n=load('native.json');v=load('visual.json');s=load('sanitizer.json');b=load('sanitizer-build.json');m=load('mutation-results.json')
 assert r['task']=='09.013' and r['native_tap_count']==47 and r['sanitizer_tap_count']==1
 assert len(n)==2 and all(x['result']=='OK' and x['returncode']==0 for x in n)
 assert sorted(sum(y.startswith('ok ')for y in x['stdout'].splitlines())for x in n)==[22,25]
 assert len(v)==1 and v[0]['result']=='OK' and v[0]['returncode']==s['exit_code']==0
 for t in (v[0]['stdout'],s['stdout']):
  assert sum(x.startswith('ok ')for x in t.splitlines())==1 and '1..1' in t.splitlines()
  assert '/painter-tool/projection/visible-update' in t and not re.search(r'(?im)^not ok |#\s*(skip|todo)\b',t)
  for name,rgb in [('before','0,0,255'),('live','255,0,0'),('canceled','0,0,255'),('committed','255,0,0')]:
   assert re.search(r'RAW_CANVAS projection-'+name+r'\.png portrait=1080x1920 oncanvas=1 sample=\d+,\d+ rgb='+rgb,t)
 assert not re.search(r'AddressSanitizer:|runtime error:',s['stderr'])
 assert len(b['instrumented_sources'])==15 and not b['vptr_sanitizer'] and not b['leak_detection'] and not b['changed_during_build'] and not b['dependencies_instrumented']
 assert not r['whole_application'] and not r['whole_graph_consumer_coverage']
 assert len(m)==3 and {x['mutation']for x in m}=={'drop-surface-update','drop-projection-display-update','drop-cancel-display-flush'}
 for x in m:
  assert x['compile_exit_code']==0 and x['result']=='FAIL'
  t=x['stdout'];assert 'projection-before.png portrait=1080x1920 oncanvas=1 sample=426,849 rgb=0,0,255' in t # target fault, not failed setup
  if x['mutation']=='drop-cancel-display-flush':
   assert 'canceled==baseline' in t and 'projection-canceled.png portrait=1080x1920 oncanvas=1 sample=426,849 rgb=255,0,0' in t
  else:assert 'live[0] > live[2]+100' in t
 red=load('expected-red-cancel-stale.json');assert red['result']=='FAIL' and red['returncode']!=0 and 'canceled==baseline' in red['stdout']
 assert r['source_sha256']==b['sources_sha256']
 for archive,key in [('tested-sources.tar.gz','source_sha256'),('causal-red-sources.tar.gz','causal_red_source_sha256')]:
  with tarfile.open(p/archive)as t:
   assert set(t.getnames())==set(r[key])
   for f,h in r[key].items():
    assert hashlib.sha256(t.extractfile(f).read()).hexdigest()==h
    if check_source and key=='source_sha256':assert hashlib.sha256((P.parents[2]/f).read_bytes()).hexdigest()==h,f
 for name in ['projection-before.png','projection-live.png','projection-canceled.png','projection-committed.png']:
  data=(p/name).read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',data[16:24])==(1080,1920)
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--check-source',action='store_true');args=parser.parse_args();check(check_source=args.check_source)
 for f,h in json.loads((P/'artifact-sha256.json').read_text()).items():assert hashlib.sha256((P/f).read_bytes()).hexdigest()==h,f
 print('PASS native47 + selected15-unit sanitizer UI1;portrait/oncanvas4 captures;3 target-specific native mutants')
