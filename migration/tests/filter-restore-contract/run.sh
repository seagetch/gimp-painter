set -euo pipefail
source /workspace/shared/gimp-clean-aggregate/env.sh
exec 9>/workspace/shared/gimp-clean-aggregate/build.lock
flock 9
src=/workspace/shared/gimp-clean-aggregate/source
build=/workspace/shared/gimp-clean-aggregate/build
repo=/workspace/scratch/0015fc5d1a03/gimp-painter
out=/workspace/shared/filter-restore-result
mkdir -p "$out"
python3 - <<'PY'
import pathlib,subprocess,hashlib,json
r=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-restore-result');sha='a84cd0b051e7b648ec9519ede82ae9541c44ea1c'
paths=['app/core/gimpfilterlayer.cpp','app/core/gimpviewable.c','app/core/gimppaintermybrush.cpp','app/widgets/gimpviewrenderer.c','app/widgets/gimpcellrendererpopup.cpp','app/display/gimppaintercanvasui.cpp','app/tools/gimpfillbrushtool.cpp','app/tools/gimpbucketfillbrushtool.h','app/tools/gimp-tools.c','app/tests/test-gimp-filter-layer.c','app/tests/test-filter-pickable-contract.inc','app/tests/test-filter-flip-contract.inc','app/core/gimpclonelayer.cpp','app/painter/filter-gauss.cpp','app/painter/filter-gauss-kernel-private.hpp','migration/tests/run_filter_layer_sanitizers.py']
for p in paths:assert (s/p).read_bytes()==subprocess.check_output(['git','show',('109b2963e1' if p=='app/tests/test-gimp-filter-layer.c' else sha)+':'+p],cwd=r),p
options=json.loads(pathlib.Path('/workspace/shared/options-gui-entry/green/source-manifest.json').read_text())
for p,h in options['input_sha256'].items():assert hashlib.sha256((s/p).read_bytes()).hexdigest()==h,p
paths=list(dict.fromkeys(paths+list(options['input_sha256'])))
(o/'options-overlay-manifest.json').write_text(json.dumps(options,indent=2)+'\n')
p='app/tests/test-gimp-filter-layer.c';original=(s/p).read_text();modified=original.replace('#include "test-filter-pickable-contract.inc"','#include "test-filter-pickable-contract.inc"\n#include "test-filter-restore-contract.inc"').replace('  ADD (filter_flip_legacy_reset_contract);','  ADD (filter_flip_legacy_reset_contract);\n  ADD (filter_gaussian_restore_latest_lower_contract);');assert original!=modified;(s/p).write_text(modified)
p='app/tests/test-filter-restore-contract.inc';(s/p).write_bytes((r/p).read_bytes());paths.append(p)
(o/'source-before.json').write_text(json.dumps({'committed_base':sha,'overlays':['app/tests/test-gimp-filter-layer.c',p],'sha256':{p:hashlib.sha256((s/p).read_bytes()).hexdigest() for p in paths}},indent=2)+'\n')
PY
ninja -C "$build" app/tests/gimp-filter-layer > "$out/build.log" 2>&1
export LD_LIBRARY_PATH=$(find "$build" -maxdepth 1 -type d -name 'libgimp*' -printf '%p:')${LD_LIBRARY_PATH:-}
python3 - <<'PY'
import subprocess,os,json,pathlib,time
b='/workspace/shared/gimp-clean-aggregate/build';s='/workspace/shared/gimp-clean-aggregate/source';env=dict(os.environ);env.update(GIMP_TESTING_ABS_TOP_SRCDIR=s,GIMP_TESTING_ABS_TOP_BUILDDIR=b,GIMP_TESTING_PLUGINDIRS=b+'/plug-ins/common',UI_TEST='yes');cmd=[b+'/app/tests/gimp-filter-layer']
for n in ['filter_gaussian_restore_latest_lower_contract']:cmd+=['-p','/gimp-filter-layer/'+n]
t=time.time();p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=90);pathlib.Path('/workspace/shared/filter-restore-result/native.json').write_text(json.dumps({'command':cmd,'exit_code':p.returncode,'duration':time.time()-t,'stdout':p.stdout,'stderr':p.stderr},indent=2)+'\n');print(p.stdout);print(p.stderr);assert p.returncode==0
PY
python3 "$src/migration/tests/run_filter_layer_sanitizers.py" "$build" --report "$out/sanitizer.json" --test-path /gimp-filter-layer/filter_gaussian_restore_latest_lower_contract
python3 - <<'PY'
import json,pathlib,hashlib
s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-restore-result');j=json.loads((o/'source-before.json').read_text());j['changed_after_run']=[p for p,h in j['sha256'].items() if hashlib.sha256((s/p).read_bytes()).hexdigest()!=h];(o/'source-after.json').write_text(json.dumps(j,indent=2)+'\n');assert not j['changed_after_run']
PY
