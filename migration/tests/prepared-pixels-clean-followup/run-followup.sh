#!/bin/bash
set -euo pipefail
R=/workspace/scratch/0015fc5d1a03/gimp-painter
W=/workspace/shared/gimp-clean-aggregate
E=/workspace/shared/prepared-pixels-clean-followup
mkdir -p "$E"
exec 9>"$W/build.lock";flock 9
source "$W/env.sh"
python3 - "$R" "$W/source" "$E" <<'PY'
from pathlib import Path
import subprocess,hashlib,json,tarfile,sys
r,s,e=map(Path,sys.argv[1:]);rev='ff2116774a';base='81bc24545d';read=lambda revision,p:subprocess.check_output(['git','-C',str(r),'show',revision+':'+p]);sha=lambda b:hashlib.sha256(b).hexdigest()
held=['app/widgets/gimpcellrendererpopup.cpp','app/display/gimppaintercanvasui.cpp','app/core/gimppaintermybrush.cpp','app/core/gimpviewable.c','app/widgets/gimpviewrenderer.c','app/tools/gimpfillbrushtool.cpp','app/tools/gimpbucketfillbrushtool.h','app/tools/gimp-tools.c']
for p in held:assert (s/p).read_bytes()==read(base,p),p
(e/'held-source-seal.json').write_text(json.dumps({'baseline':base,'four_holds_eight_production_files':{p:sha((s/p).read_bytes()) for p in held}},indent=2)+'\n')
owned=['app/paint/painter-mypaint-surface/paint-core-pixels.cpp','app/paint/painter-mypaint-surface/paint-core-pixels.hpp','app/tests/test-painter-prepared-pixels.cpp','migration/tests/build_prepared_pixels_sanitizers.py']
for p in owned:(s/p).write_bytes(read(rev,p))
p=s/'app/paint/painter-mypaint-surface/meson.build';a=p.read_text()
if "'paint-core-pixels.cpp'" not in a:
 assert "'paint-core.cpp'," in a;a=a.replace("'paint-core.cpp',","'paint-core.cpp', 'paint-core-pixels.cpp',",1);p.write_text(a)
p=s/'app/tests/meson.build';a=p.read_text();orig=read(rev,'app/tests/meson.build').decode();marker='# Prepare-only native pixel rollback payload; no history publication.';stanza=orig[orig.index(marker):]
if 'prepared_pixels_test =' not in a:p.write_text(a+'\n'+stanza)
else:assert stanza in a
paths=owned+held+['app/core/gimpimage-undo.c','app/core/gimpimage-undo.h','app/core/gimpundopopguard.c','app/core/gimpundopopguard.h','app/core/core-types.h','app/core/gimpdrawable.c','app/core/gimpdrawable.h','app/core/gimpdrawableundo.h','app/paint/gimppaintcore.c','app/paint/gimppaintcore.h','app/gegl/gimp-gegl-loops.cc','app/gegl/gimp-gegl-loops.h','app/gegl/gimp-gegl-utils.h','app/painter/object-ref.hpp','app/painter/boundary.hpp','tools/run-painter-xvfb.py','app/tests/meson.build','app/paint/painter-mypaint-surface/meson.build','app/core/gimp-data-factories.c','app/tests/test-feature-lifecycle.c']
manifest={'selected_commit':rev,'clean_base':base,'owned_source_sha256':{p:sha(read(rev,p)) for p in owned},'input_sha256':{p:sha((s/p).read_bytes()) for p in paths},'ancillary_overlays':['committed readiness73b78e3554 and feature14 fixture','explicit-publication-outcome candidate gimpimage-undo.c/h','Clone test fixture additions; not executed by this target'],'scope':'model-only selected-source follow-up, not whole HEAD; original evidence immutable'}
(e/'source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
with tarfile.open(e/'tested-inputs.tar.gz','w:gz') as t:
 for p in paths:t.add(s/p,arcname=p)
PY
ninja -C "$W/build" -j2 app/tests/painter-prepared-pixels >"$E/build.log" 2>&1
python3 "$W/source/tools/run-painter-xvfb.py" meson test -C "$W/build" painter-prepared-pixels --no-rebuild --print-errorlogs --logbase prepared-pixels-clean-followup >"$E/normal-portrait.log" 2>&1
python3 - "$W/build/meson-logs/prepared-pixels-clean-followup.json" "$E/normal.json" <<'PY'
import json,sys
rows=[json.loads(x) for x in open(sys.argv[1])];json.dump([{k:r.get(k) for k in ['name','result','returncode','stdout','stderr','duration','command']} for r in rows],open(sys.argv[2],'w'),indent=2)
PY
python3 "$W/source/migration/tests/build_prepared_pixels_sanitizers.py" "$W/build" --report "$E/asan-build.json" >"$E/asan-build.log" 2>&1
export GIMP_TESTING_ABS_TOP_SRCDIR="$W/source" GIMP_TESTING_ABS_TOP_BUILDDIR="$W/build" GIMP_TESTING_PLUGINDIRS="$W/build/plug-ins/common" UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export LD_LIBRARY_PATH=$(find "$W/build" -maxdepth 1 -type d -name 'libgimp*' | paste -sd:):${LD_LIBRARY_PATH:-}
python3 "$W/source/tools/run-painter-xvfb.py" python3 - <<'PY' >"$E/asan-portrait.log" 2>&1
import pathlib,subprocess,time,json
w=pathlib.Path('/workspace/shared/gimp-clean-aggregate');e=pathlib.Path('/workspace/shared/prepared-pixels-clean-followup');start=time.monotonic();cmd=[str(w/'build/prepared-pixels-sanitizers/painter-prepared-pixels'),'--tap'];p=subprocess.run(cmd,capture_output=True,text=True,timeout=120);(e/'asan-results.json').write_text(json.dumps(dict(command=cmd,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr,duration=time.monotonic()-start,timeout=120),indent=2)+'\n');print(p.stdout);print(p.stderr);assert p.returncode==0
PY
python3 - "$W/source" "$E" <<'PY'
import pathlib,sys,json,hashlib,re
s,e=map(pathlib.Path,sys.argv[1:]);m=json.loads((e/'source-manifest.json').read_text());assert all(hashlib.sha256((s/p).read_bytes()).hexdigest()==h for p,h in m['input_sha256'].items())
a=json.loads((e/'asan-build.json').read_text());assert len(a['instrumented_sources'])==6 and not a['changed_during_build'];assert all(m['input_sha256'][p]==h for p,h in a['sources_sha256'].items())
n=json.loads((e/'normal.json').read_text())[0];b=json.loads((e/'asan-results.json').read_text());assert n['returncode']==b['returncode']==0
for row in [n,b]:assert re.search(r'^1\.\.15$',row['stdout'],re.M);assert len(re.findall(r'^ok \d+',row['stdout'],re.M))==15
(e/'verified.json').write_text(json.dumps({'status':'PASS','normal_cases':15,'selected_sanitizer_cases':15,'instrumented_units':6,'four_held_paths_excluded':True,'inputs_unchanged':True,'oncanvas_enabled_asserted_by_committed_test':True,'portrait':'1080x1920x24 authenticated','rendered_ui_claim':False},indent=2)+'\n')
PY
