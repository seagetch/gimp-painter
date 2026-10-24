#!/bin/bash
set -euo pipefail
R=/workspace/scratch/0015fc5d1a03/gimp-painter
W=/workspace/shared/gimp-clean-aggregate
E=/workspace/shared/options-readiness
mkdir -p "$E/red" "$E/green"
exec 9>"$W/build.lock";flock 9
source "$W/env.sh"
python3 - "$R" "$W/source" "$E" <<'PY'
from pathlib import Path
import json,hashlib,subprocess,tarfile,sys
r,s,e=map(Path,sys.argv[1:]);base='2ef2dd32eeb7201e8e0318f169c6005a21c10410';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
held=['app/widgets/gimpcellrendererpopup.cpp','app/display/gimppaintercanvasui.cpp','app/core/gimppaintermybrush.cpp','app/core/gimpviewable.c','app/widgets/gimpviewrenderer.c','app/tools/gimpfillbrushtool.cpp','app/tools/gimpbucketfillbrushtool.h','app/tools/gimp-tools.c','app/core/gimpclonelayer.cpp','app/core/gimpclonelayer.h','app/core/gimpclonelayer-handle.hpp','app/core/gimpclonelayerundo.h']
for p in held:assert (s/p).read_bytes()==subprocess.check_output(['git','-C',str(r),'show',base+':'+p]),p
owned=['app/core/gimpcontext.c','app/paint/painter-mypaint-surface/gimp-painter-options.cpp','app/paint/painter-mypaint-surface/gimp-painter-options.hpp','app/paint/gimpmypaintoptions.h','app/tests/test-mypaint-options-readiness.cpp','app/tests/test-mypaint-options-readiness-c.c'];shared='app/tests/meson.build'
allhash=lambda:{str(p.relative_to(s)):sha(p) for p in s.rglob('*') if p.is_file() and '__pycache__' not in p.parts and '.git' not in p.parts}
(e/'prior-source.json').write_text(json.dumps(allhash(),indent=2)+'\n')
with tarfile.open(e/'prior-owned.tar.gz','w:gz') as t:
 for p in owned+[shared]:
  if (s/p).exists():t.add(s/p,arcname=p)
for p in owned:
 if p=='app/core/gimpcontext.c':continue
 data=Path('/workspace/shared/options-readiness-before-reaffirm.cpp').read_bytes() if p.endswith('gimp-painter-options.cpp') else (r/p).read_bytes()
 if not (s/p).exists() or (s/p).read_bytes()!=data:(s/p).write_bytes(data)
text=(r/shared).read_text();start=text.index('# Original readiness is successful selection history, not draft validity.');end=text.index('timeout: 120)',start)+len('timeout: 120)');stanza=text[start:end];old=(s/shared).read_text();assert 'mypaint_options_readiness =' not in old;(s/shared).write_text(old+'\n'+stanza+'\n')
inputs=owned+[shared,'app/core/gimpcontext.h','app/painter/binding-store.cpp','app/painter/binding-store.hpp','app/paint/painter-mypaint-surface/gimp-painter-options.h']
m={'base':base,'scope':'explicit readiness precursor overlay, no same-resource notification yet; not whole HEAD','held_exact_base':held,'owned_sha256':{p:sha(s/p) for p in owned},'input_sha256':{p:sha(s/p) for p in inputs},'complete_source_sha256':allhash()};(e/'red/source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with tarfile.open(e/'red/tested-inputs.tar.gz','w:gz') as t:
 for p in inputs:t.add(s/p,arcname=p)
PY
ninja -C "$W/build" -j2 app/tests/mypaint-options-readiness >"$E/red/build.log" 2>&1
export GIMP_TESTING_ABS_TOP_SRCDIR="$W/source" GIMP_TESTING_ABS_TOP_BUILDDIR="$W/build" GIMP_TESTING_PLUGINDIRS="$W/build/plug-ins/common" UI_TEST=yes
python3 "$W/source/tools/run-painter-xvfb.py" python3 - "$W/build/app/tests/mypaint-options-readiness" "$E/red" <<'PY' >"$E/red/portrait.log" 2>&1
import subprocess,json,sys
from pathlib import Path
p=subprocess.run([sys.argv[1],'-p','/original-options-readiness/same-source-adopted'],capture_output=True,text=True,timeout=60);Path(sys.argv[2]+'/result.json').write_text(json.dumps({'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr},indent=2)+'\n');print(p.stdout,p.stderr);assert p.returncode!=0 and 'options_readiness_c_consumer(o)' in p.stderr+p.stdout
PY
python3 - "$R" "$W/source" "$E" <<'PY'
from pathlib import Path
import json,hashlib,sys,tarfile
r,s,e=map(Path,sys.argv[1:]);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();m=json.loads((e/'red/source-manifest.json').read_text());assert all(sha(s/p)==h for p,h in m['input_sha256'].items())
(e/'red/source-after.json').write_text(json.dumps({'recorded_inputs_unchanged':True})+'\n')
for p in ['app/core/gimpcontext.c','app/paint/painter-mypaint-surface/gimp-painter-options.cpp']:(s/p).write_bytes((r/p).read_bytes())
m['scope']='explicit final readiness overlay on exact2ef source';m['owned_sha256']={p:sha(s/p) for p in m['owned_sha256']};m['input_sha256']={p:sha(s/p) for p in m['input_sha256']};m['complete_source_sha256']={str(p.relative_to(s)):sha(p) for p in s.rglob('*') if p.is_file() and '__pycache__' not in p.parts and '.git' not in p.parts};(e/'green/source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with tarfile.open(e/'green/tested-inputs.tar.gz','w:gz') as t:
 for p in m['input_sha256']:t.add(s/p,arcname=p)
PY
ninja -C "$W/build" -j2 app/tests/mypaint-options-readiness app/tests/mypaint-options-mode app/tests/gimp-painter-options app/tests/mypaint-enum-public libgimpbase/mypaint-enum-library-only >"$E/green/build.log" 2>&1
python3 "$W/source/tools/run-painter-xvfb.py" python3 - "$W/build/app/tests/mypaint-options-readiness" "$E/green" <<'PYRED' >"$E/green/blanket-red-portrait.log" 2>&1
import subprocess,json,sys
from pathlib import Path
rows=[]
for case in ['constructor-nested-different','constructor-nested-same','blocked-observer-no-toggle']:
 p=subprocess.run([sys.argv[1],'-p','/original-options-readiness/'+case],capture_output=True,text=True,timeout=60);rows.append({'case':case,'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr});Path(sys.argv[2]+'/blanket-red-results.json').write_text(json.dumps(rows,indent=2)+'\n');print(case,p.stdout,p.stderr);assert p.returncode!=0 and ('options_readiness_c_consumer(o)' in p.stderr+p.stdout or 'toggle.calls' in p.stderr+p.stdout)
PYRED
python3 - "$W/source" "$E/green" <<'PY'
from pathlib import Path
import json,hashlib,sys
s,e=map(Path,sys.argv[1:]);m=json.loads((e/'source-manifest.json').read_text());after={str(p.relative_to(s)):hashlib.sha256(p.read_bytes()).hexdigest() for p in s.rglob('*') if p.is_file() and '__pycache__' not in p.parts and '.git' not in p.parts};assert after==m['complete_source_sha256'];(e/'source-after.json').write_text(json.dumps({'all_source_unchanged':True})+'\n')
PY
