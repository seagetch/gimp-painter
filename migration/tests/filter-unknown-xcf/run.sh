set -euo pipefail
source /workspace/shared/gimp-clean-aggregate/env.sh
exec 9>/workspace/shared/gimp-clean-aggregate/build.lock
flock 9
src=/workspace/shared/gimp-clean-aggregate/source
build=/workspace/shared/gimp-clean-aggregate/build
out=/workspace/shared/filter-unknown-xcf-proof
mkdir -p "$out"
python3 - <<'PY'
import pathlib,json,hashlib,subprocess
r=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-unknown-xcf-proof');sha=subprocess.check_output(['git','rev-parse','2ef2dd32ee'],cwd=r,text=True).strip()
prior=json.loads(pathlib.Path('/workspace/shared/preset-help-successor/green/source-manifest.json').read_text());expected=dict(prior['complete_source_sha256']);matrix=json.loads(pathlib.Path('/workspace/shared/filter-attributes-proof/source-after.json').read_text());expected.update(matrix['sha256'])
for p,h in expected.items():assert hashlib.sha256((s/p).read_bytes()).hexdigest()==h,p
paths=set(matrix['sha256']);paths.update(p for p in expected if p.startswith('app/xcf/') or p.startswith('app/tests/test-painter-xcf') or p in ['app/tests/test-painter-mode-xcf.inc','migration/tests/run_painter_mode_sanitizers.py'])
owned=['app/tests/test-painter-xcf-roundtrip.c','app/tests/test-filter-unknown-xcf.inc','migration/tests/run_filter_unknown_xcf_sanitizers.py']
p=owned[0];t=(s/p).read_text();n=t.replace('#include "test-painter-xcf-fields.inc"','#include "test-filter-unknown-xcf.inc"\n#include "test-painter-xcf-fields.inc"').replace('  gimp = gimp_init_for_testing ();','  gimp = gimp_init_for_testing ();\n  g_test_add_func ("/painter-xcf-roundtrip/filter_unknown_xcf_preservation", filter_unknown_xcf_preservation);');assert n==(r/p).read_text()
for p in owned:(s/p).write_bytes((r/p).read_bytes());paths.add(p)
(o/'source-before.json').write_text(json.dumps({'committed_base':sha,'scope':'unknown-only test overlays after explicit readiness/property/help/attributes predecessors;not whole HEAD','sha256':{p:hashlib.sha256((s/p).read_bytes()).hexdigest() for p in sorted(paths)}},indent=2)+'\n')
PY
ninja -C "$build" app/tests/painter-xcf-roundtrip > "$out/build.log" 2>&1
export LD_LIBRARY_PATH=$(find "$build" -maxdepth 1 -type d -name 'libgimp*' -printf '%p:')${LD_LIBRARY_PATH:-}
python3 - <<'PY'
import subprocess,os,json,pathlib,time
b='/workspace/shared/gimp-clean-aggregate/build';s='/workspace/shared/gimp-clean-aggregate/source';env=dict(os.environ);env.update(GIMP_TESTING_ABS_TOP_SRCDIR=s,GIMP_TESTING_ABS_TOP_BUILDDIR=b,GIMP_TESTING_PLUGINDIRS=b+'/plug-ins/common',UI_TEST='yes');cmd=[b+'/app/tests/painter-xcf-roundtrip','-p','/painter-xcf-roundtrip/filter_unknown_xcf_preservation'];t=time.time();p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=90);pathlib.Path('/workspace/shared/filter-unknown-xcf-proof/native.json').write_text(json.dumps({'command':cmd,'exit_code':p.returncode,'duration':time.time()-t,'stdout':p.stdout,'stderr':p.stderr},indent=2)+'\n');print(p.stdout);print(p.stderr);assert p.returncode==0
PY
python3 "$src/migration/tests/run_filter_unknown_xcf_sanitizers.py" "$build" --report "$out/sanitizer.json" --test-path /painter-xcf-roundtrip/filter_unknown_xcf_preservation
python3 - <<'PY'
import pathlib,json,hashlib
s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-unknown-xcf-proof');j=json.loads((o/'source-before.json').read_text());j['changed_after_run']=[p for p,h in j['sha256'].items() if hashlib.sha256((s/p).read_bytes()).hexdigest()!=h];(o/'source-after.json').write_text(json.dumps(j,indent=2)+'\n');assert not j['changed_after_run']
PY
