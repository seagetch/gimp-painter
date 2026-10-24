set -euo pipefail
source /workspace/shared/gimp-clean-aggregate/env.sh
exec 9>/workspace/shared/gimp-clean-aggregate/build.lock
flock 9
src=/workspace/shared/gimp-clean-aggregate/source
build=/workspace/shared/gimp-clean-aggregate/build
out=/workspace/shared/filter-matrix-red
mkdir -p "$out"
python3 - <<'PY'
import pathlib,json,hashlib,tarfile,io
r=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-matrix-red');prior=json.loads(pathlib.Path('/workspace/shared/filter-rotate-green/source-after.json').read_text())
for p,h in prior['sha256'].items():assert hashlib.sha256((s/p).read_bytes()).hexdigest()==h,p
p='app/tests/test-gimp-filter-layer.c';t=(s/p).read_text();n=t.replace('#include "test-filter-pickable-contract.inc"','#include "test-filter-pickable-contract.inc"\n#include "test-filter-matrix-contract.inc"').replace('  ADD (filter_flip_legacy_reset_contract);','  ADD (filter_flip_legacy_reset_contract);\n  ADD (filter_gaussian_same_extent_matrix_contract);');assert n!=t;(s/p).write_text(n)
p='app/tests/test-filter-matrix-contract.inc';(s/p).write_bytes((r/p).read_bytes());paths=list(prior['sha256'])+[p];hashes={p:hashlib.sha256((s/p).read_bytes()).hexdigest() for p in paths};(o/'source-before.json').write_text(json.dumps({'predecessor':'filter-rotate-green/source-after.json','scope':'test-only bounded matrix overlay;no provider mutation','sha256':hashes},indent=2)+'\n')
with tarfile.open(o/'sources.tar.gz','w:gz') as tar:
 for p in paths:
  b=(s/p).read_bytes();ti=tarfile.TarInfo(p);ti.size=len(b);ti.mtime=0;tar.addfile(ti,io.BytesIO(b))
PY
ninja -C "$build" app/tests/gimp-filter-layer > "$out/build.log" 2>&1
export LD_LIBRARY_PATH=$(find "$build" -maxdepth 1 -type d -name 'libgimp*' -printf '%p:')${LD_LIBRARY_PATH:-}
python3 - <<'PY'
import subprocess,os,json,pathlib,time,resource,hashlib
b='/workspace/shared/gimp-clean-aggregate/build';s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-matrix-red');env=dict(os.environ);env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(s),GIMP_TESTING_ABS_TOP_BUILDDIR=b,GIMP_TESTING_PLUGINDIRS=b+'/plug-ins/common',UI_TEST='yes');cmd=[b+'/app/tests/gimp-filter-layer','-p','/gimp-filter-layer/filter_gaussian_same_extent_matrix_contract'];t=time.time();p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=90,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)));(o/'native.json').write_text(json.dumps({'command':cmd,'exit_code':p.returncode,'duration':time.time()-t,'stdout':p.stdout,'stderr':p.stderr},indent=2)+'\n');print(p.stdout);print(p.stderr);print('EXIT',p.returncode)
j=json.loads((o/'source-before.json').read_text());j['changed_after_run']=[p for p,h in j['sha256'].items() if hashlib.sha256((s/p).read_bytes()).hexdigest()!=h];(o/'source-after.json').write_text(json.dumps(j,indent=2)+'\n');assert not j['changed_after_run']
PY
