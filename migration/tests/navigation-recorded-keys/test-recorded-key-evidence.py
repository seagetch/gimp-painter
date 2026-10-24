#!/usr/bin/env python3
import copy,hashlib,importlib.util,json,pathlib,re,shutil,tempfile
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2]
spec=importlib.util.spec_from_file_location('recorded_proof',E/'check-recorded-keys.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);base=json.loads((E/'manifest.json').read_text());passed=[]
def change(e,name,fn):
 p=e/name;j=json.loads(p.read_text());fn(j);p.write_text(json.dumps(j))
def remove_bound_input(e,m,path):
 m['source_sha256'].pop(path)
 for kind in ['normal','sanitizer']:
  for group in ['','-reset','-original','-rotation','-zoom','-space']:change(e,kind+group+'.json',lambda j:j['source_sha256'].pop(path))
controls=[
 ('criterion-rewrite',lambda r,e,m:m.update(criterion='weaker')),
 ('production-fix-claim',lambda r,e,m:m.update(production_fix=True)),
 ('whole-port-claim',lambda r,e,m:m.update(whole_port_complete=True)),
 ('old-runtime-claim',lambda r,e,m:m.update(old_runtime='PASS')),
 ('oncanvas-disabled',lambda r,e,m:m.update(actual_ui_oncanvas=False)),
 ('old-detector-relabel',lambda r,e,m:m.update(historical_detector_settings_unchanged=False)),
 ('landscape',lambda r,e,m:m['xvfb'].update(width=1920,height=1080)),
 ('leak-claim',lambda r,e,m:m['sanitizer_policy'].update(leaks=True)),
 ('timeout',lambda r,e,m:change(e,'normal.json',lambda j:j.update(duration=60.1))),
 ('failed-exit',lambda r,e,m:change(e,'sanitizer.json',lambda j:j.update(returncode=1))),
 ('missing-case',lambda r,e,m:change(e,'normal.json',lambda j:j.update(cases=3))),
 ('missing-event',lambda r,e,m:change(e,'normal.json',lambda j:j.update(stdout=re.sub(r'^# recorded key tool=.*\n','',j['stdout'],count=1,flags=re.M)))),
 ('duplicate-event',lambda r,e,m:change(e,'normal.json',lambda j:j.update(stdout=j['stdout'].replace('seq=2 ','seq=3 ',1)))),
 ('wrong-mirror',lambda r,e,m:change(e,'normal.json',lambda j:j.update(stdout=j['stdout'].replace('# recorded key tool=0 mirror=0','# recorded key tool=0 mirror=1',1)))),
 ('literal-old-angle',lambda r,e,m:change(e,'normal.json',lambda j:j.update(stdout=j['stdout'].replace('old=0 native=0','old=1 native=0',1)))),
 ('wrong-native-angle',lambda r,e,m:change(e,'normal.json',lambda j:j.update(stdout=j['stdout'].replace('native=0','native=1',1)))),
 ('key-total-loss',lambda r,e,m:change(e,'normal.json',lambda j:j.update(stdout=j['stdout'].replace('keys=33','keys=32',1)))),
 ('original-name',lambda r,e,m:change(e,'normal-original.json',lambda j:j.update(stdout=j['stdout'].replace('/navigation-events/default-rotation','/navigation-events/wrong')))),
 ('duty-loss',lambda r,e,m:change(e,'audit.json',lambda j:j.update(rows=j['rows'][1:]))),
 ('other-owner-theft',lambda r,e,m:change(e,'audit.json',lambda j:j['rows'][0].update(other_whole_hunk_owners=[]))),
 ('source-route-loss',lambda r,e,m:change(e,'source-duty-review.json',lambda j:j.update(rows=j['rows'][1:]))),
 ('source-edit',lambda r,e,m:(r/'app/tests/test-painter-navigation-recorded-keys.inc').write_text('changed')),
 ('retained-trace-edit',lambda r,e,m:(r/'migration/fixtures/legacy-rotation-operations/gui-captures/rotation.tsv').write_text('changed')),
 ('unbound-runtime-asset',lambda r,e,m:remove_bound_input(e,m,'migration/fixtures/legacy-zoom/zoom.tsv')),
 ('unbound-old-oracle',lambda r,e,m:remove_bound_input(e,m,'migration/fixtures/legacy-navigation-applied/rotation-oracle.inc')),
 ('held-path-loss',lambda r,e,m:change(e,'held-source-isolation.json',lambda j:j.update(rows=j['rows'][1:]))),
 ('sanitizer-disabled',lambda r,e,m:change(e,'build-provenance.json',lambda j:j['builds'][1]['options'].update(b_sanitize='none'))),
]
paths=set(base['source_sha256']);paths.update(json.loads((E/'source-duty-review.json').read_text())['current_sources_sha256'])
for name,edit in controls:
 with tempfile.TemporaryDirectory(prefix='recorded-keys-negative-') as d:
  root=pathlib.Path(d)/'root';e=pathlib.Path(d)/'e';shutil.copytree(E,e,ignore=shutil.ignore_patterns('__pycache__'))
  for p in paths:
   out=root/p;out.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(R/p,out)
  m=copy.deepcopy(base);edit(root,e,m);m['artifact_sha256']={p:hashlib.sha256((e/p).read_bytes()).hexdigest() for p in m['artifact_sha256']};(e/'manifest.json').write_text(json.dumps(m))
  try:mod.check(root,e)
  except (ValueError,AssertionError):passed.append(name)
  else:raise AssertionError('accepted '+name)
print(json.dumps(dict(status='PASS',rejected=len(passed),controls=passed),sort_keys=True))
