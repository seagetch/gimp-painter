#!/bin/bash
set -euo pipefail
R=/workspace/scratch/0015fc5d1a03/gimp-painter
W=/workspace/shared/gimp-clean-aggregate
E=/workspace/shared/options-construction
mkdir -p "$E"
exec 9>"$W/build.lock";flock 9
source "$W/env.sh"
python3 - "$R" "$W/source" "$E" <<'PY'
from pathlib import Path
import subprocess,hashlib,json,tarfile,sys
r,s,e=map(Path,sys.argv[1:]);base='81bc24545d';sha=lambda b:hashlib.sha256(b).hexdigest()
held=['app/widgets/gimpcellrendererpopup.cpp','app/display/gimppaintercanvasui.cpp','app/core/gimppaintermybrush.cpp','app/core/gimpviewable.c','app/widgets/gimpviewrenderer.c','app/tools/gimpfillbrushtool.cpp','app/tools/gimpbucketfillbrushtool.h','app/tools/gimp-tools.c']
for p in held:assert (s/p).read_bytes()==subprocess.check_output(['git','-C',str(r),'show',base+':'+p]),p
owned=['app/paint/gimpmypaintoptions.h','app/paint/painter-mypaint-surface/gimp-painter-options.cpp','app/paint/painter-mypaint-surface/gimp-painter-options.hpp','app/tests/test-mypaint-options-construction.cpp','app/tests/test-mypaint-options-construction-c.c','migration/tests/build_options_construction_sanitizers.py']
for p in owned:(s/p).write_bytes((r/p).read_bytes())
p=s/'app/tests/meson.build';a=p.read_text();orig=(r/'app/tests/meson.build').read_text();marker='# Original options type/constructor entries, sole native model and owned result.';stanza=orig[orig.index(marker):]
if 'mypaint_options_construction =' not in a:p.write_text(a+'\n'+stanza)
else:
 start=a.index(marker);end=a.index("timeout: 120)",start)+len("timeout: 120)");p.write_text(a[:start]+stanza.rstrip()+a[end:])
paths=owned+held+['app/tests/meson.build','app/core/gimpimage-undo.c','app/core/gimpimage-undo.h','app/painter/binding-store.cpp','app/painter/binding-store.hpp','app/painter/object-ref.hpp','app/painter/boundary.hpp','tools/run-painter-xvfb.py']
m={'base':base,'owned_source_sha256':{p:sha((s/p).read_bytes()) for p in owned},'input_sha256':{p:sha((s/p).read_bytes()) for p in paths},'scope':'selected options construction overlay, not whole HEAD; Filter native Undo and fixture overlays retained, not tested by this fixture','held_providers':held}
(e/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with tarfile.open(e/'tested-inputs.tar.gz','w:gz') as t:
 for p in paths:t.add(s/p,arcname=p)
PY
ninja -C "$W/build" -j2 app/tests/mypaint-options-construction >"$E/build.log" 2>&1
python3 "$W/source/tools/run-painter-xvfb.py" meson test -C "$W/build" mypaint-options-construction --no-rebuild --print-errorlogs --logbase options-construction >"$E/normal-portrait.log" 2>&1
python3 - "$W/build/meson-logs/options-construction.json" "$E/normal.json" <<'PY'
import json,sys
rows=[json.loads(x) for x in open(sys.argv[1])];json.dump([{k:r.get(k) for k in ['name','result','returncode','stdout','stderr','duration','command']} for r in rows],open(sys.argv[2],'w'),indent=2)
PY
python3 "$W/source/migration/tests/build_options_construction_sanitizers.py" "$W/build" --report "$E/asan-build.json" >"$E/asan-build.log" 2>&1
export GIMP_TESTING_ABS_TOP_SRCDIR="$W/source" GIMP_TESTING_ABS_TOP_BUILDDIR="$W/build" GIMP_TESTING_PLUGINDIRS="$W/build/plug-ins/common" UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export LD_LIBRARY_PATH=$(find "$W/build" -maxdepth 1 -type d -name 'libgimp*' | paste -sd:):${LD_LIBRARY_PATH:-}
python3 "$W/source/tools/run-painter-xvfb.py" python3 - <<'PY' >"$E/asan-portrait.log" 2>&1
import pathlib,subprocess,time,json
w=pathlib.Path('/workspace/shared/gimp-clean-aggregate');e=pathlib.Path('/workspace/shared/options-construction');start=time.monotonic();cmd=[str(w/'build/options-construction-sanitizers/mypaint-options-construction'),'--tap'];p=subprocess.run(cmd,capture_output=True,text=True,timeout=120);(e/'asan-results.json').write_text(json.dumps(dict(command=cmd,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr,duration=time.monotonic()-start),indent=2)+'\n');print(p.stdout);print(p.stderr);assert p.returncode==0
PY
for mutant in wrong-name publish-failed; do
 python3 "$W/source/migration/tests/build_options_construction_sanitizers.py" "$W/build" --mutant "$mutant" --report "$E/mutant-$mutant-build.json" >"$E/mutant-$mutant-build.log" 2>&1
 python3 "$W/source/tools/run-painter-xvfb.py" python3 - "$mutant" <<'PY' >"$E/mutant-$mutant-portrait.log" 2>&1
import pathlib,subprocess,json,sys
w=pathlib.Path('/workspace/shared/gimp-clean-aggregate');e=pathlib.Path('/workspace/shared/options-construction');m=sys.argv[1];case='independent-owned-original-name' if m=='wrong-name' else 'native-construction-failure';cmd=[str(w/('build/options-construction-sanitizers-'+m+'/mypaint-options-construction')),'--tap','-p','/mypaint-options-construction/'+case];p=subprocess.run(cmd,capture_output=True,text=True,timeout=45);(e/('mutant-'+m+'.json')).write_text(json.dumps(dict(command=cmd,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr,case=case),indent=2)+'\n');print(p.stdout,p.stderr);assert p.returncode!=0;assert 'assertion failed' in p.stdout+p.stderr or 'should be nullptr' in p.stdout+p.stderr
PY
done
python3 - "$W/source" "$E" <<'PY'
from pathlib import Path
import json,hashlib,sys
s,e=map(Path,sys.argv[1:]);m=json.loads((e/'source-manifest.json').read_text());assert all(hashlib.sha256((s/p).read_bytes()).hexdigest()==h for p,h in m['input_sha256'].items());(e/'post-test-inputs.json').write_text(json.dumps({'all_recorded_inputs_unchanged':True},indent=2)+'\n')
PY
