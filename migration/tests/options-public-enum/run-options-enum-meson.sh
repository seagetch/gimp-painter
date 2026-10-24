#!/bin/bash
set -euo pipefail
R=/workspace/scratch/0015fc5d1a03/gimp-painter
W=/workspace/shared/gimp-clean-aggregate
E=/workspace/shared/options-enum-meson
mkdir -p "$E"
exec 9>"$W/build.lock";flock 9
source "$W/env.sh"
python3 - "$R" "$W/source" "$E" <<'PY'
from pathlib import Path
import json,hashlib,tarfile,subprocess,sys
r,s,e=map(Path,sys.argv[1:]);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();base='20b2204b11df079c306a680b41ac623f3be75c76'
held=['app/widgets/gimpcellrendererpopup.cpp','app/display/gimppaintercanvasui.cpp','app/core/gimppaintermybrush.cpp','app/core/gimpviewable.c','app/widgets/gimpviewrenderer.c','app/tools/gimpfillbrushtool.cpp','app/tools/gimpbucketfillbrushtool.h','app/tools/gimp-tools.c','app/core/gimpclonelayer.cpp','app/core/gimpclonelayer.h','app/core/gimpclonelayer-handle.hpp','app/core/gimpclonelayerundo.h']
for p in held:assert (s/p).read_bytes()==subprocess.check_output(['git','-C',str(r),'show',base+':'+p]),p
owned=['libgimpbase/gimpbaseenums.h','libgimpbase/gimpbaseenums.c','libgimpbase/gimpbase.def','libgimp/gimpenums.c.tail','pdb/enums.pl','app/paint/gimpmypaintoptions.h','app/paint/painter-mypaint-surface/gimp-painter-options.cpp','app/tests/test-mypaint-enum-public.cpp','app/tests/test-mypaint-enum-public-c.c']
shared=['libgimpbase/meson.build','app/tests/meson.build']
allfiles=lambda:{str(p.relative_to(s)):sha(p) for p in s.rglob('*') if p.is_file() and '__pycache__' not in p.parts and '.git' not in p.parts}
before=allfiles();(e/'source-before.json').write_text(json.dumps(before,indent=2)+'\n')
with tarfile.open(e/'prior-overwritten-inputs.tar.gz','w:gz') as t:
 for p in owned+shared:
  if (s/p).exists():t.add(s/p,arcname=p)
for p in owned:(s/p).write_bytes((r/p).read_bytes())
markers={'libgimpbase/meson.build':('# Public original MyPaint enum entry must work without linking the application.','timeout: 30)','mypaint_enum_library ='),'app/tests/meson.build':('# Original public library enum and app Options must share one registration.','timeout: 120)','mypaint_enum_public =')}
for p,(marker,endmarker,key) in markers.items():
 text=(r/p).read_text();start=text.index(marker);end=text.index(endmarker,start)+len(endmarker);stanza=text[start:end];old=(s/p).read_text()
 if key in old:assert stanza in old
 else:(s/p).write_text(old+'\n'+stanza+'\n')
after=allfiles();changed=sorted(p for p,h in after.items() if before.get(p)!=h);assert set(changed)<=set(owned+shared),changed
(e/'source-overlay.json').write_text(json.dumps({'base':base,'scope':'explicit enum overlay after committed mode and Filter rotate/matrix overlays; not whole HEAD','owned_sha256':{p:sha(s/p) for p in owned},'shared_sha256':{p:sha(s/p) for p in shared},'held_exact_base':held,'changed_source_paths':changed,'complete_source_sha256':after},indent=2)+'\n')
with tarfile.open(e/'tested-inputs.tar.gz','w:gz') as t:
 for p in owned+shared+['app/paint/painter-mypaint-surface/gimp-painter-options.hpp','app/tests/test-mypaint-options-mode.cpp','app/tests/test-mypaint-options-mode-c.c','app/painter/resources.hpp','app/painter/binding-store.cpp','tools/meson-mkenums.py','tools/gimp-mkenums','meson.build']:t.add(s/p,arcname=p)
PY
printf 'REAL MESON enum build starts %s\n' "$(date -u +%FT%TZ)"
ninja -C "$W/build" -j2 libgimpbase/mypaint-enum-library-only libgimp/libgimp-3.0.so.0.8.1 app/tests/mypaint-enum-public app/tests/mypaint-options-mode app/tests/gimp-painter-options >"$E/build.log" 2>&1
python3 "$W/source/tools/run-painter-xvfb.py" meson test -C "$W/build" mypaint-enum-library-only mypaint-enum-public mypaint-options-mode gimp-painter-options --no-rebuild --print-errorlogs --logbase options-enum-meson >"$E/normal-portrait.log" 2>&1
python3 - "$W/build/meson-logs/options-enum-meson.json" "$E/normal.json" <<'PY'
import json,sys
rows=[json.loads(x) for x in open(sys.argv[1])];json.dump([{k:r.get(k) for k in ['name','result','returncode','stdout','stderr','duration','command']} for r in rows],open(sys.argv[2],'w'),indent=2)
PY
nm -D "$W/build/libgimpbase/libgimpbase-3.0.so.0.8.1" | grep gimp_mypaint_brush_mode_get_type >"$E/base-symbol.txt"
nm -D "$W/build/libgimp/libgimp-3.0.so.0.8.1" | grep gimp_mypaint_brush_mode_get_type >"$E/libgimp-symbol.txt"
python3 "$W/source/tools/check_painter_headers.py" --build-dir "$W/build" --report "$E/headers.json" >"$E/headers.log" 2>&1
python3 - "$R" "$W/source" "$E" <<'PY'
from pathlib import Path
import json,hashlib,sys
r,s,e=map(Path,sys.argv[1:]);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();m=json.loads((e/'source-overlay.json').read_text());after={str(p.relative_to(s)):sha(p) for p in s.rglob('*') if p.is_file() and '__pycache__' not in p.parts and '.git' not in p.parts};assert after==m['complete_source_sha256'],[p for p,h in after.items() if m['complete_source_sha256'].get(p)!=h];assert (r/'libgimpbase/gimpbaseenums.c').read_bytes()==(s/'libgimpbase/gimpbaseenums.c').read_bytes();(e/'source-after.json').write_text(json.dumps({'all_source_content_unchanged':True,'generated_enum_matches_private_generator':True,'generated_enum_sha256':sha(s/'libgimpbase/gimpbaseenums.c')},indent=2)+'\n')
PY
