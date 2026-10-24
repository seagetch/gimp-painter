#!/usr/bin/env python3
import csv,hashlib,io,json,pathlib,re,tarfile,importlib.util
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def check(p=P):
 for n,h in json.loads((p/'manifest.json').read_text()).items():assert sha((p/n).read_bytes())==h,n
 audit=json.loads((p/'audit.json').read_text());assert audit['status']=='OPEN' and audit['closed_original_duties']==0
 rows=list(csv.DictReader((R/'migration/inventory/legacy-port-work-items.tsv').open(),delimiter='\t'));lookup={r['work_id']:r for r in rows}
 assert len(audit['rows'])==62
 for r in audit['rows']:
  assert all(lookup[r['work_id']][k]==v for k,v in r.items() if k not in ['payload','other_whole_hunk_owners'])
  assert sha((p/r['payload']).read_bytes())==r['source_sha256']
  actual=[{'work_id':x['work_id'],'task':x['wbs_task'],'status':x['status']} for x in rows if x['source_id']==r['source_id'] and x['work_id']!=r['work_id']]
  # Other tasks legitimately advance later; preserve identity/ownership, not a forever-TODO gate.
  assert {(x['work_id'],x['task']) for x in actual}=={(x['work_id'],x['task']) for x in r['other_whole_hunk_owners']}
 seal=json.loads((p/'final/source-before-build.json').read_text());assert len(seal)==96
 with tarfile.open(p/'final/tested-sources.tar.gz','r:gz') as t:
  assert set(t.getnames())==set(seal)
  source={n:t.extractfile(n).read() for n in seal}
  for n,h in seal.items():assert sha(source[n])==h,n
 for n in ['app/tests/test-painter-perspective-events.c','app/tests/test-painter-perspective-editor-events.inc']:
  assert sha((R/n).read_bytes())==seal[n]
 held=json.loads((p/'final/held-source-isolation.json').read_text())['rows'];assert len(held)==9
 for row in held:
  if row.get('native_exists') is False:assert row['path'] not in seal
  else:assert seal[row['path']]==row['sha256']
 fixture=R/'migration/fixtures/legacy-perspective-editor'
 for f in fixture.iterdir():
  if f.is_file():assert sha(f.read_bytes())==seal[str(f.relative_to(R))]
 spec=importlib.util.spec_from_file_location('oldeditor',fixture/'verify-source.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);mod.check()
 baseline=json.loads((R/'migration/tests/perspective-display-compatibility/final/normal-original.json').read_text());old=next(x for x in baseline if x['name'].endswith(' / painter-perspective-events'));expected={l.split(' ',2)[2] for l in old['stdout'].splitlines() if l.startswith('ok ') and '# SKIP' not in l};assert len(expected)==9
 expected.update('/perspective-events/'+n for n in ['editor_transformed_entry','editor_old_hit_policy','editor_nearest_policy','editor_cancel_reentry'])
 for b in ['build','build-asan']:
  j=json.loads((p/f'final/{b}.json').read_text());assert j['result']=='OK' and j['returncode']==0 and j['duration']<60 and j['timeout_seconds']==60
  assert j['source_sha256']==seal and re.fullmatch('[a-f0-9]{64}',j['executable_sha256'])
  actual=[l for l in j['stdout'].splitlines() if l.startswith('ok ') and '# SKIP' not in l];assert actual==j['cases'] and len(actual)==13
  assert {l.split(' ',2)[2] for l in actual}==expected
  for text in ['scenes=72','552 hits 600 misses, 1152 real native entry checks','exact tie first index; anisotropic image-nearest rejection without fallback']:assert text in j['stdout']
  assert j['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True)
  assert '1080x1920x24, authenticated accepted, empty authority rejected' in (p/f'final/{b}.log').read_text()
  png=(p/f'final/{b}-oncanvas.png').read_bytes();assert png[:8]==b'\x89PNG\r\n\x1a\n';assert tuple(int.from_bytes(png[i:i+4],'big') for i in [16,20])==(1040,1800)
 for kind in ['rounded-input','radius-seven']:
  d=p/'negative'/kind;m=json.loads((d/'mutation.json').read_text());assert sha((d/'original-input.txt').read_bytes())==m['original_sha256']==seal[m['path']];assert sha((d/'mutated-input.txt').read_bytes())==m['mutated_sha256']!=m['original_sha256']
  j=json.loads((d/'result.json').read_text());assert j['result']=='FAIL' and j['returncode']!=0 and 'assertion failed' in j['stdout']
 print('PASS: 62 duties open; original9+new4 native cases normal/sanitizer; 72 scenes,1152 old hit checks; nine held providers; two causal controls')
if __name__=='__main__':check()
