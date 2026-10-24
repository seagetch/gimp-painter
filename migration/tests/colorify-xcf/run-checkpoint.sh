set -euo pipefail
source /workspace/shared/gimp-clean-aggregate/env.sh
exec 9>/workspace/shared/gimp-clean-aggregate/build.lock
flock 9
export TMPDIR=/tmp
export GIMP_TEST_MAX_RGB_FIXTURE_DIR=/tmp/color-exchange-maxrgb-corpus
src=/workspace/shared/gimp-clean-aggregate/source
build=/workspace/shared/gimp-clean-aggregate/build
out=/workspace/shared/colorify-xcf
mkdir -p "$out"
python3 -B - <<'PY'
from pathlib import Path
import json,hashlib,os
r=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');s=Path('/workspace/shared/gimp-clean-aggregate/source');o=Path('/workspace/shared/colorify-xcf');q=Path('/workspace/shared/color-exchange-xcf-green/source-after.json');j=json.loads(q.read_text())
for p,h in j['sha256'].items():assert hashlib.sha256(os.fsencode(os.readlink(s/p))if(s/p).is_symlink()else(s/p).read_bytes()).hexdigest()==h,p
owned=['app/tests/test-painter-xcf-open.c','app/tests/test-painter-xcf-colorify.inc','migration/tests/run_colorify_xcf_sanitizers.py']
for p in owned:(s/p).write_bytes((r/p).read_bytes())
expected=dict(j['sha256']);expected.update({p:hashlib.sha256((s/p).read_bytes()).hexdigest()for p in owned})
(o/'source-before.json').write_text(json.dumps({'predecessor_manifest':str(q),'predecessor_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'owned':owned,'sha256':expected},indent=2)+'\n')
PY
ninja -C "$build" app/tests/painter-xcf-open > "$out/build.log" 2>&1
export LD_LIBRARY_PATH=$(find "$build" -maxdepth 1 -type d -name 'libgimp*' -printf '%p:')${LD_LIBRARY_PATH:-}
python3 -B - <<'PY'
from pathlib import Path
import json,os,subprocess,resource,time
s=Path('/workspace/shared/gimp-clean-aggregate/source');b=Path('/workspace/shared/gimp-clean-aggregate/build');o=Path('/workspace/shared/colorify-xcf');env=dict(os.environ);env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(s),GIMP_TESTING_ABS_TOP_BUILDDIR=str(b),GIMP_TESTING_PLUGINDIRS=str(b/'plug-ins/common'),UI_TEST='yes');report={}
for case in ['colorify_typed_xcf_roundtrips','colorify_unsupported_typed_xcf_roundtrips','exchange_native_model_precedence','red_eye_old_wire_roundtrips']:
 cmd=[str(b/'app/tests/painter-xcf-open'),'-p','/painter-xcf-open/'+case];t=time.monotonic();p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=180,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)));j={'command':cmd,'exit_code':p.returncode,'stdout':p.stdout,'stderr':p.stderr,'seconds':time.monotonic()-t};report[case]=j;(o/'native.json').write_text(json.dumps(report,indent=2)+'\n');print(case,p.returncode,p.stdout[-500:],p.stderr[-500:],flush=True);assert p.returncode==0
PY
python3 "$src/migration/tests/run_colorify_xcf_sanitizers.py" "$build" --report "$out/sanitizer.json" --test-path /painter-xcf-open/colorify_typed_xcf_roundtrips --test-path /painter-xcf-open/colorify_unsupported_typed_xcf_roundtrips
python3 -B - <<'PY'
from pathlib import Path
import json,hashlib,tarfile
s=Path('/workspace/shared/gimp-clean-aggregate/source');o=Path('/workspace/shared/colorify-xcf');h=json.loads((o/'source-before.json').read_text());h['changed_after_run']=[x for x,v in h['sha256'].items()if hashlib.sha256((s/x).read_bytes()).hexdigest()!=v];assert not h['changed_after_run'];(o/'source-after.json').write_text(json.dumps(h,indent=2)+'\n')
selected=set(h['owned']);selected.update(p.relative_to(s).as_posix()for p in (s/'app/xcf').iterdir()if p.suffix in {'.c','.cpp','.h','.hpp'});selected.update(p.relative_to(s).as_posix()for p in (s/'app/tests').glob('test-painter-xcf*.inc'));selected.update(['app/core/gimpfilterlayer.cpp','app/core/gimpfilterlayer.h','migration/tests/run_painter_mode_sanitizers.py'])
(o/'selected-sources.json').write_text(json.dumps({p:h['sha256'][p]for p in sorted(selected)},indent=2)+'\n')
with tarfile.open(o/'sources.tar.gz','w:gz')as t:
 for p in sorted(selected):t.add(s/p,arcname=p)
PY
