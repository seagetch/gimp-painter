set -euo pipefail
source /workspace/shared/gimp-clean-aggregate/env.sh
exec 9>/workspace/shared/gimp-clean-aggregate/build.lock
flock 9
src=/workspace/shared/gimp-clean-aggregate/source
build=/workspace/shared/gimp-clean-aggregate/build
out=/workspace/shared/filter-attributes-proof
mkdir -p "$out"
python3 - <<'PY'
import pathlib,json,hashlib,subprocess
r=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-attributes-proof');sha=subprocess.check_output(['git','rev-parse','2ef2dd32ee'],cwd=r,text=True).strip()
prior=json.loads(pathlib.Path('/workspace/shared/preset-help-successor/green/source-manifest.json').read_text());expected=prior['complete_source_sha256']
for p,h in expected.items():assert hashlib.sha256((s/p).read_bytes()).hexdigest()==h,p
paths=set(json.loads(pathlib.Path('/workspace/shared/filter-property-proof/source-after.json').read_text())['sha256']);paths.update(prior['input_sha256'])
p='app/tests/test-gimp-filter-layer.c';t=(s/p).read_text();assert 'test-filter-attributes-contract.inc' not in t;n=t.replace('#include "test-filter-pickable-contract.inc"','#include "test-filter-pickable-contract.inc"\n#include "test-filter-attributes-contract.inc"').replace('  ADD (filter_flip_legacy_reset_contract);','  ADD (filter_flip_legacy_reset_contract);\n  ADD (filter_gaussian_attributes_contract);');assert n==(r/p).read_text();(s/p).write_text(n)
p='app/tests/test-filter-attributes-contract.inc';(s/p).write_bytes((r/p).read_bytes());paths.add(p);(o/'source-before.json').write_text(json.dumps({'committed_base':sha,'scope':'two attribute fixture overlays after sealed readiness/property/help predecessors;not whole HEAD','predecessor':'preset-help-successor/green/source-manifest.json','sha256':{p:hashlib.sha256((s/p).read_bytes()).hexdigest() for p in sorted(paths)}},indent=2)+'\n')
PY
ninja -C "$build" app/tests/gimp-filter-layer > "$out/build.log" 2>&1
export LD_LIBRARY_PATH=$(find "$build" -maxdepth 1 -type d -name 'libgimp*' -printf '%p:')${LD_LIBRARY_PATH:-}
python3 - <<'PY'
import subprocess,os,json,pathlib,time
b='/workspace/shared/gimp-clean-aggregate/build';s='/workspace/shared/gimp-clean-aggregate/source';env=dict(os.environ);env.update(GIMP_TESTING_ABS_TOP_SRCDIR=s,GIMP_TESTING_ABS_TOP_BUILDDIR=b,GIMP_TESTING_PLUGINDIRS=b+'/plug-ins/common',UI_TEST='yes');cmd=[b+'/app/tests/gimp-filter-layer','-p','/gimp-filter-layer/filter_gaussian_attributes_contract'];t=time.time();p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=90);pathlib.Path('/workspace/shared/filter-attributes-proof/native.json').write_text(json.dumps({'command':cmd,'exit_code':p.returncode,'duration':time.time()-t,'stdout':p.stdout,'stderr':p.stderr},indent=2)+'\n');print(p.stdout);print(p.stderr);assert p.returncode==0
PY
python3 "$src/migration/tests/run_filter_layer_sanitizers.py" "$build" --report "$out/sanitizer.json" --test-path /gimp-filter-layer/filter_gaussian_attributes_contract
python3 - <<'PY'
import pathlib,json,hashlib
s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-attributes-proof');j=json.loads((o/'source-before.json').read_text());j['changed_after_run']=[p for p,h in j['sha256'].items() if hashlib.sha256((s/p).read_bytes()).hexdigest()!=h];(o/'source-after.json').write_text(json.dumps(j,indent=2)+'\n');assert not j['changed_after_run']
PY
