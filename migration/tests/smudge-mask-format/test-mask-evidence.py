#!/usr/bin/env python3
import pathlib,json,tempfile,shutil,hashlib,importlib.util,contextlib,io,re
P=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('checker',P/'check-mask-format.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.check();passed=[]
def trial(name,fn):
 with tempfile.TemporaryDirectory()as td:
  q=pathlib.Path(td)/'evidence';shutil.copytree(P,q);fn(q)
  manifest=json.loads((q/'manifest.json').read_text())
  for n in manifest:manifest[n]=hashlib.sha256((q/n).read_bytes()).hexdigest()
  (q/'manifest.json').write_text(json.dumps(manifest))
  try:
   with contextlib.redirect_stdout(io.StringIO()):m.check(q)
  except(AssertionError,KeyError,ValueError):passed.append(name)
  else:raise AssertionError('accepted '+name)
def change(q,path,fn):
 p=q/path;j=json.loads(p.read_text());fn(j);p.write_text(json.dumps(j))
def native(rows):return next(r for r in rows if r['name'].endswith('/ gimp-painter-smudge'))
trial('invented52-duty closure',lambda q:change(q,'audit.json',lambda j:j.update(closed_original_duties=52)))
trial('missing duty',lambda q:change(q,'audit.json',lambda j:j['rows'].pop()))
trial('weakened criterion',lambda q:change(q,'audit.json',lambda j:j['rows'][0].update(acceptance='subset')))
trial('lost successor owner',lambda q:change(q,'audit.json',lambda j:j['rows'][0]['other_whole_hunk_owners'].pop()))
trial('missing tenth held provider',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'].pop()))
trial('wrong committed held source',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'][0].update(sha256='0'*64)))
trial('unbound candidate',lambda q:change(q,'final/source-before-build.json',lambda j:j.update({'app/paint/gimppaintersmudge.cpp':'0'*64})))
trial('different causal fixture',lambda q:change(q,'final/expected-red/source.json',lambda j:j.update({'app/tests/test-painter-smudge-center-matrix.inc':'0'*64})))
trial('red passed',lambda q:change(q,'final/expected-red/result.json',lambda j:j.update(result='OK')))
trial('different old center result',lambda q:change(q,'final/expected-red/result.json',lambda j:j.update(stdout=j['stdout'].replace('expected=130 actual=220','expected=220 actual=220'))))
trial('candidate bytes mismatch',lambda q:(q/'final/candidate.cpp').write_bytes((q/'final/candidate.cpp').read_bytes()+b'\n'))
trial('missing normal route',lambda q:change(q,'final/normal.json',lambda j:j.pop()))
trial('failed candidate',lambda q:change(q,'final/normal.json',lambda j:native(j).update(result='FAIL')))
trial('native budget raised',lambda q:change(q,'final/normal.json',lambda j:native(j).update(duration=61)))
trial('missing matrix axis',lambda q:change(q,'final/normal.json',lambda j:native(j).update(stdout=native(j)['stdout'].replace('paper=128','paper=0'))))
trial('no actual dabs',lambda q:change(q,'final/normal.json',lambda j:native(j).update(stdout=native(j)['stdout'].replace('updates=5','updates=0'))))
trial('outside silently ignored',lambda q:change(q,'final/normal.json',lambda j:native(j).update(stdout=re.sub(r'outside=[1-9][0-9]*','outside=0',native(j)['stdout']))))
trial('portrait changed',lambda q:(q/'final/normal.log').write_text((q/'final/normal.log').read_text().replace('1080x1920','1920x1080')))
trial('paper expected bytes differ',lambda q:change(q,'final/normal-paper.json',lambda j:j[0].update(exact=False)))
trial('paper route omitted',lambda q:change(q,'final/normal-paper.json',lambda j:j.pop()))
trial('Filter RTTI source omitted',lambda q:change(q,'final/focused-native.json',lambda j:j['sources_sha256'].pop('app/config/gimppainterfilterconfig.cpp')))
trial('sanitizer failed',lambda q:change(q,'final/focused-native.json',lambda j:j['runs'][0].update(exit_code=1)))
trial('sanitizer disabled',lambda q:change(q,'final/focused-native.json',lambda j:j['commands'][0].append('-fno-sanitize=vptr')))
trial('sanitizer paper route omitted',lambda q:change(q,'final/focused-native.json',lambda j:j['runs'].pop()))
trial('kernel bytes differ',lambda q:change(q,'final/focused-pixels.json',lambda j:j.update(exact_legacy_match=False)))
trial('causal trace erased',lambda q:(q/'diagnostics/diagnostic7/run.log').write_text(''))
print(json.dumps({'status':'PASS','negative_controls':passed},indent=2))
