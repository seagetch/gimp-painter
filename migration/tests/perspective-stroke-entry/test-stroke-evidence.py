#!/usr/bin/env python3
import pathlib,json,tempfile,shutil,hashlib,importlib.util,contextlib,io
P=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('checker',P/'check-stroke-entry.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.check();passed=[]
def trial(name,fn):
 with tempfile.TemporaryDirectory() as td:
  q=pathlib.Path(td)/'evidence';shutil.copytree(P,q);fn(q)
  manifest=json.loads((q/'manifest.json').read_text())
  for n in manifest:manifest[n]=hashlib.sha256((q/n).read_bytes()).hexdigest()
  (q/'manifest.json').write_text(json.dumps(manifest))
  try:
   with contextlib.redirect_stdout(io.StringIO()):m.check(q)
  except (AssertionError,KeyError,ValueError):passed.append(name)
  else:raise AssertionError('accepted '+name)
def change(q,path,fn):
 p=q/path;j=json.loads(p.read_text());fn(j);p.write_text(json.dumps(j))
def newrow(rows):return next(r for r in rows if r['name'].endswith(' / painter-perspective-stroke-events'))
trial('invented closure',lambda q:change(q,'audit.json',lambda j:j.update(closed_original_duties=87)))
trial('missing original duty',lambda q:change(q,'audit.json',lambda j:j['rows'].pop()))
trial('weakened original criterion',lambda q:change(q,'audit.json',lambda j:j['rows'][0].update(acceptance='subset')))
trial('lost shared owner',lambda q:change(q,'audit.json',lambda j:j['rows'][0]['other_whole_hunk_owners'].pop()))
trial('missing held provider',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'].pop()))
trial('wrong held source',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'][0].update(sha256='0'*64)))
trial('unbound archive',lambda q:change(q,'final/source-before-build.json',lambda j:j.update({'app/tests/test-painter-perspective-stroke-events.c':'0'*64})))
trial('missing original target rerun',lambda q:change(q,'final/build.json',lambda j:j.pop(0)))
trial('invented new count',lambda q:change(q,'final/build.json',lambda j:newrow(j)['cases'].pop()))
trial('failed normal result',lambda q:change(q,'final/build.json',lambda j:newrow(j).update(result='FAIL')))
trial('sanitizer budget exceeded',lambda q:change(q,'final/build-asan.json',lambda j:newrow(j).update(duration=61)))
trial('missing transformed proof',lambda q:change(q,'final/build.json',lambda j:newrow(j).update(stdout=newrow(j)['stdout'].replace('216 transformed scenes','215 transformed scenes'))))
trial('missing threshold tie proof',lambda q:change(q,'final/build.json',lambda j:newrow(j).update(stdout=newrow(j)['stdout'].replace('native exact32 threshold','native relaxed threshold'))))
trial('wrong portrait orientation',lambda q:change(q,'final/build.json',lambda j:newrow(j)['xvfb'].update(width=1920,height=1080)))
trial('threshold mutation passes',lambda q:change(q,'negative/threshold-31/result.json',lambda j:j.update(result='OK')))
trial('tie mutation unbound',lambda q:change(q,'negative/last-tie/mutation.json',lambda j:j.update(original_sha256='0'*64)))
trial('projection mutation passes',lambda q:change(q,'negative/reversed-projection/result.json',lambda j:j.update(result='OK')))
print(json.dumps({'status':'PASS','negative_controls':passed},indent=2))
