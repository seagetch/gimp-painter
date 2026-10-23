#!/usr/bin/env python3
import importlib.util,json,pathlib,tempfile,shutil
E=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('checker',E/'check-zoom.py');c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
results=[]
def run(name,mutate,reseal=False):
 with tempfile.TemporaryDirectory(prefix='zoom-evidence-negative-') as d:
  e=pathlib.Path(d)/'e';shutil.copytree(E,e,ignore=shutil.ignore_patterns('__pycache__'))
  mutate(e)
  if reseal:
   m=json.loads((e/'manifest.json').read_text())
   for p in m['artifact_sha256']:m['artifact_sha256'][p]=c.sha((e/p).read_bytes())
   (e/'manifest.json').write_text(json.dumps(m))
  try:c.check(c.R,e)
  except (ValueError,KeyError,StopIteration) as error:results.append({'name':name,'rejected':True,'reason':str(error)});return
  raise AssertionError('accepted '+name)
def edit(e,name,fn):
 p=e/name;r=json.loads(p.read_text());fn(r);p.write_text(json.dumps(r))
run('weaken criterion',lambda e:edit(e,'manifest.json',lambda r:r.update(criterion='factor only')))
run('erase dependency',lambda e:edit(e,'manifest.json',lambda r:r.update(dependencies=[])))
run('old runtime false claim',lambda e:edit(e,'manifest.json',lambda r:r['scope'].update(old_runtime='PASS')))
run('wrong source',lambda e:edit(e,'manifest.json',lambda r:r['source_sha256'].update({'app/display/gimpdisplayshell-scale.c':'0'*64})))
run('timeout resealed',lambda e:edit(e,'sanitizer-zoom.json',lambda r:r.update(duration=61)),True)
run('missing extreme case resealed',lambda e:edit(e,'normal-zoom.json',lambda r:r.update(stdout=r['stdout'].replace('/old-zoom/extreme-pointer','/old-zoom/relabelled'))),True)
run('receipt source mismatch resealed',lambda e:edit(e,'normal-original.json',lambda r:r.update(source_sha256={})),True)
run('raw environment injection resealed',lambda e:edit(e,'normal-zoom.json',lambda r:r.update(env={})),True)
run('failure relabelled resealed',lambda e:edit(e,'extreme-pointer-render-failure.json',lambda r:r.update(result='OK')),True)
run('wrong screenshot resealed',lambda e:(e/'portrait-oncanvas-zoom-m0.png').write_bytes(b'not a portrait'),True)
run('ordinary stipple comparison removed',lambda e:edit(e,'normal-zoom.json',lambda r:r.update(stdout=r['stdout'].replace('# layer stipple original-path pixels ','# removed '))),True)
run('header failure resealed',lambda e:edit(e,'headers.json',lambda r:r[0].update(result='FAIL')),True)
positive=c.check();(E/'negative-controls.json').write_text(json.dumps({'negative':results,'positive':positive},indent=2)+'\n');print(json.dumps({'negative_rejected':len(results),'positive':positive}))
