#!/bin/bash
set -euo pipefail
# Invoke only after parent releases exact-HEAD aggregate reservation.
R=/workspace/scratch/0015fc5d1a03/gimp-painter
W=/workspace/shared/gimp-clean-aggregate
MODE=${1:-green}
E=/workspace/shared/options-mode-entry/$MODE
mkdir -p "$E"
exec 9>"$W/build.lock";flock 9
source "$W/env.sh"
python3 - "$R" "$W/source" "$E" "$MODE" <<'PY'
from pathlib import Path
import subprocess,hashlib,json,tarfile,sys
r,s,e=map(Path,sys.argv[1:4]);mode=sys.argv[4];base='20b2204b11df079c306a680b41ac623f3be75c76';sha=lambda b:hashlib.sha256(b).hexdigest()
held=['app/widgets/gimpcellrendererpopup.cpp','app/display/gimppaintercanvasui.cpp','app/core/gimppaintermybrush.cpp','app/core/gimpviewable.c','app/widgets/gimpviewrenderer.c','app/tools/gimpfillbrushtool.cpp','app/tools/gimpbucketfillbrushtool.h','app/tools/gimp-tools.c']
for p in held:assert (s/p).read_bytes()==subprocess.check_output(['git','-C',str(r),'show',base+':'+p]),p
clone=['app/core/gimpclonelayer.cpp','app/core/gimpclonelayer.h','app/core/gimpclonelayer-handle.hpp','app/core/gimpclonelayerundo.h'];clone_base='20b2204b11df079c306a680b41ac623f3be75c76'
for p in clone:assert (s/p).read_bytes()==subprocess.check_output(['git','-C',str(r),'show',clone_base+':'+p]),p
owned=['app/paint/gimpmypaintoptions.h','app/paint/painter-mypaint-surface/gimp-painter-options.hpp','app/tests/test-mypaint-options-mode.cpp','app/tests/test-mypaint-options-mode-c.c','migration/tests/build_options_mode_sanitizers.py']
if mode=='green':owned+=['app/paint/painter-mypaint-surface/gimp-painter-options.cpp']
for p in owned:(s/p).write_bytes((r/p).read_bytes())
p=s/'app/tests/meson.build';a=p.read_text();orig=(r/'app/tests/meson.build').read_text();marker='# Original constant MyPaint rendering-mode helper, not stored mode selection.';start=orig.index(marker);end=orig.index('timeout: 120)',start)+len('timeout: 120)');stanza=orig[start:end]
if 'mypaint_options_mode =' not in a:p.write_text(a+'\n'+stanza+'\n')
else:assert stanza in a
paths=list(dict.fromkeys(owned+held+clone+['app/widgets/gimppaintermybrusheditor.cpp','app/paint/painter-mypaint-surface/gimp-painter-options.cpp','app/tests/meson.build','migration/inventory/additional-c-header-routes.json','app/painter/binding-store.cpp','app/painter/binding-store.hpp','app/painter/object-ref.hpp','app/painter/boundary.hpp','tools/run-painter-xvfb.py']))
m={'base_for_held_providers':base,'mode':mode,'owned_source_sha256':{p:sha((s/p).read_bytes()) for p in owned},'input_sha256':{p:sha((s/p).read_bytes()) for p in paths},'scope':'selected original constant-mode entry overlay after parent committed aggregate; not whole HEAD','held_providers':held,'clone_providers':clone,'clone_commit':clone_base,'ancillary_overlays':'Exact20b committed baseline before only named mode overlay; root aggregate21targets238cases+Filter4+graphUI3 passed separately; not whole-HEAD proof for this candidate'}
(e/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with tarfile.open(e/'tested-inputs.tar.gz','w:gz') as t:
 for p in paths:t.add(s/p,arcname=p)
PY
set +e
ninja -C "$W/build" -j2 app/tests/mypaint-options-mode >"$E/build.log" 2>&1
result=$?
set -e
if [ "$MODE" = red ]; then test "$result" != 0;grep -q 'undefined reference.*gimp_mypaint_options_get_brush_mode' "$E/build.log";exit 0;fi
test "$result" = 0
python3 "$W/source/tools/run-painter-xvfb.py" meson test -C "$W/build" mypaint-options-mode --no-rebuild --print-errorlogs --logbase options-mode-entry >"$E/normal-portrait.log" 2>&1
python3 - "$W/build/meson-logs/options-mode-entry.json" "$E/normal.json" <<'PY'
import json,sys
rows=[json.loads(x) for x in open(sys.argv[1])];json.dump([{k:r.get(k) for k in ['name','result','returncode','stdout','stderr','duration','command']} for r in rows],open(sys.argv[2],'w'),indent=2)
PY
python3 "$W/source/migration/tests/build_options_mode_sanitizers.py" "$W/build" --report "$E/asan-build.json" >"$E/asan-build.log" 2>&1
export GIMP_TESTING_ABS_TOP_SRCDIR="$W/source" GIMP_TESTING_ABS_TOP_BUILDDIR="$W/build" GIMP_TESTING_PLUGINDIRS="$W/build/plug-ins/common" UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export LD_LIBRARY_PATH=$(find "$W/build" -maxdepth 1 -type d -name 'libgimp*' | paste -sd:):${LD_LIBRARY_PATH:-}
python3 "$W/source/tools/run-painter-xvfb.py" python3 - <<'PY' >"$E/asan-portrait.log" 2>&1
import pathlib,subprocess,time,json
w=pathlib.Path('/workspace/shared/gimp-clean-aggregate');e=pathlib.Path('/workspace/shared/options-mode-entry/green');start=time.monotonic();cmd=[str(w/'build/options-mode-entry-sanitizers/mypaint-options-mode'),'--tap'];p=subprocess.run(cmd,capture_output=True,text=True,timeout=180);(e/'asan-results.json').write_text(json.dumps(dict(command=cmd,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr,duration=time.monotonic()-start),indent=2)+'\n');print(p.stdout,p.stderr);assert p.returncode==0
PY
python3 "$W/source/tools/check_painter_headers.py" --build-dir "$W/build" --report "$E/headers.json" >"$E/headers.log" 2>&1
python3 - "$W/source" "$E" <<'PY'
from pathlib import Path
import json,hashlib,sys
s,e=map(Path,sys.argv[1:]);m=json.loads((e/'source-manifest.json').read_text());assert all(hashlib.sha256((s/p).read_bytes()).hexdigest()==h for p,h in m['input_sha256'].items());(e/'post-test-inputs.json').write_text(json.dumps({'all_recorded_inputs_unchanged':True},indent=2)+'\n')
PY
