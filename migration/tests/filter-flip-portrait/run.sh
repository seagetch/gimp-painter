set -euo pipefail
: "${FILTER_PORTRAIT_BASE_SHA:?Parent must supply sealed committed clean base SHA}"
source /workspace/shared/gimp-clean-aggregate/env.sh
exec 9>/workspace/shared/gimp-clean-aggregate/build.lock
flock 9
repo=/workspace/scratch/0015fc5d1a03/gimp-painter
src=/workspace/shared/gimp-clean-aggregate/source
build=/workspace/shared/gimp-clean-aggregate/build
out=/workspace/shared/filter-flip-portrait-result
mkdir -p "$out"
export FILTER_PORTRAIT_BASE_SHA
python3 - <<'PY'
import pathlib,subprocess,os,hashlib,json
r=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');out=pathlib.Path('/workspace/shared/filter-flip-portrait-result');sha=os.environ['FILTER_PORTRAIT_BASE_SHA']
paths=['app/core/gimpfilterlayer.cpp','app/core/gimpviewable.c','app/core/gimppaintermybrush.cpp','app/widgets/gimpviewrenderer.c','app/widgets/gimpcellrendererpopup.cpp','app/display/gimppaintercanvasui.cpp','app/tools/gimpfillbrushtool.cpp','app/tools/gimpbucketfillbrushtool.h','app/tools/gimp-tools.c','app/tests/test-painter-mode-ui.c','app/tests/test-painter-mode-anime.inc','app/core/gimpimage-flip.c','app/painter/filter-gauss.cpp','app/painter/filter-gauss-kernel-private.hpp','tools/run-painter-xvfb.py']
for p in paths:assert (s/p).read_bytes()==subprocess.check_output(['git','show',sha+':'+p],cwd=r),p
p='app/tests/test-painter-mode-ui.c';original=(s/p).read_text();modified=original.replace('#include "test-painter-mode-anime.inc"','#include "test-painter-mode-anime.inc"\n#include "test-filter-flip-portrait.inc"').replace('  g_test_add_data_func("/painter-mode-ui/native-selector"','  g_test_add_func("/painter-mode-ui/filter-flip-portrait",filter_flip_oncanvas_portrait);\n  g_test_add_data_func("/painter-mode-ui/native-selector"')
assert modified!=original
(s/p).write_text(modified)
p='app/tests/test-filter-flip-portrait.inc';(s/p).write_bytes((r/p).read_bytes());paths.append(p)
(out/'source-before.json').write_text(json.dumps({'committed_base':sha,'owned_test_overlays':['app/tests/test-painter-mode-ui.c',p],'sha256':{p:hashlib.sha256((s/p).read_bytes()).hexdigest() for p in paths}},indent=2)+'\n')
PY
ninja -C "$build" app/tests/painter-mode-ui > "$out/build.log" 2>&1
export LD_LIBRARY_PATH=$(find "$build" -maxdepth 1 -type d -name 'libgimp*' -printf '%p:')${LD_LIBRARY_PATH:-}
export GIMP_TESTING_ABS_TOP_SRCDIR="$src" GIMP_TESTING_ABS_TOP_BUILDDIR="$build" GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common" UI_TEST=yes
export GIMP_TEST_FILTER_FLIP_SCREENSHOT_DIR="$out"
python3 - <<'PY'
import pathlib,json,subprocess,time,hashlib
s=pathlib.Path('/workspace/shared/gimp-clean-aggregate/source');o=pathlib.Path('/workspace/shared/filter-flip-portrait-result');cmd=['python3',str(s/'tools/run-painter-xvfb.py'),'/workspace/shared/gimp-clean-aggregate/build/app/tests/painter-mode-ui','-p','/painter-mode-ui/filter-flip-portrait'];t=time.time();p=subprocess.run(cmd,capture_output=True,text=True,timeout=180)
(o/'native-gui.json').write_text(json.dumps(dict(command=cmd,duration=time.time()-t,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr),indent=2)+'\n');print(p.stdout);print(p.stderr)
j=json.loads((o/'source-before.json').read_text());j['changed_after_run']=[p for p,h in j['sha256'].items() if hashlib.sha256((s/p).read_bytes()).hexdigest()!=h];(o/'source-after.json').write_text(json.dumps(j,indent=2)+'\n');assert not j['changed_after_run'];assert p.returncode==0
PY
