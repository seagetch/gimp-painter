#!/usr/bin/env python3
import copy,hashlib,importlib.util,json,pathlib,shutil,tempfile
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2]
spec=importlib.util.spec_from_file_location('reset_proof',E/'check-reset.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
base=json.loads((E/'manifest.json').read_text());passed=[]
def change(e,name,fn):
 p=e/name;j=json.loads(p.read_text());fn(j);p.write_text(json.dumps(j))
def rewrite_bound_test(root,e,m):
 p='app/tests/test-painter-navigation-reset-action.inc';f=root/p;f.write_text(f.read_text().replace('"view-rotate-reset"','"view-rotate-15"'))
 h=hashlib.sha256(f.read_bytes()).hexdigest();m['source_sha256'][p]=h
 for kind in ['normal','sanitizer']:
  for group in ['','-original','-rotation','-zoom','-space']:
   change(e,kind+group+'.json',lambda j:j['source_sha256'].update({p:h}))
controls=[
 ('runtime-asset-drift',lambda r,e,m:(r/'migration/fixtures/legacy-zoom/zoom.tsv').write_text('wrong')),
 ('runtime-asset-map-loss',lambda r,e,m:change(e,'retrospective-runtime-dependencies.json',lambda j:j.update(assets=j['assets'][1:]))),
 ('runtime-assertion-site-loss',lambda r,e,m:change(e,'retrospective-runtime-dependencies.json',lambda j:j['assets'][0].update(native_read_functions=['retained_old_starts']))),
 ('wrong-registered-action-coherent-hashes',rewrite_bound_test),
 ('criterion-rewrite',lambda r,e,m:m.update(criterion='weaker')),
 ('production-fix-claim',lambda r,e,m:m.update(production_fix=True)),
 ('whole-port-claim',lambda r,e,m:m.update(whole_port_complete=True)),
 ('historical-runtime-claim',lambda r,e,m:m.update(old_runtime='PASS')),
 ('oncanvas-disabled',lambda r,e,m:m.update(actual_ui_oncanvas=False)),
 ('detector-relabel',lambda r,e,m:m.update(historical_detector_settings_unchanged=False)),
 ('landscape',lambda r,e,m:m['xvfb'].update(width=1920,height=1080)),
 ('leak-claim',lambda r,e,m:m['sanitizer_policy'].update(leaks=True)),
 ('timeout',lambda r,e,m:change(e,'normal.json',lambda j:j.update(duration=60.1))),
 ('nonzero-exit',lambda r,e,m:change(e,'sanitizer.json',lambda j:j.update(returncode=1))),
 ('missing-case',lambda r,e,m:change(e,'normal.json',lambda j:j.update(cases=5))),
 ('reset-not-zero',lambda r,e,m:change(e,'normal.json',lambda j:j.update(stdout=j['stdout'].replace('after=0','after=1',1)))),
 ('repeat-trail',lambda r,e,m:change(e,'normal.json',lambda j:j.update(stdout=j['stdout'].replace('repeat-residual=0','repeat-residual=1',1)))),
 ('duplicate-flip',lambda r,e,m:change(e,'normal.json',lambda j:j.update(stdout=j['stdout'].replace('H=1 V=0 entry=0 angle=0','H=0 V=0 entry=0 angle=0',1)))),
 ('zoom-changed',lambda r,e,m:change(e,'normal.json',lambda j:j.update(stdout=j['stdout'].replace('zoom=1 repeat','zoom=2 repeat',1)))),
 ('reentry-no-motion',lambda r,e,m:change(e,'normal.json',lambda j:j.update(stdout=__import__('re').sub(r'moved-angle=[^ ]+','moved-angle=0',j['stdout'],count=1)))),
 ('original-name',lambda r,e,m:change(e,'normal-original.json',lambda j:j.update(stdout=j['stdout'].replace('/navigation-events/default-rotation','/navigation-events/wrong-original')))),
 ('other-owner-theft',lambda r,e,m:change(e,'audit.json',lambda j:j['rows'][0].update(other_whole_hunk_owners=[]))),
 ('duty-loss',lambda r,e,m:change(e,'audit.json',lambda j:j.update(rows=j['rows'][1:]))),
 ('source-drift',lambda r,e,m:(r/'app/tests/test-painter-navigation-reset-action.inc').write_text('wrong')),
 ('held-path-loss',lambda r,e,m:change(e,'held-source-isolation.json',lambda j:j.update(rows=j['rows'][1:]))),
 ('sanitizer-disabled',lambda r,e,m:change(e,'build-provenance.json',lambda j:j['builds'][1]['options'].update(b_sanitize='none'))),
]
for name,edit in controls:
 with tempfile.TemporaryDirectory(prefix='reset-negative-') as d:
  root=pathlib.Path(d)/'root';e=pathlib.Path(d)/'e';shutil.copytree(E,e,ignore=shutil.ignore_patterns('__pycache__'))
  for p in list(base['source_sha256'])+[a['path'] for a in json.loads((E/'retrospective-runtime-dependencies.json').read_text())['assets']]:
   out=root/p;out.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(R/p,out)
  m=copy.deepcopy(base);edit(root,e,m);m['artifact_sha256']={p:hashlib.sha256((e/p).read_bytes()).hexdigest() for p in m['artifact_sha256']};(e/'manifest.json').write_text(json.dumps(m))
  try:mod.check(root,e)
  except (ValueError,AssertionError):passed.append(name)
  else:raise AssertionError('accepted '+name)
print(json.dumps(dict(status='PASS',rejected=len(passed),controls=passed),sort_keys=True))
