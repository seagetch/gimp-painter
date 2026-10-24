set -euo pipefail
source /workspace/shared/gimp-clean-aggregate/env.sh
exec 9>/workspace/shared/gimp-clean-aggregate/build.lock
flock 9
src=/workspace/shared/gimp-clean-aggregate/source
build=/workspace/shared/gimp-clean-aggregate/build
out=/workspace/shared/filter-matrix-green
mkdir -p "$out"
python3 - <<'PY'
import pathlib,json,hashlib,shutil
r=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-matrix-green');prior=json.loads(pathlib.Path('/workspace/shared/filter-matrix-red/source-after.json').read_text())
for p,h in prior['sha256'].items():assert hashlib.sha256((s/p).read_bytes()).hexdigest()==h,p
assert (s/'app/tests/test-gimp-filter-layer.c').read_bytes()==(r/'app/tests/test-gimp-filter-layer.c').read_bytes()
for p in ['app/core/gimpfilterlayer.cpp','app/tests/test-filter-flip-contract.inc']:(s/p).write_bytes((r/p).read_bytes())
j={'predecessor':'filter-matrix-red/source-after.json','scope':'narrow matrix provider override and flip pointer-contract update;test fixture unchanged','sha256':{p:hashlib.sha256((s/p).read_bytes()).hexdigest() for p in prior['sha256']}};(o/'source-before.json').write_text(json.dumps(j,indent=2)+'\n')
PY
ninja -C "$build" app/tests/gimp-filter-layer > "$out/build.log" 2>&1
export LD_LIBRARY_PATH=$(find "$build" -maxdepth 1 -type d -name 'libgimp*' -printf '%p:')${LD_LIBRARY_PATH:-}
python3 - <<'PY'
import subprocess,os,json,pathlib,time
b='/workspace/shared/gimp-clean-aggregate/build';s='/workspace/shared/gimp-clean-aggregate/source';env=dict(os.environ);env.update(GIMP_TESTING_ABS_TOP_SRCDIR=s,GIMP_TESTING_ABS_TOP_BUILDDIR=b,GIMP_TESTING_PLUGINDIRS=b+'/plug-ins/common',UI_TEST='yes');cmd=[b+'/app/tests/gimp-filter-layer']
for n in ['filter_gaussian_same_extent_matrix_contract','filter_gaussian_own_center_rotate_contract','filter_flip_legacy_reset_contract','filter_gaussian_restore_latest_lower_contract','filter_gaussian_running_duplicate_contract']:cmd+=['-p','/gimp-filter-layer/'+n]
t=time.time();p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=90);pathlib.Path('/workspace/shared/filter-matrix-green/native.json').write_text(json.dumps({'command':cmd,'exit_code':p.returncode,'duration':time.time()-t,'stdout':p.stdout,'stderr':p.stderr},indent=2)+'\n');print(p.stdout);print(p.stderr);assert p.returncode==0
PY
python3 "$src/migration/tests/run_filter_layer_sanitizers.py" "$build" --report "$out/sanitizer.json" --test-path /gimp-filter-layer/filter_gaussian_same_extent_matrix_contract --test-path /gimp-filter-layer/filter_gaussian_own_center_rotate_contract --test-path /gimp-filter-layer/filter_flip_legacy_reset_contract
python3 - <<'PY'
import json,pathlib,hashlib
s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-matrix-green');j=json.loads((o/'source-before.json').read_text());j['changed_after_run']=[p for p,h in j['sha256'].items() if hashlib.sha256((s/p).read_bytes()).hexdigest()!=h];(o/'source-after.json').write_text(json.dumps(j,indent=2)+'\n');assert not j['changed_after_run']
PY
