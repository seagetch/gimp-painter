#!/usr/bin/env python3
"""Validate bounded independent checkpoints, not a fictional whole-app run."""
import hashlib,json,pathlib,re,struct,subprocess,sys
P=pathlib.Path(__file__).resolve().parent;ROOT=P.parents[3]
def load(p):return json.loads(p.read_text())
def tap(text,n):
 assert len(re.findall(r'^ok \d+ ',text,re.M))==n
 assert not re.search(r'^not ok|^ok .*#\s*(SKIP|TODO)|ERROR: AddressSanitizer|runtime error:',text,re.M|re.I)
def normal(rows,expected):
 assert len(rows)==len(expected)
 seen=set()
 for row in rows:
  name=row['name'].split(' / ')[-1];assert name not in seen;seen.add(name)
  assert row['result']=='OK' and row['returncode']==0 and 'env'not in row
  if 'stdout'in row:tap(row['stdout'],expected[name])
  else:assert row['tap_ok']==expected[name]
 assert seen==set(expected)
# Four exact dependency-owner routes, real native tests and bounded sanitizer.
for row in load(P/'filter/normal-summary.json')['normal']:
 assert row['returncode']==0 and row['result']=='OK' and row['tap_ok']==1
 raw=load(P/'filter'/f"{row['case']}.json");assert 'env'not in raw;tap(raw['stdout'],1)
assert len(load(P/'filter/normal-summary.json')['normal'])==4
r=load(P/'filter/sanitizers.json');assert r['exit_code']==0 and not r['changed_after_compile'];tap(r['stdout'],4)
# Popup/selector/dropdown registered normal and separately instrumented runs.
expected={'painter-popup-handles':34,'editor-dropdown':57,'mypaint-selector-contracts':29}
normal(load(P/'popup/normal-results.json')['results'],expected)
rows=load(P/'popup/results.json')['results'];assert len(rows)==3 and {r['target']for r in rows}==set(expected)
for row in rows:
 name=row['target'];assert row['exit_code']==0 and row['tap_ok']==expected[name]
 for prefix in ('','normal-'):tap((P/'popup'/f'{prefix}{name}.stdout').read_text(),expected[name])
# Earlier editor checkpoint is immutable; later controls have separate receipts.
rows=load(P/'editor/results.json')['results'];assert len(rows)==2
assert [(r['mode'],r['exit_code'],r['tap_ok'])for r in rows]==[('existing-25',0,25),('page-memory-3',0,3)]
for row in rows:tap((P/'editor'/f"{row['mode']}.stdout").read_text(),row['tap_ok'])
normal(load(ROOT/'migration/tests/editor-page-memory/page-memory-final.json'),{'painter-mypaint-editor':25,'painter-editor-page-memory':3})
normal(load(P/'editor-controls/normal.json')['tests'],{'painter-editor-control-lifetimes':3})
rows=load(P/'editor-controls/results.json')['results'];assert len(rows)==1 and rows[0]['exit_code']==0 and rows[0]['tap_ok']==3 and rows[0]['timeout_seconds']==120
tap((P/'editor-controls/painter-editor-control-lifetimes.stdout').read_text(),3)
for name,digest in load(P/'editor-controls/source-sha256.json').items():
 assert hashlib.sha256((P/'editor-controls/tested-source'/name).read_bytes()).hexdigest()==digest
# Native application-owned services; startup is normal only, never called ASan.
A=ROOT/'migration/tests/application-lifecycle-signals'
normal(load(A/'startup-results-sanitized.json')['tests'],{'painter-type-startup':7})
normal(load(A/'http-results-sanitized.json')['tests'],{'painter-http':18})
rows=load(A/'sanitizer-results.json');assert len(rows)==2
for row in rows:
 expected_count={'painter-http':18,'painter-http-ui':6}[row['name']]
 assert row['returncode']==0 and row['tap_passed']==expected_count
 tap((A/f"sanitizer-{row['name']}.log").read_text(),expected_count)
# Current layer owner checkpoints have source archives and separate exact runs.
for package in ('layer-tiles-initializer','layer-signal-dispatch'):
 subprocess.run([sys.executable,str(ROOT/'migration/tests/vfunc-exception-containment'/package/'validate.py')],check=True)
r=load(P/'application-exit/result.json')
assert r['resolution']==r['window_size']==[1080,1920] and r['painter_canvas_ui']
assert r['http_status']==200 and r['exit_code']==0 and r['http_listener_closed_after_exit'] and r['accepted_exit_hook_observed']
assert struct.unpack('>II',(P/'application-exit/portrait-oncanvas-http.png').read_bytes()[16:24])==(1080,1920)
for name,digest in r['sources_sha256'].items():
 assert hashlib.sha256((P/'application-exit/tested-source'/name).read_bytes()).hexdigest()==digest
# Package-local evidence manifests: no silent modifications to retained outcomes.
count=0
for manifest in P.rglob('*sha256.json'):
 values=load(manifest)
 if not isinstance(values,dict)or not all(isinstance(v,str)and len(v)==64 for v in values.values()):continue
 for name,digest in values.items():
  path=manifest.parent/name
  if path.is_file():assert hashlib.sha256(path.read_bytes()).hexdigest()==digest;count+=1
print(f'PASS bounded owner-route receipts; {count} local artifact hashes; startup normal only; no whole-app sanitizer claim')
