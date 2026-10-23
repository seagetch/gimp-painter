from pathlib import Path
import subprocess,json,hashlib,re,os
root=Path('/workspace/scratch/5b5281e79681/gtk-binding-recovery-20261010/worktree');out=Path(__file__).resolve().parent;build=Path('/workspace/scratch/5b5281e79681/gimp-native-restore-20261010/build-http')
sources=['app/paint/painter-mypaint-surface/gimp-painter-options.cpp', 'app/paint/painter-mypaint-surface/gimp-painter-options.h', 'app/paint/painter-mypaint-surface/gimp-painter-options.hpp', 'app/paint/gimppaintoptions.c', 'app/paint/gimppaintoptions.h', 'app/core/gimpcontext.c', 'app/core/gimppaintermybrush.cpp', 'app/paint/painter-mypaint/resource.cpp', 'app/paint/painter-mypaint/resource.hpp', 'app/paint/painter-mypaint/engine.cpp', 'app/paint/painter-mypaint/mapping.hpp', 'app/paint/painter-mypaint/tests/test-resource.cpp', 'app/paint/painter-mypaint/meson.build', 'app/paint/painter-brush-settings/brush-settings.json', 'app/paint/painter-brush-settings/generate.py', 'app/paint/painter-brush-settings/tests/test-data.h', 'app/paint/painter-brush-settings/tests/test-data.c', 'app/paint/painter-brush-settings/tests/test-data.cpp', 'app/painter/binding-store.cpp', 'app/painter/binding-store.hpp', 'app/painter/gimp-painter-binding.cpp', 'app/painter/connection.hpp', 'app/painter/object-ref.hpp', 'app/painter/boundary.hpp', 'app/tests/test-gimp-painter-options.cpp', 'app/tests/meson.build']
seals={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sources}
jobs=[('options',['app/tests/gimp-painter-options']),('resource',['app/paint/painter-mypaint/painter-mypaint-resource',str(root/'data/painter-mypaint-brushes')]),('metadata-c',['app/paint/painter-brush-settings/painter-brush-metadata-c']),('metadata-cpp',['app/paint/painter-brush-settings/painter-brush-metadata-cpp'])]
results=[]
for label,args in jobs:
 binary=build/args[0];argv=[str(binary),*args[1:]]
 r=subprocess.run(argv,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
 (out/(label+'.log')).write_text(r.stdout)
 cases=re.findall(r'^ok \d+ (.+)$',r.stdout,re.M)
 d={'name':label,'argv':argv,'exit_code':r.returncode,'status':'PASS' if r.returncode==0 else 'FAIL','cases':cases,'stdout':r.stdout,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'source_sha256':seals};results.append(d)
 (out/(label+'.json')).write_text(json.dumps(d,indent=2)+'\n');print(json.dumps({'name':label,'exit_code':r.returncode,'passed_cases':len(cases)}),flush=True)
 if r.returncode:print(r.stdout[-4000:],flush=True);break
changed=[p for p,d in seals.items() if hashlib.sha256((root/p).read_bytes()).hexdigest()!=d];assert not changed,changed
(out/'native-summary.json').write_text(json.dumps({'results':[{k:r[k] for k in ('name','status','exit_code','cases','binary_sha256')} for r in results],'source_sha256':seals,'changed_during_run':changed,'scope':'Native options/property/config and static metadata lookup recreation; no new sanitizer, legacy Reset oracle, grouped GUI or all-platform execution'},indent=2)+'\n')
assert len(results)==4 and all(r['exit_code']==0 for r in results)
