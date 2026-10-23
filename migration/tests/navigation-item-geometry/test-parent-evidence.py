#!/usr/bin/env python3
import copy,hashlib,importlib.util,json,pathlib,shutil,tempfile
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2]
spec=importlib.util.spec_from_file_location('parent_proof',E/'check-parent.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
base=json.loads((E/'manifest.json').read_text());passed=[]
def change(e,name,fn):
 p=e/name;j=json.loads(p.read_text());fn(j);p.write_text(json.dumps(j))
def remove_old_line(e):
 p=next(p for p in (e/'pinned-hunks').iterdir() if any(x.startswith('-') for x in p.read_text().splitlines()));lines=p.read_text().splitlines(keepends=True);i=next(i for i,x in enumerate(lines) if x.startswith('-'));del lines[i];p.write_text(''.join(lines))
def remove_bound_input(e,m,path):
 m['source_sha256'].pop(path)
 for kind in ['normal','sanitizer']:
  for group in mod.groups+list(mod.old_groups):
   change(e,kind+'-'+group+'.json',lambda j:j['source_sha256'].pop(path))
controls=[
 ('sanitizer-build-disabled',lambda r,e,m:change(e,'build-provenance.json',lambda j:j['builds'][1]['options'].update(b_sanitize='none'))),
 ('missing-held-path',lambda r,e,m:change(e,'held-source-isolation.json',lambda j:j.update(rows=j['rows'][1:]))),
 ('held-candidate-drift',lambda r,e,m:change(e,'held-source-isolation.json',lambda j:j['rows'][0].update(sha256='0'*64))),
 ('duplicated-flip-scene',lambda r,e,m:change(e,'normal-selection-1.json',lambda j:j.update(stdout=j['stdout'].replace('H=0 V=1','H=0 V=0',1)))),
 ('wrong-render-angle',lambda r,e,m:change(e,'normal-chunks-4.json',lambda j:j.update(stdout=j['stdout'].replace('angle=37','angle=36',1)))),
 ('missing-coverage-duty',lambda r,e,m:change(e,'coverage.json',lambda j:j.update(rows=j['rows'][1:]))),
 ('coverage-pending',lambda r,e,m:change(e,'coverage.json',lambda j:j.update(status='PENDING'))),
 ('coverage-missing-receipt',lambda r,e,m:change(e,'coverage.json',lambda j:j['rows'][0].update(receipts=['missing.json']))),
 ('unbound-native-include',lambda r,e,m:remove_bound_input(e,m,'app/tests/test-painter-navigation-render-geometry.inc')),
 ('unbound-old-oracle',lambda r,e,m:remove_bound_input(e,m,'migration/fixtures/legacy-pan/oracle.inc')),
 ('criterion-rewrite',lambda r,e,m:m.update(criterion='weaker criterion')),
 ('duty-loss',lambda r,e,m:m.update(original_work_rows=195)),
 ('whole-port-claim',lambda r,e,m:m.update(whole_port_complete=True)),
 ('old-runtime-claim',lambda r,e,m:m.update(old_runtime='PASS')),
 ('oncanvas-disabled',lambda r,e,m:m.update(actual_ui_oncanvas=False)),
 ('old-detector-relabel',lambda r,e,m:m.update(historical_detector_settings_unchanged=False)),
 ('landscape',lambda r,e,m:m['xvfb'].update(width=1920,height=1080)),
 ('assertions-disabled',lambda r,e,m:m['build_policy'].update(b_ndebug=True)),
 ('timeout',lambda r,e,m:change(e,'normal-selection-0.json',lambda j:j.update(duration=60.1))),
 ('nonzero-exit',lambda r,e,m:change(e,'sanitizer-checker-0.json',lambda j:j.update(returncode=1))),
 ('skip',lambda r,e,m:change(e,'normal-selection-0.json',lambda j:j.update(stdout=j['stdout'].replace('ok 1 /','ok 1 # SKIP /')))),
 ('selection-residual',lambda r,e,m:change(e,'normal-selection-1.json',lambda j:j.update(stdout=j['stdout'].replace('residual=0','residual=1',1)))),
 ('missing-grid-scene',lambda r,e,m:change(e,'normal-grid-precise-1.json',lambda j:j.update(stdout=j['stdout'].replace('# precise grid ','# omitted ',1)))),
 ('checker-pixels',lambda r,e,m:change(e,'normal-checker-1.json',lambda j:j.update(stdout=j['stdout'].replace('pixels=220900','pixels=1',1)))),
 ('signed-coverage',lambda r,e,m:change(e,'normal-signed-3.json',lambda j:j.update(stdout=j['stdout'].replace('scenes=384','scenes=383',1)))),
 ('chunk-pixels',lambda r,e,m:change(e,'normal-chunks-1.json',lambda j:j.update(stdout=__import__('re').sub(r'pixels=\d+','pixels=0',j['stdout'],count=1)))),
 ('removed-legacy-line-erased',lambda r,e,m:remove_old_line(e)),
 ('other-owner-theft',lambda r,e,m:change(e,'audit.json',lambda j:j['rows'][0].update(other_whole_hunk_owners=[]))),
 ('missing-source-route',lambda r,e,m:change(e,'primitive-source-duty-review.json',lambda j:j.update(rows=j['rows'][1:]))),
 ('duplicate-duty',lambda r,e,m:change(e,'audit.json',lambda j:j['rows'].__setitem__(1,copy.deepcopy(j['rows'][0])))),
 ('source-edit',lambda r,e,m:(r/'app/display/gimpdisplayshell-callbacks.c').write_text((r/'app/display/gimpdisplayshell-callbacks.c').read_text()+'\n')),
 ('receipt-source-drift',lambda r,e,m:change(e,'normal-selection-0.json',lambda j:j['source_sha256'].update({'app/display/gimpdisplayshell.c':'0'*64}))),
 ('original-case-name',lambda r,e,m:change(e,'normal-original.json',lambda j:j.update(stdout=j['stdout'].replace('/navigation-events/default-rotation','/navigation-events/wrong-original')))),
]
source_paths=set(base['source_sha256'])
for name in ['render-source-duty-review.json','primitive-source-duty-review.json','navigation-source-duty-review.json']:source_paths.update(json.loads((E/name).read_text())['current_sources_sha256'])
for name,edit in controls:
 with tempfile.TemporaryDirectory(prefix='matrix-negative-') as d:
  root=pathlib.Path(d)/'root';e=pathlib.Path(d)/'e';shutil.copytree(E,e,ignore=shutil.ignore_patterns('__pycache__'))
  for p in source_paths:
   out=root/p;out.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(R/p,out)
  m=copy.deepcopy(base);edit(root,e,m)
  m['artifact_sha256']={p:hashlib.sha256((e/p).read_bytes()).hexdigest() for p in m['artifact_sha256']};(e/'manifest.json').write_text(json.dumps(m))
  try:mod.check(root,e)
  except (ValueError,AssertionError):passed.append(name)
  else:raise AssertionError('accepted '+name)
print(json.dumps(dict(status='PASS',rejected=len(passed),controls=passed),sort_keys=True))
