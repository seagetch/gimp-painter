#!/usr/bin/env python3
import csv,hashlib,json,pathlib,re,tarfile,importlib.util,struct
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def tattoo(b):
 assert b[:14]==b'gimp xcf v011\0';offset=30
 while offset+8<=len(b):
  kind,n=struct.unpack_from('>II',b,offset);assert n<=len(b)-offset-8
  if kind==20:assert n==4;return offset+8,struct.unpack_from('>I',b,offset+8)[0]
  assert kind!=0;offset+=8+n
 raise AssertionError('missing tattoo')
def check(p=P):
 for n,h in json.loads((p/'manifest.json').read_text()).items():assert sha((p/n).read_bytes())==h,n
 audit=json.loads((p/'audit.json').read_text());assert audit['status']=='OPEN' and audit['closed_original_duties']==0 and len(audit['rows'])==45
 rows=list(csv.DictReader((R/'migration/inventory/legacy-port-work-items.tsv').open(),delimiter='\t'));lookup={r['work_id']:r for r in rows}
 for r in audit['rows']:
  assert all(lookup[r['work_id']][k]==v for k,v in r.items() if k not in ['payload','other_whole_hunk_owners'])
  assert sha((p/r['payload']).read_bytes())==r['source_sha256']
  assert {(x['work_id'],x['wbs_task']) for x in rows if x['source_id']==r['source_id'] and x['work_id']!=r['work_id']}=={(x['work_id'],x['task']) for x in r['other_whole_hunk_owners']}
 seal=json.loads((p/'final/source-before-build.json').read_text());assert len(seal)==146
 with tarfile.open(p/'final/tested-sources.tar.gz','r:gz') as t:
  assert set(t.getnames())==set(seal);source={n:t.extractfile(n).read() for n in seal}
  for n,h in seal.items():assert sha(source[n])==h,n
 prior=json.loads((R/'migration/tests/perspective-stroke-entry/final/source-before-build.json').read_text())
 for n,h in prior.items():
  if n!='app/tests/meson.build':assert seal[n]==h,n
 current=(R/'app/tests/test-painter-perspective-lifecycle-events.c').read_bytes()
 if sha(current)!=seal['app/tests/test-painter-perspective-lifecycle-events.c']:
  # Historical runtime stays pinned; only this exact additive tested successor is admitted.
  spec=importlib.util.spec_from_file_location('lifecycle_successor',R/'migration/tests/perspective-smoothing-entry/admit-lifecycle-successor.py');successor=importlib.util.module_from_spec(spec);spec.loader.exec_module(successor)
  assert successor.check(source['app/tests/test-painter-perspective-lifecycle-events.c'],current)
 text=source['app/tests/test-painter-perspective-lifecycle-events.c'].decode();assert '#define main frozen_lifecycle_editor_main' in text and text.count('g_test_add_func (')==3
 meson=source['app/tests/meson.build'].decode();assert meson.count("  'painter-perspective-lifecycle-events',")==1
 assert 'GIMP_DIRTY_NONE' in source['app/core/gimpperspectiveguideundo.cpp'].decode()
 held=json.loads((p/'final/held-source-isolation.json').read_text())['rows'];assert len(held)==9
 for row in held:
  if row.get('native_exists') is False:assert row['path'] not in seal
  else:assert seal[row['path']]==row['sha256']
 fixture=R/'migration/fixtures/legacy-perspective-persistence'
 for f in fixture.iterdir():
  if f.is_file():assert sha(f.read_bytes())==seal[str(f.relative_to(R))]
 spec=importlib.util.spec_from_file_location('oldpersist',fixture/'verify-source.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.check()
 original={r['name'].split(' / ')[-1]:{c.split(' ',2)[2] for c in r['cases']} for r in json.loads((R/'migration/tests/perspective-stroke-entry/final/build.json').read_text())}
 original['painter-perspective-lifecycle-events']={'/perspective-lifecycle/native-undo-all-edits','/perspective-lifecycle/xcf-nonpersistent','/perspective-lifecycle/switch-close'}
 for b in ['build','build-asan']:
  results=json.loads((p/f'final/{b}.json').read_text());assert len(results)==3;seen=set()
  for j in results:
   name=j['name'].split(' / ')[-1];seen.add(name)
   assert j['result']=='OK' and j['returncode']==0 and j['duration']<60 and j['timeout_seconds']==60
   assert j['source_sha256']==seal and re.fullmatch('[a-f0-9]{64}',j['executable_sha256'])
   actual=[l for l in j['stdout'].splitlines() if l.startswith('ok ') and '# SKIP' not in l];assert actual==j['cases']
   assert {l.split(' ',2)[2] for l in actual}==original[name] and len(actual)==len(original[name])
   if name=='painter-perspective-lifecycle-events':
    for claim in ['writer comparison first/control=1 control/without-guide=1','tattoo counter around image duplicate: 2 -> 3','no-op max3 adds no Undo','close active editor clears control/draw/display/status refs']:assert claim in j['stdout']
   assert j['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True)
  assert seen==set(original)
  assert '1080x1920x24, authenticated accepted, empty authority rejected' in (p/f'final/{b}.log').read_text()
  d=p/f'final/{b}-images';streams=[(d/n).read_bytes() for n in ['with-guide-first.xcf','with-guide-control.xcf','without-guide.xcf']];assert streams[0]==streams[1]==streams[2]
  for label in ['before-delete-three-points','after-delete-two-points','undo-restored-three-points']:
   png=(d/(label+'.png')).read_bytes();assert png[:8]==b'\x89PNG\r\n\x1a\n';assert tuple(int.from_bytes(png[i:i+4],'big') for i in [16,20])==(1040,1800)
 d=p/'diagnostic-tattoo';a,b,c=[(d/n).read_bytes() for n in ['with-guide-first.xcf','with-guide-control.xcf','without-guide.xcf']]
 assert len(a)==len(b)==len(c)==1274 and b==c and [i for i in range(len(a)) if a[i]!=b[i]]==[66]
 assert tattoo(a)==(63,2) and tattoo(b)==(63,3)
 for kind in ['skipped-undo','extra-persistent-guide','missing-image-switch']:
  d=p/'negative'/kind;m=json.loads((d/'mutation.json').read_text());assert sha((d/'original-input.txt').read_bytes())==m['original_sha256']==seal[m['path']];assert sha((d/'mutated-input.txt').read_bytes())==m['mutated_sha256']!=m['original_sha256']
  j=json.loads((d/'result.json').read_text());assert j['result']=='FAIL' and j['returncode']!=0 and ('assertion failed' in j['stdout'] or 'should be' in j['stdout'])
 print('PASS: original13+stroke2+lifecycle3 normal/sanitizer; byte-exact guide non-persistence;45 duties open;9 held;3 causal controls')
if __name__=='__main__':check()
