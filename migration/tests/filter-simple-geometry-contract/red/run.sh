set -euo pipefail
source /workspace/shared/gimp-clean-aggregate/env.sh
exec 9>/workspace/shared/gimp-clean-aggregate/build.lock
flock 9
src=/workspace/shared/gimp-clean-aggregate/source
build=/workspace/shared/gimp-clean-aggregate/build
out=/workspace/shared/filter-simple-geometry-red
mkdir -p "$out"
python3 - <<'PYS'
import pathlib,json,hashlib,tarfile,io
r=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-simple-geometry-red')
prior=json.loads(pathlib.Path('/workspace/shared/filter-matrix-green/source-after.json').read_text())['sha256'];enum=json.loads(pathlib.Path('/workspace/shared/options-enum-meson/source-overlay.json').read_text());prior.update(enum['owned_sha256']);prior.update(enum['shared_sha256'])
for p,h in prior.items():assert hashlib.sha256((s/p).read_bytes()).hexdigest()==h,p
names=['translate_noop','translate_changed','scale_same','scale_changed','resize_noop','resize_changed']
p='app/tests/test-gimp-filter-layer.c';t=(s/p).read_text();assert 'test-filter-simple-geometry-contract.inc' not in t;n=t.replace('#include "test-filter-pickable-contract.inc"','#include "test-filter-pickable-contract.inc"\n#include "test-filter-simple-geometry-contract.inc"').replace('  ADD (filter_flip_legacy_reset_contract);','  ADD (filter_flip_legacy_reset_contract);'+''.join('\n  ADD (filter_gaussian_'+x+');' for x in names));(s/p).write_text(n)
p='app/tests/test-filter-simple-geometry-contract.inc';(s/p).write_bytes((r/p).read_bytes());prior[p]='';hashes={p:hashlib.sha256((s/p).read_bytes()).hexdigest() for p in prior};(o/'source-before.json').write_text(json.dumps({'predecessor':'filter-matrix-green/source-after.json + exact options-enum-meson/source-overlay.json','scope':'six Gaussian geometry tests only;no provider mutation','sha256':hashes},indent=2)+'\n')
with tarfile.open(o/'sources.tar.gz','w:gz') as tar:
 for p in hashes:
  b=(s/p).read_bytes();ti=tarfile.TarInfo(p);ti.size=len(b);ti.mtime=0;tar.addfile(ti,io.BytesIO(b))
PYS
ninja -C "$build" app/tests/gimp-filter-layer > "$out/build.log" 2>&1
export LD_LIBRARY_PATH=$(find "$build" -maxdepth 1 -type d -name 'libgimp*' -printf '%p:')${LD_LIBRARY_PATH:-}
python3 - <<'PYS'
import subprocess,os,json,pathlib,time,resource,hashlib
b='/workspace/shared/gimp-clean-aggregate/build';s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-simple-geometry-red');env=dict(os.environ);env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(s),GIMP_TESTING_ABS_TOP_BUILDDIR=b,GIMP_TESTING_PLUGINDIRS=b+'/plug-ins/common',UI_TEST='yes');results=[]
for name in ['translate_noop','translate_changed','scale_same','scale_changed','resize_noop','resize_changed']:
 cmd=[b+'/app/tests/gimp-filter-layer','-p','/gimp-filter-layer/filter_gaussian_'+name];t=time.time();p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=90,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)));results.append({'case':name,'command':cmd,'exit_code':p.returncode,'duration':time.time()-t,'stdout':p.stdout,'stderr':p.stderr});print(name,p.returncode,p.stdout,p.stderr,flush=True)
(o/'native.json').write_text(json.dumps(results,indent=2)+'\n');j=json.loads((o/'source-before.json').read_text());j['changed_after_run']=[p for p,h in j['sha256'].items() if hashlib.sha256((s/p).read_bytes()).hexdigest()!=h];(o/'source-after.json').write_text(json.dumps(j,indent=2)+'\n');assert not j['changed_after_run']
PYS
