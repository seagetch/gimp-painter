#!/usr/bin/env python3
import copy,hashlib,importlib.util,json,pathlib,shutil,tempfile
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2]
spec=importlib.util.spec_from_file_location('path',E/'check-path.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
base=json.loads((E/'manifest.json').read_text());passed=[]
def change_json(e,name,fn):
 p=e/name;j=json.loads(p.read_text());fn(j);p.write_text(json.dumps(j))
controls=[
 ('parent-closure',lambda r,e,m:m.update(parent_closed=True)),
 ('original-duty-theft',lambda r,e,m:m.update(original_work_rows_closed=21)),
 ('old-runtime-claim',lambda r,e,m:m.update(old_runtime='PASS')),
 ('oncanvas-disabled',lambda r,e,m:m.update(oncanvas=False)),
 ('landscape',lambda r,e,m:m['xvfb'].update(width=1920,height=1080)),
 ('assertions-disabled',lambda r,e,m:m['build_policy'].update(b_ndebug=True)),
 ('timeout',lambda r,e,m:change_json(e,'normal-bounds.json',lambda j:j.update(duration=60.1))),
 ('nonzero-exit',lambda r,e,m:change_json(e,'sanitizer-window-0.json',lambda j:j.update(returncode=1))),
 ('skip',lambda r,e,m:change_json(e,'normal-bounds.json',lambda j:j.update(stdout=j['stdout'].replace('ok 1 /','ok 1 # SKIP /')))),
 ('missing-bounds',lambda r,e,m:change_json(e,'normal-bounds.json',lambda j:j.update(stdout=j['stdout'].replace('scenes=216','scenes=215',1)))),
 ('residual-ink',lambda r,e,m:change_json(e,'normal-window-1.json',lambda j:j.update(stdout=j['stdout'].replace('residual=0','residual=1',1)))),
 ('missing-window',lambda r,e,m:change_json(e,'sanitizer-window-2.json',lambda j:j.update(stdout=j['stdout'].replace('# path window ','# omitted ',1)))),
 ('layout-red-erased',lambda r,e,m:change_json(e,'sanitizer-window-layout-red.json',lambda j:j.update(result='OK'))),
 ('red-erased',lambda r,e,m:change_json(e,'expected-red.json',lambda j:j.update(result='OK'))),
 ('non-extent-change',lambda r,e,m:m.update(extent_only_replacement=['not present','old'])),
 ('source-edit',lambda r,e,m:(r/'app/display/gimpcanvaspath.c').write_text((r/'app/display/gimpcanvaspath.c').read_text()+'\n')),
 ('receipt-source-drift',lambda r,e,m:change_json(e,'normal-window-0.json',lambda j:j['source_sha256'].update({'app/display/gimpcanvaspath.c':'0'*64}))),
]
for name,edit in controls:
 with tempfile.TemporaryDirectory(prefix='path-negative-') as d:
  root=pathlib.Path(d)/'root';e=pathlib.Path(d)/'e';shutil.copytree(E,e,ignore=shutil.ignore_patterns('__pycache__'))
  for p in base['source_sha256'] | base['dependency_source_sha256']:
   out=root/p;out.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(R/p,out)
  m=copy.deepcopy(base);edit(root,e,m)
  m['artifact_sha256']={p:hashlib.sha256((e/p).read_bytes()).hexdigest() for p in m['artifact_sha256']}
  (e/'manifest.json').write_text(json.dumps(m))
  try:mod.check(root,e)
  except (ValueError,AssertionError):passed.append(name)
  else:raise AssertionError('accepted '+name)
print(json.dumps(dict(status='PASS',rejected=len(passed),controls=passed),sort_keys=True))
