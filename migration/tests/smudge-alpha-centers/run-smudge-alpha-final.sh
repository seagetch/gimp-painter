#!/bin/bash
set -euo pipefail
D=/workspace/shared/gimp-native-secondary;R=/workspace/scratch/0015fc5d1a03/gimp-painter
source "$D/env.sh";exec 9>"$D/build.lock";flock 9
O="$D/smudge-alpha-centers/final";mkdir -p "$O"
python3 - <<'PY'
import pathlib,json,hashlib,shutil,tarfile
D=pathlib.Path('/workspace/shared/gimp-native-secondary');R=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');S=D/'source';O=D/'smudge-alpha-centers/final'
h=json.loads((D/'smudge-center-matrix/diagnostic9/held-source-isolation.json').read_text())
for row in h['rows']:
 p=S/row['path']
 if row.get('native_exists') is False:assert not p.exists()
 else:assert hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256']
(O/'held-source-isolation.json').write_text(json.dumps(h,indent=2)+'\n')
owned=['app/tests/test-gimp-painter-smudge.cpp','app/tests/test-painter-smudge-center-matrix.inc','app/tests/test-painter-smudge-alpha-centers.inc','migration/tests/run_smudge_alpha_sanitizers.py']+[str(p.relative_to(R))for p in (R/'migration/fixtures/legacy-smudge-center').iterdir() if p.is_file()]
for n in owned:(S/n).parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(R/n,S/n)
paths=set(json.loads((R/'migration/tests/smudge-mask-format/final/source-before-build.json').read_text()))|set(owned)|{'app/paint/gimppainterpaper.cpp','app/paint/gimppainterpaper.h','app/paint/gimpbrushcore-loops.cc','migration/fixtures/legacy-paper-smudge/pixels.tsv.gz'}
seal={n:hashlib.sha256((S/n).read_bytes()).hexdigest() for n in sorted(paths)};(O/'source-before-build.json').write_text(json.dumps(seal,indent=2)+'\n')
with tarfile.open(O/'tested-sources.tar.gz','w:gz') as t:
 for n in seal:t.add(S/n,arcname=n)
print('before build inputs',len(seal))
PY
ninja -j2 -C "$D/build" app/tests/gimp-painter-smudge app/tests/painter-smudge-trace app/paint/painter-smudge/painter-smudge-pixels-trace > "$O/normal-build.log" 2>&1
python3 "$D/source/tools/run-painter-xvfb.py" meson test -C "$D/build" --no-rebuild --num-processes=1 --print-errorlogs gimp-painter-smudge painter-smudge-legacy-strokes painter-smudge-owned-strokes painter-smudge-legacy-pixels > "$O/normal.log" 2>&1
python3 - <<'PY'
import pathlib,json,hashlib
D=pathlib.Path('/workspace/shared/gimp-native-secondary');O=D/'smudge-alpha-centers/final';seal=json.loads((O/'source-before-build.json').read_text());rows=[]
for n,h in seal.items():assert hashlib.sha256((D/'source'/n).read_bytes()).hexdigest()==h,n
for line in (D/'build/meson-logs/testlog.json').read_text().splitlines():
 j=json.loads(line);rows.append({k:j.get(k,'')for k in ['name','result','returncode','duration','stdout','stderr']})
(O/'normal.json').write_text(json.dumps(rows,indent=2)+'\n');assert len(rows)==4 and all(r['result']=='OK' and r['duration']<60 for r in rows)
(O/'normal-executables.json').write_text(json.dumps({p:hashlib.sha256((D/'build'/p).read_bytes()).hexdigest()for p in ['app/tests/gimp-painter-smudge','app/tests/painter-smudge-trace','app/paint/painter-smudge/painter-smudge-pixels-trace']},indent=2)+'\n');print([(r['name'],r['duration'])for r in rows])
PY
python3 "$D/source/tools/run-painter-xvfb.py" python3 "$D/source/migration/tests/run_smudge_alpha_sanitizers.py" "$D/build" --report "$O/focused-native.json" > "$O/focused-native.log" 2>&1
python3 "$D/source/migration/tests/run_smudge_pixels_sanitizers.py" "$D/build" --report "$O/focused-pixels.json" > "$O/focused-pixels.log" 2>&1
