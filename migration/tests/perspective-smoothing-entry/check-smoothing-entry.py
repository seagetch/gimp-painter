#!/usr/bin/env python3
import csv,hashlib,json,pathlib,re,tarfile,importlib.util
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def module(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def check(p=P):
 for n,h in json.loads((p/'manifest.json').read_text()).items():assert sha((p/n).read_bytes())==h,n
 audit=json.loads((p/'audit.json').read_text());assert audit['status']=='OPEN' and audit['closed_original_duties']==0 and len(audit['rows'])==260
 assert {t:sum(r['wbs_task']==t for r in audit['rows']) for t in audit['tasks']}=={'26.013':48,'26.014':212}
 rows=list(csv.DictReader((R/'migration/inventory/legacy-port-work-items.tsv').open(),delimiter='\t'));lookup={r['work_id']:r for r in rows}
 for r in audit['rows']:
  assert all(lookup[r['work_id']][k]==v for k,v in r.items() if k not in ['payload','other_whole_hunk_owners'])
  assert sha((p/r['payload']).read_bytes())==r['source_sha256']
  assert {(x['work_id'],x['wbs_task']) for x in rows if x['source_id']==r['source_id'] and x['work_id']!=r['work_id']}=={(x['work_id'],x['task']) for x in r['other_whole_hunk_owners']}
 ordering=json.loads((p/'ordering-sources.json').read_text());assert ordering['revision']=='afa43fae3e920210146abed514f136fd49f671b5'
 pins={'app/tools/gimppainttool.c': 'e8a79052efe0fc0fb9306c4f226ee4f43ccf58d7', 'app/display/gimpdisplayshell-tool-events.c': 'b670161eb5906349f84a51da961ae339b7209574', 'app/display/gimpmotionbuffer.c': '1e2d4c424cde39afa9065e65ec82d7f1b8ee66f2', 'app/paint/gimppaintoptions.c': '60025a2d4838a20414f5f9890e53a8870f18c175', 'app/paint/gimppaintoptions.h': '0cb042630b3b2f5b1e456a3624973819ec6c4c47'}
 assert {r['path']:r['blob'] for r in ordering['sources']}==pins
 for r in ordering['sources']:
  raw=(p/'ordering-source'/r['payload']).read_bytes();assert sha(raw)==r['sha256'];assert hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()==r['blob']
 seal=json.loads((p/'final/source-before-build.json').read_text());assert len(seal)==164
 with tarfile.open(p/'final/tested-sources.tar.gz','r:gz') as t:
  assert set(t.getnames())==set(seal);source={n:t.extractfile(n).read() for n in seal}
  for n,h in seal.items():assert sha(source[n])==h,n
 prior=R/'migration/tests/perspective-lifecycle-entry/final'
 with tarfile.open(prior/'tested-sources.tar.gz','r:gz') as t:old=t.extractfile('app/tests/test-painter-perspective-lifecycle-events.c').read()
 adm=module('successor',p/'admit-lifecycle-successor.py');assert adm.check(old,source['app/tests/test-painter-perspective-lifecycle-events.c'],p)
 priorseal=json.loads((prior/'source-before-build.json').read_text())
 for n,h in priorseal.items():
  if n!='app/tests/test-painter-perspective-lifecycle-events.c':assert seal[n]==h,n
 for n in ['app/tests/test-painter-perspective-lifecycle-events.c','app/tests/test-painter-perspective-smoothing-events.inc']:
  assert sha((R/n).read_bytes())==seal[n],n
 text=source['app/tests/test-painter-perspective-smoothing-events.inc'].decode()
 for token in ['smoothing_real_motion (tool, coords, time, state, display);','smoothing_real_interpolate (core, drawables, options, time);','tool_class->motion=smoothing_real_motion;core_class->interpolate=smoothing_real_interpolate;','g_assert_cmpfloat(best,<=,64)','g_assert_cmpuint(strokes,==,72)','if(smooth)g_assert_cmpuint(smoothing_changed,>,0);else g_assert_cmpuint(smoothing_changed,==,0);']:
  assert token in text,token
 held=json.loads((p/'final/held-source-isolation.json').read_text())['rows'];assert len(held)==9
 for row in held:
  if row.get('native_exists') is False:assert row['path'] not in seal
  else:assert seal[row['path']]==row['sha256']
 fixture=R/'migration/fixtures/legacy-perspective-smoothing'
 for f in fixture.iterdir():
  if f.is_file():assert sha(f.read_bytes())==seal[str(f.relative_to(R))]
 module('oldsmooth',fixture/'verify-source.py').check()
 original={r['name'].split(' / ')[-1]:{c.split(' ',2)[2] for c in r['cases']} for r in json.loads((prior/'build.json').read_text())}
 original['painter-perspective-lifecycle-events'].add('/perspective-correction/paintbrush-smoothing-order')
 for b in ['build','build-asan']:
  results=json.loads((p/f'final/{b}.json').read_text());assert len(results)==3;seen=set()
  for j in results:
   name=j['name'].split(' / ')[-1];seen.add(name)
   assert j['result']=='OK' and j['returncode']==0 and j['duration']<60 and j['timeout_seconds']==60
   assert j['source_sha256']==seal and re.fullmatch('[a-f0-9]{64}',j['executable_sha256'])
   actual=[l for l in j['stdout'].splitlines() if l.startswith('ok ') and '# SKIP' not in l];assert actual==j['cases']
   assert {l.split(' ',2)[2] for l in actual}==original[name] and len(actual)==len(original[name])
   if name=='painter-perspective-lifecycle-events':
    for claim in ['72 three-point transformed smoothing-off/on strokes, 372 constrained-before-Gaussian samples, 150 smoothed samples, 2351 alpha pixels','brushmask bytes=81 sum=20655','first stroke before release maxalpha=255','writer comparison first/control=1 control/without-guide=1']:assert claim in j['stdout']
   assert j['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True)
  assert seen==set(original)
  assert '1080x1920x24, authenticated accepted, empty authority rejected' in (p/f'final/{b}.log').read_text()
  png=(p/f'final/{b}-images/paintbrush-smoothed-three-directions.png').read_bytes()
  assert png[:8]==b'\x89PNG\r\n\x1a\n' and tuple(int.from_bytes(png[i:i+4],'big') for i in [16,20])==(1040,1800)
 for kind in ['identity-smoothing','wrong-angle','shifted-raster-envelope']:
  d=p/'negative'/kind;m=json.loads((d/'mutation.json').read_text());assert sha((d/'original-input.txt').read_bytes())==m['original_sha256']==seal[m['path']]
  assert sha((d/'mutated-input.txt').read_bytes())==m['mutated_sha256']!=m['original_sha256']
  j=json.loads((d/'result.json').read_text());assert j['result']=='FAIL' and j['returncode']!=0 and ('assertion failed' in j['stdout'] or 'should be' in j['stdout'])
 assert 'brushmask bytes=289 sum=0' in (p/'diagnostics/diagnostic-7/run.log').read_text()
 assert 'prepress snap=1' in (p/'diagnostics/diagnostic-3/run.log').read_text()
 print('PASS: original13+stroke2+lifecycle3+smoothing1 normal/sanitizer;72 real Paintbrush strokes;260 duties OPEN;9 held;3 causal controls')
if __name__=='__main__':check()
