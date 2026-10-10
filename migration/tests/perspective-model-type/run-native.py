from pathlib import Path
import hashlib,json,subprocess,re
root=Path('/workspace/scratch/5b5281e79681/gtk-binding-recovery-20261010/worktree');out=Path(__file__).resolve().parent;binary=Path('/workspace/scratch/5b5281e79681/gimp-native-restore-20261010/build-http/app/tests/painter-perspective')
sources=['app/core/'+n for n in ['gimpperspectiveguide.cpp','gimpperspectiveguide.h','gimpperspectiveguide.hpp','gimpimage-perspective-guide.c','gimpimage-perspective-guide.h','gimpperspectiveguideundo.cpp','gimpimage.c','gimpimage-private.h']]
sources+=['app/painter/'+n for n in ['binding-store.cpp','binding-store.hpp','gimp-painter-binding.cpp','boundary.hpp','object-ref.hpp']]
sources+=['app/tests/test-painter-perspective.c','app/tests/meson.build','app/tools/gimpperspectiveguidetool.cpp','app/display/gimpcanvasperspectiveguide.cpp','app/display/gimpdisplayshell-tool-events.c','migration/fixtures/legacy-perspective/runtime-capture.log','migration/fixtures/legacy-perspective/snap-angle-source.inc']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
seals={p:sha(root/p) for p in sources};binary_hash=sha(binary)
r=subprocess.run([str(binary)],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
cases=re.findall(r'^ok \d+ (.+)$',r.stdout,re.M);changed=[p for p,h in seals.items() if sha(root/p)!=h]
result=dict(name='native-perspective-model-type',exit_code=r.returncode,status='PASS' if r.returncode==0 and len(cases)==12 and not changed else 'FAIL',cases=cases,argv=[str(binary)],binary_sha256=binary_hash,source_sha256=seals,changed_during_run=changed,stdout=r.stdout,scope='Original model type/image ownership and removed virtual slot, plus existing core regression cases. Source oracle and previously captured runtime fixture distinguished.')
(out/'native.json').write_text(json.dumps(result,indent=2)+'\n');(out/'native.log').write_text(r.stdout);print(json.dumps({k:result[k] for k in ['name','exit_code','status','cases','changed_during_run']}))
assert result['status']=='PASS',r.stdout[-5000:]
