#!/usr/bin/env python3
import importlib.util,pathlib,tempfile,shutil,json,hashlib,contextlib,io
P=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('checker',P/'check-registration.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.check();controls=[]
def trial(name,fn):
 with tempfile.TemporaryDirectory() as td:
  q=pathlib.Path(td)/'evidence';shutil.copytree(P,q);fn(q)
  manifest=json.loads((q/'manifest.json').read_text())
  for n in manifest:manifest[n]=hashlib.sha256((q/n).read_bytes()).hexdigest()
  (q/'manifest.json').write_text(json.dumps(manifest))
  try:
   with contextlib.redirect_stdout(io.StringIO()):m.check(q)
  except (AssertionError,KeyError,ValueError):controls.append(name)
  else:raise AssertionError('accepted '+name)
def rowchange(q,fn):
 p=q/'build.json';r=json.loads(p.read_text());fn(r);p.write_text(json.dumps(r))
trial('missing registered angle',lambda q:rowchange(q,lambda r:r.pop()))
trial('timeout extension cannot excuse slow scope',lambda q:rowchange(q,lambda r:r[0].update(duration=61)))
trial('failure cannot pass',lambda q:rowchange(q,lambda r:r[0].update(result='FAIL')))
trial('invented case count',lambda q:rowchange(q,lambda r:r[0]['cases'].pop()))
trial('duplicate scope',lambda q:rowchange(q,lambda r:r.__setitem__(4,r[3])))
trial('missing default exclusions',lambda q:(q/'meson.build').write_text((q/'meson.build').read_text().replace("'--skip-prefix', '/perspective-ui/old-path']","'--skip-prefix', '/unrelated']")))
trial('removed fixed budget',lambda q:(q/'meson.build').write_text((q/'meson.build').read_text().replace('timeout: 60, is_parallel: false','timeout: 180, is_parallel: false')))
trial('unverified portrait',lambda q:(q/'build.log').write_text((q/'build.log').read_text().replace('1080x1920x24','1920x1080x24')))
print(json.dumps({'status':'PASS','negative_controls':controls},indent=2))
