#!/usr/bin/env python3
"""Reject altered claims, omitted transitions and weakened original obligations."""
import importlib.util,json,pathlib,shutil,tempfile,hashlib
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
spec=importlib.util.spec_from_file_location('display_check',P/'check-display.py');c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
def edit_json(e,name,fn):
 p=e/name;v=json.loads(p.read_text());fn(v);p.write_text(json.dumps(v,indent=2)+'\n')
c.check(R,P)  # A valid positive baseline is required before any mutation.
controls=[]
def add(name,path,fn):controls.append((name,lambda root,e:edit_json(e,path,fn)))
for key,value in [('criterion','weakened'),('original_task_complete',True),('closed_original_duties',193),('whole_port_complete',True),('old_runtime_executed',True),('independent_old_transform_claim',True),('old_style_parity_claim',True),('guide_channel_tolerance',3),('classified_border_maximum',4),('unknown_outside_pixels_allowed',1),('new_ui_oncanvas',False),('historical_event_setup_unchanged',False)]:
 add(key,'manifest.json',lambda v,k=key,x=value:v.update({k:x}))
add('landscape','manifest.json',lambda v:v['xvfb'].update(width=1920,height=1080))
add('leak-claim','manifest.json',lambda v:v['sanitizer'].update(leaks=True))
add('audit-closure','audit.json',lambda v:v.update(status='DONE'))
add('duty-loss','audit.json',lambda v:v['rows'].pop())
add('owner-theft','audit.json',lambda v:v['rows'][0]['other_whole_hunk_owners'].pop())
add('criterion-change','audit.json',lambda v:v['rows'][0].update(acceptance='different'))
add('timeout','final/normal-angle0.json',lambda v:v[0].update(duration=61))
add('failed-exit','final/normal-angle0.json',lambda v:v[0].update(returncode=1))
add('elf-mismatch','final/normal-angle0.json',lambda v:v[0].update(executable_sha256='0'*64))
add('unbound-source','final/normal-angle0.json',lambda v:v[0]['source_sha256'].pop('app/tests/test-painter-perspective-path-oracle.cpp'))
add('case-loss','final/normal-angle0.json',lambda v:v[0].update(cases=0))
add('event-loss','final/normal-angle0.json',lambda v:v[0].update(stdout=v[0]['stdout'].replace('perspective damage angle=0','omitted damage angle=0',1)))
add('global-loss','final/normal-angle0.json',lambda v:v[0].update(stdout=v[0]['stdout'].replace('perspective global all=','omitted global all=',1)))
add('unknown-pixel','final/normal-angle0.json',lambda v:v[0].update(stdout=v[0]['stdout'].replace('unexplained=0','unexplained=1',1)))
add('residual-pixel','final/normal-angle0.json',lambda v:v[0].update(stdout=v[0]['stdout'].replace('residual=0','residual=1',1)))
add('literal-old-path-difference','final/normal-geometry.json',lambda v:v[0].update(stdout=v[0]['stdout'].replace('mismatched=0','mismatched=1',1)))
add('old-original-case-loss','final/normal-original.json',lambda v:v[0].update(stdout=v[0]['stdout'].replace('/perspective/model_bounds','/perspective/not-original',1)))
add('held-clone-loss','final/held-source-isolation.json',lambda v:v['rows'].pop())
add('sanitizer-disabled','final/build-provenance.json',lambda v:v['sanitizer']['options'].update(b_sanitize='none'))
add('red-exit-relabel','diagnostics/old-path-red/result.json',lambda v:v.update(returncode=0))
add('negative-control-relabel','negative-missing-damage/result.json',lambda v:v.update(returncode=0))
controls.append(('source-drift',lambda root,e:(root/'app/tests/test-painter-perspective-damage.inc').write_text('altered source')))
controls.append(('old-source-drift',lambda root,e:(root/'migration/fixtures/legacy-perspective-draw/draw-source.inc').write_text('altered old function')))
with tempfile.TemporaryDirectory(prefix='perspective-evidence-controls-') as directory:
 root=pathlib.Path(directory)/'root';m=json.loads((P/'manifest.json').read_text())
 for p in m['owned_source_sha256']:
  out=root/p;out.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(R/p,out)
 for name,mutation in controls:
  e=pathlib.Path(directory)/'evidence';shutil.copytree(P,e,ignore=shutil.ignore_patterns('__pycache__'))
  mutation(root,e)
  manifest=json.loads((e/'manifest.json').read_text())
  manifest['artifact_sha256']={p:hashlib.sha256((e/p).read_bytes()).hexdigest() for p in manifest['artifact_sha256']}
  (e/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
  try:c.check(root,e)
  except (AssertionError,KeyError,ValueError,FileNotFoundError):pass
  else:raise AssertionError('control accepted: '+name)
  shutil.rmtree(e)
  for p in m['owned_source_sha256']:shutil.copy2(R/p,root/p)
print(json.dumps(dict(status='PASS',rejected=len(controls),controls=[x[0] for x in controls])))
