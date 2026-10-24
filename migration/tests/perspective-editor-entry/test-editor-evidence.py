#!/usr/bin/env python3
import pathlib,json,tempfile,shutil,hashlib,importlib.util,contextlib,io
P=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('checker',P/'check-editor-entry.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.check();passed=[]
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
trial('invented original closure',lambda q:change(q,'audit.json',lambda j:j.update(closed_original_duties=62)))
trial('missing inherited duty',lambda q:change(q,'audit.json',lambda j:j['rows'].pop()))
trial('weakened old criterion',lambda q:change(q,'audit.json',lambda j:j['rows'][0].update(acceptance='subset')))
trial('lost shared owner',lambda q:change(q,'audit.json',lambda j:j['rows'][0]['other_whole_hunk_owners'].pop()))
trial('missing held provider',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'].pop()))
trial('wrong held source',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'][0].update(sha256='0'*64)))
trial('unverified source archive',lambda q:change(q,'final/source-before-build.json',lambda j:j.update({'app/tests/test-painter-perspective-events.c':'0'*64})))
trial('invented passing count',lambda q:change(q,'final/build.json',lambda j:j['cases'].pop()))
trial('failed normal run',lambda q:change(q,'final/build.json',lambda j:j.update(result='FAIL')))
trial('over-budget sanitizer',lambda q:change(q,'final/build-asan.json',lambda j:j.update(duration=61)))
trial('missing pose proof',lambda q:change(q,'final/build.json',lambda j:j.update(stdout=j['stdout'].replace('scenes=72','scenes=71'))))
trial('missing old hit proof',lambda q:change(q,'final/build.json',lambda j:j.update(stdout=j['stdout'].replace('552 hits 600 misses','552 hits 599 misses'))))
trial('unverified portrait',lambda q:change(q,'final/build.json',lambda j:j['xvfb'].update(width=1920,height=1080)))
trial('causal mutation unexpectedly passes',lambda q:change(q,'negative/rounded-input/result.json',lambda j:j.update(result='OK')))
trial('unbound mutation original',lambda q:change(q,'negative/radius-seven/mutation.json',lambda j:j.update(original_sha256='0'*64)))
print(json.dumps({'status':'PASS','negative_controls':passed},indent=2))
