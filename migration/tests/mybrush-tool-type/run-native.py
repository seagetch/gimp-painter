from pathlib import Path
import subprocess,json,hashlib,re
root=Path('/workspace/scratch/5b5281e79681/gtk-binding-recovery-20261010/worktree');out=Path(__file__).resolve().parent;build=Path('/workspace/scratch/5b5281e79681/gimp-native-restore-20261010/build-http')
sources=['app/tools/'+n for n in ['gimppaintermybrushtool.cpp','gimppaintermybrushtool.h','gimppaintermybrushtool.hpp','gimp-tools.c','gimptool.c','gimpdrawtool.c','gimpcolortool.c','tool_manager.c']]
sources+=['app/widgets/'+n for n in ['gimppaintermybrusheditor.cpp','gimppaintermybrusheditor.h','gimppaintermybrusheditor.hpp']]
sources+=['app/paint/painter-mypaint-surface/'+n for n in ['gimp-painter-options.cpp','gimp-painter-options.h','gimp-painter-session.cpp','gimp-painter-session.h','paint-core.cpp','paint-core.hpp']]
sources+=['app/paint/painter-mypaint/'+n for n in ['resource.cpp','engine.cpp']]
sources+=['app/painter/'+n for n in ['binding-store.cpp','binding-store.hpp','gimp-painter-binding.cpp','connection.hpp','object-ref.hpp','source.hpp','boundary.hpp']]
sources+=['app/tests/'+n for n in ['test-painter-mypaint-tool.cpp','test-painter-mypaint-editor.cpp','meson.build']]
seals={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sources}
jobs=[]
for name in ['registration-gate','standalone-lifecycle','stationary-equal-time-undo','cancel','halt-last-ref-motion','public-press-last-ref','last-ref-without-halt']:
 jobs.append(('tool-'+name,['app/tests/painter-mypaint-tool','-p','/painter-tool/'+name]))
for name in ['06-replace-close','09-close-during-refresh','13-owner-destruction-orders']:
 jobs.append(('editor-'+name,['app/tests/painter-mypaint-editor','-p','/painter-editor/'+name]))
results=[]
for label,args in jobs:
 binary=build/args[0];argv=[str(binary),*args[1:]]
 r=subprocess.run(argv,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
 (out/(label+'.log')).write_text(r.stdout);cases=re.findall(r'^ok \d+ (.+)$',r.stdout,re.M)
 d={'name':label,'argv':argv,'exit_code':r.returncode,'status':'PASS' if r.returncode==0 else 'FAIL','cases':cases,'stdout':r.stdout,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'source_sha256':seals};results.append(d)
 (out/(label+'.json')).write_text(json.dumps(d,indent=2)+'\n');print(json.dumps({'name':label,'exit_code':r.returncode,'passed_cases':len(cases)}),flush=True)
 if r.returncode:print(r.stdout[-4000:],flush=True);break
changed=[p for p,d in seals.items() if hashlib.sha256((root/p).read_bytes()).hexdigest()!=d];assert not changed,changed
(out/'native-summary.json').write_text(json.dumps({'results':[{k:r[k] for k in ('name','status','exit_code','cases','binary_sha256')} for r in results],'source_sha256':seals,'changed_during_run':changed,'scope':'Native GTK registration/tool lifetime and editor/options destruction orders; no new full renderer/platform/sanitizer acceptance'},indent=2)+'\n')
assert len(results)==10 and all(r['exit_code']==0 and len(r['cases'])==1 for r in results)
