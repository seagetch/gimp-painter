#!/usr/bin/env python3
import importlib.util,json,pathlib,tempfile,shutil
E=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('checker',E/'check-space.py');c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
results=[]
def edit(e,name,fn):
 p=e/name;r=json.loads(p.read_text());fn(r);p.write_text(json.dumps(r))
def run(name,mutate,reseal=False):
 with tempfile.TemporaryDirectory(prefix='space-evidence-negative-') as d:
  e=pathlib.Path(d)/'e';shutil.copytree(E,e,ignore=shutil.ignore_patterns('__pycache__'));mutate(e)
  if reseal:
   m=json.loads((e/'manifest.json').read_text())
   for p in m['artifact_sha256']:m['artifact_sha256'][p]=c.sha((e/p).read_bytes())
   (e/'manifest.json').write_text(json.dumps(m))
  try:c.check(c.R,e)
  except (ValueError,KeyError,StopIteration) as error:results.append({'name':name,'rejected':True,'reason':str(error)});return
  raise AssertionError('accepted '+name)
run('weaken original criterion',lambda e:edit(e,'manifest.json',lambda r:r.update(original_criterion='middle only')))
run('claim skipped old runtime',lambda e:edit(e,'manifest.json',lambda r:r['scope'].update(old_runtime='PASS')))
run('wrong source',lambda e:edit(e,'manifest.json',lambda r:r['source_sha256'].update({'app/display/gimpdisplayshell-tool-events.c':'0'*64})))
run('timeout resealed',lambda e:edit(e,'sanitizer-space.json',lambda r:r.update(duration=61)),True)
run('lost modifier matrix resealed',lambda e:edit(e,'normal-space.json',lambda r:r.update(stdout=r['stdout'].replace('# old applied zoom scene=20','# removed scene=20'))),True)
run('missing deferred case resealed',lambda e:edit(e,'normal-space.json',lambda r:r.update(stdout=r['stdout'].replace('/old-space/deferred-release','/old-space/not-tested'))),True)
run('raw environment injected',lambda e:edit(e,'normal-space.json',lambda r:r.update(env={})),True)
run('work item erased',lambda e:edit(e,'work-item-audit.json',lambda r:r['rows'].pop()),True)
run('original work criterion weakened',lambda e:edit(e,'work-item-audit.json',lambda r:r['rows'][0].update(original_acceptance='declarations only')),True)
run('other whole-hunk duty erased',lambda e:edit(e,'work-item-audit.json',lambda r:r['rows'][0].update(other_whole_hunk_owners=[])),True)
run('work marked accepted without proof',lambda e:edit(e,'work-item-audit.json',lambda r:r['rows'][-1].update(status='TODO')),True)
run('red relabelled',lambda e:edit(e,'expected-red.json',lambda r:r.update(result='OK')),True)
run('wrong portrait image',lambda e:(e/'portrait-oncanvas-space-m0.png').write_bytes(b'wrong'),True)
# Independent immutable old-source controls: changing either the extracted
# case or adapter arithmetic must reject before any current-code comparison.
F=c.R/'migration/fixtures/legacy-navigation-space';spec=importlib.util.spec_from_file_location('source_check',F/'verify.py');v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
for file,old,new in [('oracle.inc','legacy_pan_inverse','native_inverse'),('oracle.inc','legacy_pan_begin','native_begin'),('pan-case.c','start_x = coords.x','start_x = (gint) coords.x')]:
 with tempfile.TemporaryDirectory(prefix='space-source-negative-') as d:
  f=pathlib.Path(d)/'f';shutil.copytree(F,f,ignore=shutil.ignore_patterns('__pycache__'));p=f/file;s=p.read_text();assert old in s;p.write_text(s.replace(old,new,1))
  try:v.check(f)
  except AssertionError:results.append({'name':file+' '+old,'rejected':True});continue
  raise AssertionError('accepted source mutant')
positive=c.check();v.check();(E/'negative-controls.json').write_text(json.dumps({'negative':results,'positive':positive,'source_positive':v.check()},indent=2)+'\n');print(json.dumps({'negative_rejected':len(results),'positive':positive}))
