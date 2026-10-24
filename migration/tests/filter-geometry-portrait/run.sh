set -euo pipefail
source /workspace/shared/gimp-clean-aggregate/env.sh
exec 9>/workspace/shared/gimp-clean-aggregate/build.lock
flock 9
src=/workspace/shared/gimp-clean-aggregate/source
build=/workspace/shared/gimp-clean-aggregate/build
out=/workspace/shared/filter-geometry-portrait-result
mkdir -p "$out"
python3 - <<'PY'
import pathlib,json,hashlib,subprocess
r=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-geometry-portrait-result');prior=json.loads(pathlib.Path('/workspace/shared/filter-simple-geometry-green/source-after.json').read_text())['sha256']
for p,h in prior.items():assert hashlib.sha256((s/p).read_bytes()).hexdigest()==h,p
for p in ['app/tests/test-painter-mode-ui.c','app/tests/test-filter-flip-portrait.inc','app/tests/test-painter-mode-anime.inc','tools/run-painter-xvfb.py']:
 assert (s/p).read_bytes()==subprocess.check_output(['git','show','9fad7ffbc6:'+p],cwd=r),p
 prior[p]=hashlib.sha256((s/p).read_bytes()).hexdigest()
for p in ['app/tests/test-painter-mode-ui.c','app/tests/test-filter-geometry-portrait.inc']:
 (s/p).write_bytes((r/p).read_bytes());prior[p]=hashlib.sha256((s/p).read_bytes()).hexdigest()
(o/'source-before.json').write_text(json.dumps({'predecessor':'filter-simple-geometry-green/source-after.json','committed_filter_provider':'9fad7ffbc6','scope':'two GUI fixture overlays only;explicit enum provider checkpoint retained','sha256':prior},indent=2)+'\n')
PY
ninja -C "$build" app/tests/painter-mode-ui > "$out/build.log" 2>&1
export LD_LIBRARY_PATH=$(find "$build" -maxdepth 1 -type d -name 'libgimp*' -printf '%p:')${LD_LIBRARY_PATH:-}
export GIMP_TESTING_ABS_TOP_SRCDIR="$src" GIMP_TESTING_ABS_TOP_BUILDDIR="$build" GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common" UI_TEST=yes
export GIMP_TEST_FILTER_GEOMETRY_SCREENSHOT_DIR="$out"
python3 - <<'PY'
import pathlib,json,subprocess,time,hashlib
s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-geometry-portrait-result');cmd=['python3',str(s/'tools/run-painter-xvfb.py'),'/workspace/shared/gimp-clean-aggregate/build/app/tests/painter-mode-ui','-p','/painter-mode-ui/filter-geometry-portrait'];t=time.time();p=subprocess.run(cmd,capture_output=True,text=True,timeout=180)
(o/'native-gui.json').write_text(json.dumps(dict(command=cmd,duration=time.time()-t,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr),indent=2)+'\n');print(p.stdout);print(p.stderr)
j=json.loads((o/'source-before.json').read_text());j['changed_after_run']=[p for p,h in j['sha256'].items() if hashlib.sha256((s/p).read_bytes()).hexdigest()!=h];(o/'source-after.json').write_text(json.dumps(j,indent=2)+'\n');assert not j['changed_after_run'];assert p.returncode==0
PY
