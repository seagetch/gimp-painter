#!/usr/bin/env python3
import pathlib,json,tempfile,shutil,hashlib,importlib.util,contextlib,io
P=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('checker',P/'check-initial-sampling.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.check();passed=[]
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
def native(rows):return next(r for r in rows if r['name'].endswith(' / gimp-painter-smudge'))
trial('invented52-duty closure',lambda q:change(q,'audit.json',lambda j:j.update(closed_original_duties=52)))
trial('missing source duty',lambda q:change(q,'audit.json',lambda j:j['rows'].pop()))
trial('weakened sampling criterion',lambda q:change(q,'audit.json',lambda j:j['rows'][0].update(acceptance='subset')))
trial('lost successor owner',lambda q:change(q,'audit.json',lambda j:j['rows'][0]['other_whole_hunk_owners'].pop()))
trial('missing held provider',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'].pop()))
trial('wrong held source',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'][0].update(sha256='0'*64)))
trial('unbound normal source',lambda q:change(q,'final/source-before-normal-build.json',lambda j:j.update({'app/paint/gimppaintersmudge.cpp':'0'*64})))
trial('red used different fixture',lambda q:change(q,'final/expected-red/source-before-build.json',lambda j:j.update({'app/tests/test-painter-smudge-initial-sampling.inc':'0'*64})))
trial('red had no positive dab',lambda q:change(q,'final/expected-red/result.json',lambda j:j.update(stdout=j['stdout'].replace('dabs=4','dabs=0'))))
trial('red passed',lambda q:change(q,'final/expected-red/result.json',lambda j:j.update(result='OK')))
trial('candidate mismatch',lambda q:(q/'final/candidate.cpp').write_bytes((q/'final/candidate.cpp').read_bytes()+b'\n'))
trial('missing old stroke route',lambda q:change(q,'final/normal.json',lambda j:j.pop()))
trial('failed native green',lambda q:change(q,'final/normal.json',lambda j:native(j).update(result='FAIL')))
trial('native time budget exceeded',lambda q:change(q,'final/normal.json',lambda j:native(j).update(duration=61)))
trial('wrong initial seed',lambda q:change(q,'final/normal.json',lambda j:native(j).update(stdout=native(j)['stdout'].replace('expectedR=40 actualR=40','expectedR=40 actualR=220'))))
trial('unchanged raster claimed',lambda q:change(q,'final/normal.json',lambda j:native(j).update(stdout=native(j)['stdout'].replace('changed native bytes=486','changed native bytes=0'))))
trial('wrong portrait',lambda q:change(q,'final/normal.json',lambda j:native(j)['xvfb'].update(width=1920,height=1080)))
trial('Filter RTTI owner omitted',lambda q:change(q,'final/focused-native.json',lambda j:j['sources_sha256'].pop('app/config/gimppainterfilterconfig.cpp')))
trial('sanitizer runtime failure ignored',lambda q:change(q,'final/focused-native.json',lambda j:j['runs'][0].update(exit_code=1)))
trial('sanitizer disabled to pass',lambda q:change(q,'final/focused-native.json',lambda j:j['commands'][0].append('-fno-sanitize=vptr')))
trial('kernel old bytes differ',lambda q:change(q,'final/focused-pixels.json',lambda j:j.update(exact_legacy_match=False)))
trial('historical mixed RTTI failure erased',lambda q:change(q,'diagnostics/mixed-rtti-focused-native.json',lambda j:[r.update(exit_code=0) for r in j['runs']]))
print(json.dumps({'status':'PASS','negative_controls':passed},indent=2))
