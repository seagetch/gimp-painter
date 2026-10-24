#!/usr/bin/env python3
import pathlib,json,tempfile,shutil,hashlib,importlib.util,contextlib,io
P=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('checker',P/'check-current-dynamics.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.check();passed=[]
def trial(name,fn):
 with tempfile.TemporaryDirectory()as td:
  q=pathlib.Path(td)/'evidence';shutil.copytree(P,q);fn(q);manifest=json.loads((q/'manifest.json').read_text())
  for n in manifest:manifest[n]=hashlib.sha256((q/n).read_bytes()).hexdigest()
  (q/'manifest.json').write_text(json.dumps(manifest))
  try:
   with contextlib.redirect_stdout(io.StringIO()):m.check(q)
  except(AssertionError,KeyError,ValueError):passed.append(name)
  else:raise AssertionError('accepted '+name)
def change(q,path,fn):
 p=q/path;j=json.loads(p.read_text());fn(j);p.write_text(json.dumps(j))
trial('invented52-duty closure',lambda q:change(q,'audit.json',lambda j:j.update(closed_original_duties=52)))
trial('missing original duty',lambda q:change(q,'audit.json',lambda j:j['rows'].pop()))
trial('lost successor owner',lambda q:change(q,'audit.json',lambda j:j['rows'][0]['other_whole_hunk_owners'].pop()))
trial('silently closed review row',lambda q:change(q,'source-duty-review.json',lambda j:j['rows'][0].update(status='DONE')))
trial('missing held provider',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'].pop()))
trial('wrong held bytes',lambda q:change(q,'final/held-source-isolation.json',lambda j:j['rows'][0].update(sha256='0'*64)))
trial('unbound compiled source',lambda q:change(q,'final/source-before-build.json',lambda j:j.update({'app/core/gimpdynamicsoutput.c':'0'*64})))
trial('wrong committed source identity',lambda q:change(q,'committed-source-proof.json',lambda j:j['rows'][0].update(git_blob='0'*40)))
trial('different recorded commit',lambda q:change(q,'committed-source-proof.json',lambda j:j.update(commit='0'*40)))
trial('missing whole-unit equivalence',lambda q:change(q,'final/current-equivalence.json',lambda j:j['whole_equal'].pop()))
trial('fake relevant function',lambda q:change(q,'final/current-equivalence.json',lambda j:j['functions'][0].update(body='irrelevant')))
trial('old native test missing',lambda q:change(q,'final/normal-dynamics.json',lambda j:j.update(stdout=j['stdout'].replace('ok 3 /painter/dynamics/roundtrip-copy','missing'))))
trial('neutral real case missing',lambda q:change(q,'final/normal-neutral.json',lambda j:j.update(stdout='unrelated property passed')))
trial('native runtime failed',lambda q:change(q,'final/normal-dynamics.json',lambda j:j.update(result='FAIL')))
trial('native budget raised',lambda q:change(q,'final/normal-neutral.json',lambda j:j.update(duration=61)))
trial('portrait changed',lambda q:(q/'final/normal-neutral.log').write_text((q/'final/normal-neutral.log').read_text().replace('1080x1920','1920x1080')))
trial('sanitizer model failed',lambda q:change(q,'final/five-c-units.json',lambda j:j.update(exit_code=1)))
trial('sanitizer neutral failed',lambda q:change(q,'final/sanitizers.json',lambda j:j.update(exit_code=1)))
trial('stale sanitizer source',lambda q:change(q,'final/sanitizers.json',lambda j:j['sources_sha256'].update({'app/core/gimpdynamicsoutput.c':'0'*64})))
trial('uninstrumented neutral link',lambda q:change(q,'final/sanitizers.json',lambda j:j.update(link_command=[s.replace('/dynamics-sanitizers/app_core_libappcore.a','/app/core/libappcore.a')for s in j['link_command']])))
trial('disabled sanitizer',lambda q:change(q,'final/sanitizers.json',lambda j:j['link_command'].append('-fno-sanitize=undefined')))
trial('neutral mutant accepted',lambda q:change(q,'final/neutral-causal-control/result.json',lambda j:j.update(result='OK')))
trial('mutant changed unrelated provider',lambda q:change(q,'final/neutral-causal-control/source-before-build.json',lambda j:j.update({'app/core/gimpdynamics.c':'0'*64})))
trial('restoration failed',lambda q:change(q,'final/neutral-causal-control/restored-result.json',lambda j:j.update(result='FAIL')))
print(json.dumps({'status':'PASS','negative_controls':passed},indent=2))
