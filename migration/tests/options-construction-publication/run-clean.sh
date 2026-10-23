#!/bin/bash
set -euo pipefail
R=/workspace/scratch/0015fc5d1a03/gimp-painter
W=/workspace/shared/gimp-clean-aggregate
MODE=${1:-green}
E=/workspace/shared/options-construction-publication/$MODE
mkdir -p "$E"
exec 9>"$W/build.lock";flock 9
source "$W/env.sh"
python3 - "$R" "$W/source" "$E" <<'PY'
from pathlib import Path
import subprocess,hashlib,json,tarfile,sys
r,s,e=map(Path,sys.argv[1:]);base='81bc24545d';sha=lambda b:hashlib.sha256(b).hexdigest()
held=['app/widgets/gimpcellrendererpopup.cpp','app/display/gimppaintercanvasui.cpp','app/core/gimppaintermybrush.cpp','app/core/gimpviewable.c','app/widgets/gimpviewrenderer.c','app/tools/gimpfillbrushtool.cpp','app/tools/gimpbucketfillbrushtool.h','app/tools/gimp-tools.c']
for p in held:assert (s/p).read_bytes()==subprocess.check_output(['git','-C',str(r),'show',base+':'+p]),p
owned=['app/paint/gimpmypaintoptions.h','app/paint/painter-mypaint-surface/gimp-painter-options.cpp','app/paint/painter-mypaint-surface/gimp-painter-options.hpp','app/tests/test-mypaint-options-construction.cpp','app/tests/test-mypaint-options-construction-c.c','app/tests/test-mypaint-options-construction-reentry.cpp','migration/tests/build_options_construction_reentry_sanitizers.py']
for p in owned:(s/p).write_bytes((r/p).read_bytes())
p=s/'app/tests/meson.build';a=p.read_text();orig=(r/'app/tests/meson.build').read_text();marker='# Causal endpoint acquisition and native-construction callback reentry.';start=orig.index(marker);end=orig.index('timeout: 120)',start)+len('timeout: 120)');stanza=orig[start:end]
if 'mypaint_options_construction_reentry =' not in a:p.write_text(a+'\n'+stanza+'\n')
else:assert stanza in a
paths=owned+held+['app/tests/meson.build','app/core/gimpimage-undo.c','app/core/gimpimage-undo.h','app/painter/binding-store.cpp','app/painter/binding-store.hpp','app/painter/object-ref.hpp','app/painter/boundary.hpp','tools/run-painter-xvfb.py']
m={'base':base,'owned_source_sha256':{p:sha((s/p).read_bytes()) for p in owned},'input_sha256':{p:sha((s/p).read_bytes()) for p in paths},'scope':'selected options constructor reentry overlay, not whole HEAD; other Filter fixture/coreUndo overlays retained, not executed here','held_providers':held}
(e/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with tarfile.open(e/'tested-inputs.tar.gz','w:gz') as t:
 for p in paths:t.add(s/p,arcname=p)
PY
ninja -C "$W/build" -j2 app/tests/mypaint-options-construction app/tests/mypaint-options-construction-reentry >"$E/build.log" 2>&1
set +e
python3 "$W/source/tools/run-painter-xvfb.py" meson test -C "$W/build" mypaint-options-construction mypaint-options-construction-reentry --no-rebuild --print-errorlogs --logbase options-reentry >"$E/normal-portrait.log" 2>&1
result=$?
set -e
python3 - "$W/build/meson-logs/options-reentry.json" "$E/normal.json" <<'PY'
import json,sys
rows=[json.loads(x) for x in open(sys.argv[1])];json.dump([{k:r.get(k) for k in ['name','result','returncode','stdout','stderr','duration','command']} for r in rows],open(sys.argv[2],'w'),indent=2)
PY
if [ "$MODE" = red ]; then test "$result" != 0;exit 0;fi
test "$result" = 0
python3 "$W/source/migration/tests/build_options_construction_reentry_sanitizers.py" "$W/build" --report "$E/asan-build.json" >"$E/asan-build.log" 2>&1
export GIMP_TESTING_ABS_TOP_SRCDIR="$W/source" GIMP_TESTING_ABS_TOP_BUILDDIR="$W/build" GIMP_TESTING_PLUGINDIRS="$W/build/plug-ins/common" UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export LD_LIBRARY_PATH=$(find "$W/build" -maxdepth 1 -type d -name 'libgimp*' | paste -sd:):${LD_LIBRARY_PATH:-}
python3 "$W/source/tools/run-painter-xvfb.py" python3 - <<'PY' >"$E/asan-portrait.log" 2>&1
import pathlib,subprocess,time,json
w=pathlib.Path('/workspace/shared/gimp-clean-aggregate');e=pathlib.Path('/workspace/shared/options-construction-publication/green');rows=[]
for target in ['mypaint-options-construction','mypaint-options-construction-reentry']:
 start=time.monotonic();cmd=[str(w/('build/options-construction-reentry-sanitizers/'+target)),'--tap'];p=subprocess.run(cmd,capture_output=True,text=True,timeout=120);rows.append(dict(command=cmd,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr,duration=time.monotonic()-start));print(p.stdout,p.stderr)
(e/'asan-results.json').write_text(json.dumps(rows,indent=2)+'\n');assert all(r['returncode']==0 for r in rows)
PY
python3 - "$W/source" "$E" <<'PY'
from pathlib import Path
import json,hashlib,sys
s,e=map(Path,sys.argv[1:]);m=json.loads((e/'source-manifest.json').read_text());assert all(hashlib.sha256((s/p).read_bytes()).hexdigest()==h for p,h in m['input_sha256'].items());(e/'post-test-inputs.json').write_text(json.dumps({'all_recorded_inputs_unchanged':True},indent=2)+'\n')
PY
