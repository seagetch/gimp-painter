#!/usr/bin/env python3
import csv,hashlib,json,pathlib,re,tarfile,importlib.util
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def check(p=P):
 for n,h in json.loads((p/'manifest.json').read_text()).items():assert sha((p/n).read_bytes())==h,n
 audit=json.loads((p/'audit.json').read_text());assert audit['status']=='OPEN' and audit['closed_original_duties']==0 and len(audit['rows'])==87
 rows=list(csv.DictReader((R/'migration/inventory/legacy-port-work-items.tsv').open(),delimiter='\t'));lookup={r['work_id']:r for r in rows}
 for r in audit['rows']:
  assert all(lookup[r['work_id']][k]==v for k,v in r.items() if k not in ['payload','other_whole_hunk_owners'])
  assert sha((p/r['payload']).read_bytes())==r['source_sha256']
  assert {(x['work_id'],x['wbs_task']) for x in rows if x['source_id']==r['source_id'] and x['work_id']!=r['work_id']}=={(x['work_id'],x['task']) for x in r['other_whole_hunk_owners']}
 seal=json.loads((p/'final/source-before-build.json').read_text());assert len(seal)==104
 with tarfile.open(p/'final/tested-sources.tar.gz','r:gz') as t:
  assert set(t.getnames())==set(seal);source={n:t.extractfile(n).read() for n in seal}
  for n,h in seal.items():assert sha(source[n])==h,n
 prior=json.loads((R/'migration/tests/perspective-editor-entry/final/source-before-build.json').read_text())
 for n,h in prior.items():
  if n!='app/tests/meson.build':assert seal[n]==h,n
 assert sha((R/'app/tests/test-painter-perspective-stroke-events.c').read_bytes())==seal['app/tests/test-painter-perspective-stroke-events.c']
 text=source['app/tests/test-painter-perspective-stroke-events.c'].decode();assert '#define main frozen_editor_main' in text and '#include "test-painter-perspective-events.c"' in text
 assert text.count('g_test_add_func (')==2
 meson=source['app/tests/meson.build'].decode();assert meson.count("  'painter-perspective-stroke-events',")==1
 assert "test_timeout = test_name == 'gimp-filter-layer' ? 600 : 60" in meson
 held=json.loads((p/'final/held-source-isolation.json').read_text())['rows'];assert len(held)==9
 for row in held:
  if row.get('native_exists') is False:assert row['path'] not in seal
  else:assert seal[row['path']]==row['sha256']
 fixture=R/'migration/fixtures/legacy-perspective-stroke'
 for f in fixture.iterdir():
  if f.is_file():assert sha(f.read_bytes())==seal[str(f.relative_to(R))]
 spec=importlib.util.spec_from_file_location('oldstroke',fixture/'verify-source.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.check()
 old=json.loads((R/'migration/tests/perspective-editor-entry/final/build.json').read_text());original={l.split(' ',2)[2] for l in old['cases']};assert len(original)==13
 for b in ['build','build-asan']:
  results=json.loads((p/f'final/{b}.json').read_text());assert len(results)==2;seen=set()
  for j in results:
   name=j['name'].split(' / ')[-1];seen.add(name)
   assert j['result']=='OK' and j['returncode']==0 and j['duration']<60 and j['timeout_seconds']==60
   assert j['source_sha256']==seal and re.fullmatch('[a-f0-9]{64}',j['executable_sha256'])
   actual=[l for l in j['stdout'].splitlines() if l.startswith('ok ') and '# SKIP' not in l];assert actual==j['cases']
   names={l.split(' ',2)[2] for l in actual}
   if name=='painter-perspective-events':assert names==original and len(actual)==13
   else:
    assert name=='painter-perspective-stroke-events';assert names=={'/perspective-stroke/old-transformed-entry','/perspective-stroke/exact-threshold-tie-reset'} and len(actual)==2
    assert '216 transformed scenes, 432 independent consecutive strokes' in j['stdout']
    assert 'native exact32 threshold, first-horizontal45degree tie, dropped crossing sample, short next stroke reset' in j['stdout']
   assert j['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True)
  assert seen=={'painter-perspective-events','painter-perspective-stroke-events'}
  assert '1080x1920x24, authenticated accepted, empty authority rejected' in (p/f'final/{b}.log').read_text()
 for kind in ['threshold-31','last-tie','reversed-projection']:
  d=p/'negative'/kind;m=json.loads((d/'mutation.json').read_text());assert sha((d/'original-input.txt').read_bytes())==m['original_sha256']==seal[m['path']];assert sha((d/'mutated-input.txt').read_bytes())==m['mutated_sha256']!=m['original_sha256']
  j=json.loads((d/'result.json').read_text());assert j['result']=='FAIL' and j['returncode']!=0 and ('assertion failed' in j['stdout'] or 'should be FALSE' in j['stdout'])
 print('PASS: original13+new2 normal/sanitizer;216 poses/432 strokes;87 duties open;9 held;3 causal controls')
if __name__=='__main__':check()
