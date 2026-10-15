from pathlib import Path
import json,hashlib,shutil,subprocess,re
b=Path(__file__).resolve().parent;d=b/'deliverables/migration/fixtures/legacy-layer-operations';d.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def fields(line):return dict(re.findall(r'(\w+)=([^\s]+)',line))
observations={}
for scene in ['clone','filter']:
 out=d/scene;out.mkdir(exist_ok=True)
 for p in (b/'captures'/scene).iterdir():
  if p.is_file():shutil.copy2(p,out/p.name)
  elif p.name.startswith('independent-'):
   dest=out/p.name;dest.mkdir(exist_ok=True)
   for name in ['reopen.scm','reopen.log','exit-code']:shutil.copy2(p/name,dest/name)
 lines=(out/'capture.log').read_text().splitlines();trace=[x for x in lines if x.startswith(('STATE ','OBJECT ','EVENT ','PIXELS ','ARG ','ACTION ','RESULT ','HISTORY ','SAVE ','REGISTER ','OPERATIONS_COMPLETE'))]
 observations[scene]={'exit_code':int((out/'exit-code').read_text()),'trace':trace,'states':[fields(x) for x in trace if x.startswith('STATE ')],'objects':[fields(x) for x in trace if x.startswith('OBJECT ')],'pixels':[fields(x) for x in trace if x.startswith('PIXELS ')],'events':[fields(x) for x in trace if x.startswith('EVENT ')],'args':[fields(x) for x in trace if x.startswith('ARG ')],'independent_reopens':{p.name:int((p/'exit-code').read_text()) for p in out.glob('independent-*')}}
 repeat=b/'repeat-captures'/scene
 if (repeat/'exit-code').exists():
  r=out/'repeat';r.mkdir(exist_ok=True)
  for name in ['capture.log','exit-code']:shutil.copy2(repeat/name,r/name)
  records=[fields(x) for x in (repeat/'capture.log').read_text().splitlines() if x.startswith('PIXELS ')]
  comparisons=[]
  for a in observations[scene]['pixels']:
   matching=[x for x in records if x['step']==a['step'] and x['id']==a['id']];rp=repeat/f"{a['step']}.{a['id']}.pixels";p=out/rp.name
   comparisons.append({'step':a['step'],'id':a['id'],'primary_sha256':sha(p),'repeat_sha256':sha(rp),'equal':p.read_bytes()==rp.read_bytes(),'repeat_log_hash_matches':len(matching)==1 and matching[0]['sha256']==sha(rp)})
  (r/'comparison.json').write_text(json.dumps(comparisons,indent=2)+'\n')
(d/'observations.json').write_text(json.dumps(observations,indent=2)+'\n')
for name in ['capture-layer-operations.c','build.sh','run.sh','independent.sh','build.log','binary.sha256','original-binary-after.sha256','README.md','source-notes.md','archive-provenance.json','validation.log','package.py','collect-provenance.py','REPRODUCE.md']:
 if (b/name).exists():shutil.copy2(b/name,d/name)
patch=subprocess.check_output(['git','diff','--binary','HEAD'],cwd=b/'gimp');(d/'source-overlay.patch').write_bytes(patch)
files=['app/core/gimpclonelayer.cpp','app/core/gimpgrouplayer.c','app/core/gimplayer.c','app/core/gimpitem.c','app/core/gimpimage.c','app/core/gimpimage-undo.c','app/core/gimplayerundo.c','app/core/gimpfilterlayer.cpp','app/tests/test-xcf.c']
sources=[]
for f in files:
 pinned=subprocess.check_output(['git','show','HEAD:'+f],cwd=b/'gimp');sources.append({'path':f,'pinned_sha256':hashlib.sha256(pinned).hexdigest(),'capture_sha256':sha(b/'gimp'/f),'unchanged':pinned==(b/'gimp'/f).read_bytes()})
(d/'source-provenance.json').write_text(json.dumps(sources,indent=2)+'\n')
report={'schema_version':1,'task':'02.011','source_commit':'afa43fae3e920210146abed514f136fd49f671b5','capture_date':'2026-10-15','copied_tree_from':str(b.parent/'legacy-composite-20261015/gimp'),'isolated_source_and_build':str(b/'gimp'),'capture_binary_sha256':sha(b/'gimp/app/tests/.libs/test-xcf'),'reference_installed_binary_sha256':sha(b.parent/'legacy-filter-positive-20261015/build/prefix/bin/gimp-2.8'),'reference_install_untouched':sha(b.parent/'legacy-filter-positive-20261015/build/prefix/bin/gimp-2.8')=='85bc4733504509eb7b5a215cd0d19f6b6af5c4a23cd6112987de92acf3d230a1','reference_build':'migration/baseline/legacy/replay-20261015/report.json','scope':'Synthetic headless original-core operations. No physical UI/tablet claim; no GIMP3 parity claim. Filter plug-in-edge XCF reopen failures are preserved.','commands':['bash legacy-layer-operations-20261015/build.sh','bash legacy-layer-operations-20261015/run.sh','bash legacy-layer-operations-20261015/run.sh repeat-captures','bash legacy-layer-operations-20261015/independent.sh'],'state_counts':{s:len(v['states']) for s,v in observations.items()},'pixel_count':sum(len(v['pixels']) for v in observations.values()),'completed':{s:v['exit_code']==0 and f'OPERATIONS_COMPLETE scene={s}' in v['trace'] for s,v in observations.items()}}
(d/'capture-report.json').write_text(json.dumps(report,indent=2)+'\n')
if (b/'patch-check.log').exists():shutil.copy2(b/'patch-check.log',d/'patch-check.log')
if (b/'dependencies.txt').exists():shutil.copy2(b/'dependencies.txt',d/'dependencies.txt')
manifest=[{'path':str(p.relative_to(d)),'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(d.rglob('*')) if p.is_file() and p.name!='manifest.json']
(d/'manifest.json').write_text(json.dumps({'schema_version':1,'files':manifest},indent=2)+'\n')
print(json.dumps(report,indent=2))
