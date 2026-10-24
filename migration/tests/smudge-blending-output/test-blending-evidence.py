#!/usr/bin/env python3
import pathlib,json,tempfile,shutil,hashlib,importlib.util,contextlib,io
P=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('checker',P/'check-blending-output.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.check();passed=[]
def trial(name,fn):
 with tempfile.TemporaryDirectory()as td:
  q=pathlib.Path(td)/'evidence';shutil.copytree(P,q);fn(q);j=json.loads((q/'manifest.json').read_text());j={n:hashlib.sha256((q/n).read_bytes()).hexdigest()for n in j};(q/'manifest.json').write_text(json.dumps(j))
  try:
   with contextlib.redirect_stdout(io.StringIO()):m.check(q)
  except(AssertionError,KeyError,ValueError):passed.append(name)
  else:raise AssertionError('accepted '+name)
def change(q,n,fn):
 p=q/n;j=json.loads(p.read_text());fn(j);p.write_text(json.dumps(j))
trial('missing source duty',lambda q:change(q,'source-duty-review.json',lambda j:j['rows'].pop()))
trial('weakened criterion',lambda q:change(q,'source-duty-review.json',lambda j:j.update(criterion='subset')))
trial('historical original changed',lambda q:change(q,'source-duty-review.json',lambda j:j['rows'][0]['before'].update(status='DONE')))
trial('future owner dropped',lambda q:change(q,'source-duty-review.json',lambda j:j['rows'][0]['other_owners_at_review'].pop()))
trial('successor criterion altered',lambda q:change(q,'source-duty-review.json',lambda j:j['rows'][0]['after'].update(acceptance='subset')))
trial('fake successor result',lambda q:change(q,'source-duty-review.json',lambda j:j['rows'][0]['after'].update(status='TODO')))
trial('unbound source',lambda q:change(q,'final/source-before-build.json',lambda j:j.update({'app/paint/gimppaintersmudge.cpp':'0'*64})))
trial('unrestored source',lambda q:change(q,'final/source-after-build.json',lambda j:j.pop('app/paint/gimppaintersmudge.cpp')))
trial('missing held provider',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'].pop()))
trial('changed held bytes',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'][0].update(sha256='0'*64)))
for name in ['flow-alias','shade-feedback']:
 trial(name+' accepted',lambda q,name=name:change(q,'final/'+name+'/result.json',lambda j:j.update(result='OK')))
 trial(name+' not pixel mismatch',lambda q,name=name:change(q,'final/'+name+'/result.json',lambda j:j.update(stderr='setup failed')))
 trial(name+' lost affected cases',lambda q,name=name:change(q,'final/'+name+'/result.json',lambda j:j.update(stdout=j['stdout'].replace('Mismatched finish cases/count:','Missing:'))))
 trial(name+' unrelated source edit',lambda q,name=name:change(q,'final/'+name+'/source-before-build.json',lambda j:j.update({'app/core/gimpdynamics.c':'0'*64})))
 trial(name+' no actual mutation',lambda q,name=name:(q/'final'/name/'mutated.cpp').write_text('unchanged'))
trial('normal failed',lambda q:change(q,'final/normal.json',lambda j:j[0].update(result='FAIL')))
trial('normal timeout increased',lambda q:change(q,'final/normal.json',lambda j:j[0].update(duration=61)))
trial('missing standard route',lambda q:change(q,'final/normal.json',lambda j:j.pop()))
trial('portrait reversed',lambda q:(q/'run.log').write_text((q/'run.log').read_text().replace('1080x1920','1920x1080')))
trial('focused case failed',lambda q:change(q,'final/focused-native.json',lambda j:j['runs'][0].update(exit_code=1)))
trial('focused route omitted',lambda q:change(q,'final/focused-native.json',lambda j:j['runs'].pop()))
trial('sanitizer disabled',lambda q:change(q,'final/focused-native.json',lambda j:j['commands'][0].append('-fno-sanitize=vptr')))
trial('source changed during sanitizer',lambda q:change(q,'final/focused-native.json',lambda j:j.update(changed_during_run=['production'])))
print(json.dumps({'status':'PASS','negative_controls':passed},indent=2))
