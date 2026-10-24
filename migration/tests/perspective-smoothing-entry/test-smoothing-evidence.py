#!/usr/bin/env python3
import pathlib,json,tempfile,shutil,hashlib,importlib.util,contextlib,io
P=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('checker',P/'check-smoothing-entry.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.check();passed=[]
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
def newrow(rows):return next(r for r in rows if r['name'].endswith(' / painter-perspective-lifecycle-events'))
trial('invented original closure',lambda q:change(q,'audit.json',lambda j:j.update(closed_original_duties=260)))
trial('missing MyPaint duty',lambda q:change(q,'audit.json',lambda j:j['rows'].remove(next(r for r in j['rows'] if 'mypainttool.cpp' in r['path']))))
trial('weakened original feel criterion',lambda q:change(q,'audit.json',lambda j:j['rows'][0].update(acceptance='subset')))
trial('lost shared owner',lambda q:change(q,'audit.json',lambda j:j['rows'][0]['other_whole_hunk_owners'].pop()))
trial('missing held provider',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'].pop()))
trial('wrong held source',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'][0].update(sha256='0'*64)))
trial('unbound archive',lambda q:change(q,'final/source-before-build.json',lambda j:j.update({'app/tests/test-painter-perspective-smoothing-events.inc':'0'*64})))
trial('unbound successor source',lambda q:change(q,'lifecycle-successor.json',lambda j:j.update(after_sha256='0'*64)))
trial('wrong historical predecessor',lambda q:change(q,'lifecycle-successor.json',lambda j:j.update(before_sha256='0'*64)))
trial('missing original lifecycle case',lambda q:change(q,'final/build.json',lambda j:newrow(j)['cases'].pop(0)))
trial('failed normal result',lambda q:change(q,'final/build.json',lambda j:newrow(j).update(result='FAIL')))
trial('sanitizer budget exceeded',lambda q:change(q,'final/build-asan.json',lambda j:newrow(j).update(duration=61)))
trial('smoothing not exercised',lambda q:change(q,'final/build.json',lambda j:newrow(j).update(stdout=newrow(j)['stdout'].replace('150 smoothed','0 smoothed'))))
trial('empty ink admitted',lambda q:change(q,'final/build.json',lambda j:newrow(j).update(stdout=newrow(j)['stdout'].replace('2351 alpha','0 alpha'))))
trial('wrong portrait orientation',lambda q:change(q,'final/build.json',lambda j:newrow(j)['xvfb'].update(width=1920,height=1080)))
trial('smoothing fault passes',lambda q:change(q,'negative/identity-smoothing/result.json',lambda j:j.update(result='OK')))
trial('angle fault unbound',lambda q:change(q,'negative/wrong-angle/mutation.json',lambda j:j.update(original_sha256='0'*64)))
trial('raster fault passes',lambda q:change(q,'negative/shifted-raster-envelope/result.json',lambda j:j.update(result='OK')))
trial('zero-mask diagnostic erased',lambda q:(q/'diagnostics/diagnostic-7/run.log').write_text('PASS'))
trial('ordering source substituted',lambda q:change(q,'ordering-sources.json',lambda j:j['sources'][0].update(blob='0'*40)))
print(json.dumps({'status':'PASS','negative_controls':passed},indent=2))
