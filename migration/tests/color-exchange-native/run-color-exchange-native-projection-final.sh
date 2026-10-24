set -euo pipefail
source /workspace/shared/gimp-clean-aggregate/env.sh
exec 9>/workspace/shared/gimp-clean-aggregate/build.lock
flock 9
src=/workspace/shared/gimp-clean-aggregate/source
build=/workspace/shared/gimp-clean-aggregate/build
out=/workspace/shared/color-exchange-native-projection-final
mkdir -p "$out"
python3 - <<'PY'
from pathlib import Path
import json,hashlib,subprocess
r=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');s=Path('/workspace/shared/gimp-clean-aggregate/source');o=Path('/workspace/shared/color-exchange-native-projection-final');q=Path('/workspace/shared/preset-catalog/gui-wrapper-trace/source-manifest.json');j=json.loads(q.read_text());expected=dict(j['complete_source_sha256']);expected.update(json.loads(Path('/workspace/shared/red-eye-xcf-green/source-after.json').read_text())['sha256']);expected.update(json.loads(Path('/workspace/shared/color-exchange-native-red/source-after.json').read_text())['sha256']);expected.update(json.loads(Path('/workspace/shared/red-eye-portrait-result/source-after.json').read_text())['sha256'])
expected.update(json.loads(Path('/workspace/shared/color-exchange-native-final/source-after.json').read_text())['sha256'])
# Catalog successor full source seal supplied only after explicit release.
q=Path(Path('/workspace/shared/color-exchange-catalog-predecessor.txt').read_text().strip());expected=json.loads(q.read_text())['complete_source_sha256']
expected.update(json.loads(Path('/workspace/shared/color-exchange-native-complete/source-after.json').read_text())['sha256'])
for p,h in expected.items():assert hashlib.sha256((s/p).read_bytes()).hexdigest()==h,p
owned=['app/core/gimpfilterlayer.cpp','app/tests/test-gimp-filter-layer.c','app/tests/test-filter-color-exchange-contract.inc','migration/tests/run_color_exchange_native_sanitizers.py']
raw=['app/painter/filter-native-kernels.cpp','app/painter/filter-native-kernels.hpp','app/painter/filter-point-shadow.cpp','app/painter/filter-point-shadow.hpp','app/painter/filter-color-exchange.hpp','app/painter/filter-color-exchange-private.hpp']
for p in raw:assert (r/p).read_bytes()==subprocess.check_output(['git','show','e1226f62de:'+p],cwd=r)
paths=set(json.loads(Path('/workspace/shared/filter-attributes-proof/source-after.json').read_text())['sha256']);paths.update(p for p in expected if p.startswith('app/tests/test-filter-') and p.endswith('.inc'));paths.update(['app/core/gimpfiltercontext.cpp','app/core/gimpfiltercontext.hpp','app/painter/filter-context.cpp','app/painter/filter-context.hpp','migration/tests/run_filter_layer_sanitizers.py','migration/tests/painter_sanitizer_scope.py']);paths.update(owned+raw)
for p in owned+raw:
 if not (s/p).exists() or (s/p).read_bytes()!=(r/p).read_bytes(): (s/p).write_bytes((r/p).read_bytes())
(o/'source-before.json').write_text(json.dumps({'scope':'sealed catalog/missing-route predecessor plus exact18bd four raw providers and own native candidate;not whole HEAD','predecessor_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'owned':owned,'raw_committed':'e1226f62de','raw_paths':raw,'sha256':{p:hashlib.sha256((s/p).read_bytes()).hexdigest() for p in sorted(paths)}},indent=2)+'\n')
PY
ninja -C "$build" app/tests/gimp-filter-layer > "$out/build.log" 2>&1
export LD_LIBRARY_PATH=$(find "$build" -maxdepth 1 -type d -name 'libgimp*' -printf '%p:')${LD_LIBRARY_PATH:-}
python3 - <<'PY'
from pathlib import Path
import json,os,subprocess,time,resource
s=Path('/workspace/shared/gimp-clean-aggregate/source');b=Path('/workspace/shared/gimp-clean-aggregate/build');o=Path('/workspace/shared/color-exchange-native-projection-final');env=dict(os.environ);env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(s),GIMP_TESTING_ABS_TOP_BUILDDIR=str(b),GIMP_TESTING_PLUGINDIRS=str(b/'plug-ins/common'),UI_TEST='yes');cases=['filter_color_exchange_native_basic','exchange_unsupported_formats','exchange_component_masks','exchange_late_context_routes','exchange_cancel_and_resume','exchange_invalid_arguments','exchange_outside_selection_no_merge','exchange_rgb_rgba_inputs','exchange_noalpha_owner_rejected','exchange_projection_extent_cache','filter_red_eye_native_basic','red_eye_component_masks','red_eye_invalid_arguments','filter_gaussian_attributes_contract'];report={}
for case in cases:
 cmd=[str(b/'app/tests/gimp-filter-layer'),'-p','/gimp-filter-layer/'+case];start=time.monotonic();p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=180,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)));report[case]={'command':cmd,'exit_code':p.returncode,'stdout':p.stdout,'stderr':p.stderr,'seconds':time.monotonic()-start};(o/'native.json').write_text(json.dumps(report,indent=2)+'\n');print(case,p.returncode,p.stdout,p.stderr,flush=True);assert p.returncode==0
PY
python3 - <<'PY'
from pathlib import Path
import json,hashlib
s=Path('/workspace/shared/gimp-clean-aggregate/source');o=Path('/workspace/shared/color-exchange-native-projection-final');j=json.loads((o/'source-before.json').read_text());j['changed_after_run']=[p for p,h in j['sha256'].items() if hashlib.sha256((s/p).read_bytes()).hexdigest()!=h];(o/'source-after.json').write_text(json.dumps(j,indent=2)+'\n');assert not j['changed_after_run']
PY

python3 "$src/migration/tests/run_color_exchange_native_sanitizers.py" "$build" --report "$out/sanitizer.json" \
 --test-path /gimp-filter-layer/filter_color_exchange_native_basic \
 --test-path /gimp-filter-layer/exchange_unsupported_formats \
 --test-path /gimp-filter-layer/exchange_component_masks \
 --test-path /gimp-filter-layer/exchange_late_context_routes \
 --test-path /gimp-filter-layer/exchange_cancel_and_resume \
 --test-path /gimp-filter-layer/exchange_invalid_arguments \
 --test-path /gimp-filter-layer/exchange_outside_selection_no_merge \
 --test-path /gimp-filter-layer/exchange_rgb_rgba_inputs \
 --test-path /gimp-filter-layer/exchange_noalpha_owner_rejected \
 --test-path /gimp-filter-layer/exchange_projection_extent_cache

python3 /workspace/shared/color-exchange-projection-mutation.py
python3 - <<'END'
from pathlib import Path
import json,hashlib,tarfile
s=Path('/workspace/shared/gimp-clean-aggregate/source');o=Path('/workspace/shared/color-exchange-native-projection-final');j=json.loads((o/'source-before.json').read_text());j['changed_after_run']=[p for p,h in j['sha256'].items() if hashlib.sha256((s/p).read_bytes()).hexdigest()!=h];assert not j['changed_after_run'];(o/'source-after.json').write_text(json.dumps(j,indent=2)+'\n')
sources=dict(j['sha256']);sources.update(json.loads((o/'sanitizer.json').read_text())['source_sha256'])
with tarfile.open(o/'sources.tar.gz','w:gz') as t:
 for p,h in sorted(sources.items()):
  assert hashlib.sha256((s/p).read_bytes()).hexdigest()==h,p
  t.add(s/p,arcname=p)
END
