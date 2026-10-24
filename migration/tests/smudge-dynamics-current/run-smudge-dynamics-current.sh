#!/bin/bash
set -euo pipefail
D=/workspace/shared/gimp-native-secondary;R=/workspace/scratch/0015fc5d1a03/gimp-painter
source "$D/env.sh";exec 9>"$D/build.lock";flock 9
O="$D/smudge-dynamics-current/final";mkdir -p "$O"
python3 - <<'PY'
import pathlib,json,hashlib,shutil,tarfile,subprocess
R=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');D=pathlib.Path('/workspace/shared/gimp-native-secondary');S=D/'source';O=D/'smudge-dynamics-current/final'
h=json.loads((R/'migration/tests/smudge-alpha-centers/final/held-source-isolation.json').read_text())
for row in h['rows']:
 p=S/row['path']
 if row.get('native_exists')is False:assert not p.exists()
 else:assert hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256']
(O/'held-source-isolation.json').write_text(json.dumps(h,indent=2)+'\n');shutil.copyfile(R/'migration/tests/run_dynamics_current_sanitizers.py',S/'migration/tests/run_dynamics_current_sanitizers.py')
paths=['app/core/gimpdynamics.c','app/core/gimpdynamics.h','app/core/gimpdynamicsoutput.c','app/core/gimpdynamicsoutput.h','app/core/gimpcurve.c','app/core/gimpcurve.h','app/core/core-enums.c','app/core/core-enums.h','app/tests/test-painter-dynamics.c','app/tests/test-painter-brush-geometry.cpp','app/paint/gimppaintoptions.c','app/paint/gimppaintoptions.h','app/paint/gimppainterbrushgeometry.h','migration/fixtures/legacy-brush-geometry/default-dynamics.tsv.gz','migration/tests/run_dynamics_sanitizers.py','migration/tests/run_dynamics_current_sanitizers.py','app/tests/meson.build']+[r['path']for r in h['rows']if r.get('native_exists')is not False]
seal={n:hashlib.sha256((S/n).read_bytes()).hexdigest()for n in sorted(set(paths))};(O/'source-before-build.json').write_text(json.dumps(seal,indent=2)+'\n')
with tarfile.open(O/'tested-sources.tar.gz','w:gz')as t:
 for n in seal:t.add(S/n,arcname=n)
# Relevant function equivalence, not whole-file current-HEAD equivalence.
def extract(s,needle):
 a=s.index(needle);b=s.index('{',a);depth=1;i=b+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
items=[]
for n,needle in [('app/core/core-enums.c','GType\ngimp_dynamics_output_type_get_type (void)'),('app/paint/gimppaintoptions.c','gboolean\ngimp_painter_brush_geometry_options (GimpPaintOptions *options)')]:
 a=extract((R/n).read_text(),needle);b=extract((S/n).read_text(),needle);assert a==b;items.append({'path':n,'function':needle,'equal':True,'body_sha256':hashlib.sha256(a.encode()).hexdigest(),'body':a,'main_sha256':hashlib.sha256((R/n).read_bytes()).hexdigest(),'private_sha256':seal[n]})
whole=[]
for n in ['app/core/gimpdynamics.c','app/core/gimpdynamicsoutput.c','app/core/gimpcurve.c','app/tests/test-painter-dynamics.c','app/tests/test-painter-brush-geometry.cpp']:
 assert (R/n).read_bytes()==(S/n).read_bytes();whole.append(n)
(O/'current-equivalence.json').write_text(json.dumps({'main_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),'scope':'Read-only relevant functions and specified whole units; private held providers remain earlier committed baselines, not exact current-HEAD whole-app proof.','whole_equal':whole,'functions':items},indent=2)+'\n');print(len(seal),'before-build source inputs')
PY
ninja -j2 -C "$D/build" app/tests/painter-dynamics app/tests/painter-brush-geometry > "$O/normal-build.log" 2>&1
python3 "$D/source/tools/run-painter-xvfb.py" meson test -C "$D/build" --no-rebuild --print-errorlogs painter-dynamics > "$O/normal-dynamics.log" 2>&1
python3 - <<'PY'
import pathlib,json,hashlib
D=pathlib.Path('/workspace/shared/gimp-native-secondary');O=D/'smudge-dynamics-current/final';j=json.loads((D/'build/meson-logs/testlog.json').read_text().splitlines()[0]);r={k:j.get(k,'')for k in ['name','result','returncode','duration','stdout','stderr']};r['executable_sha256']=hashlib.sha256((D/'build/app/tests/painter-dynamics').read_bytes()).hexdigest();(O/'normal-dynamics.json').write_text(json.dumps(r,indent=2)+'\n');assert r['result']=='OK'
PY
python3 "$D/source/tools/run-painter-xvfb.py" meson test -C "$D/build" --no-rebuild --print-errorlogs --test-args='-p /painter/geometry/default-dynamics' painter-brush-geometry > "$O/normal-neutral.log" 2>&1
python3 - <<'PY'
import pathlib,json,hashlib
D=pathlib.Path('/workspace/shared/gimp-native-secondary');O=D/'smudge-dynamics-current/final';j=json.loads((D/'build/meson-logs/testlog.json').read_text().splitlines()[0]);r={k:j.get(k,'')for k in ['name','result','returncode','duration','stdout','stderr']};r['executable_sha256']=hashlib.sha256((D/'build/app/tests/painter-brush-geometry').read_bytes()).hexdigest();(O/'normal-neutral.json').write_text(json.dumps(r,indent=2)+'\n');assert r['result']=='OK'
PY
python3 "$D/source/tools/run-painter-xvfb.py" python3 "$D/source/migration/tests/run_dynamics_current_sanitizers.py" "$D/build" --report "$O/sanitizers.json" > "$O/sanitizers.log" 2>&1
