#!/bin/bash
set -euo pipefail
D=/workspace/shared/gimp-native-secondary
source "$D/env.sh"
exec 9>"$D/build.lock";flock 9
python3 "$D/source/tools/run-painter-xvfb.py" python3 - <<'PY'
import pathlib,json,subprocess,hashlib,tarfile
D=pathlib.Path('/workspace/shared/gimp-native-secondary');S=D/'source';B=D/'build';R=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');O=D/'smudge-blending-output/final';O.mkdir(parents=True,exist_ok=True)
def sha(b):return hashlib.sha256(b).hexdigest()
def dump(p,j):p.write_text(json.dumps(j,indent=2)+'\n')
seal=json.loads((R/'migration/tests/smudge-alpha-centers/final/source-before-build.json').read_text())
for n,h in seal.items():assert sha((S/n).read_bytes())==h,n
held=json.loads((R/'migration/tests/smudge-alpha-centers/final/held-source-isolation.json').read_text())
for row in held['rows']:
 p=S/row['path']
 if row.get('native_exists')is False:assert not p.exists()
 else:assert sha(p.read_bytes())==row['sha256']
dump(O/'held-source-isolation.json',held);dump(O/'source-before-build.json',seal)
with tarfile.open(O/'tested-sources.tar.gz','w:gz')as t:
 for n in seal:t.add(S/n,arcname=n)
p=S/'app/paint/gimppaintersmudge.cpp';original=p.read_bytes()
def build(q,targets):
 r=subprocess.run(['ninja','-j2','-C',str(B),*targets],capture_output=True);(q/'build.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0
try:
 for name,old,new in [
 ('flow-alias','const double blending = gimp_dynamics_get_linear_value (brush->dynamics, GIMP_DYNAMICS_OUTPUT_BLENDING,','const double blending = gimp_dynamics_get_linear_value (brush->dynamics, GIMP_DYNAMICS_OUTPUT_FLOW,'),
 ('shade-feedback','    pixels = shaded.data ();','    gegl_buffer_set (state.accumulator.get (), &accum_area, 0, native, shaded.data (), GEGL_AUTO_ROWSTRIDE);\n    pixels = shaded.data ();')]:
  q=O/name;q.mkdir(exist_ok=True);assert original.count(old.encode())==1;mut=original.replace(old.encode(),new.encode());p.write_bytes(mut);(q/'mutated.cpp').write_bytes(mut);pins=dict(seal);pins['app/paint/gimppaintersmudge.cpp']=sha(mut);dump(q/'source-before-build.json',pins)
  build(q,['app/tests/painter-smudge-trace'])
  r=subprocess.run(['meson','test','-C',str(B),'--no-rebuild','--print-errorlogs','painter-smudge-legacy-strokes'],capture_output=True);(q/'run.log').write_bytes(r.stdout+r.stderr)
  j=json.loads((B/'meson-logs/testlog.json').read_text().splitlines()[0]);row={k:j.get(k,'')for k in ['name','result','returncode','duration','stdout','stderr']};row['executable_sha256']=sha((B/'app/tests/painter-smudge-trace').read_bytes());dump(q/'result.json',row);assert r.returncode!=0 and row['result']=='FAIL'
  p.write_bytes(original)
finally:p.write_bytes(original)
build(O,['app/tests/gimp-painter-smudge','app/tests/painter-smudge-trace','app/paint/painter-smudge/painter-smudge-pixels-trace'])
r=subprocess.run(['meson','test','-C',str(B),'--no-rebuild','--num-processes=1','--print-errorlogs','gimp-painter-smudge','painter-smudge-legacy-strokes','painter-smudge-owned-strokes','painter-smudge-legacy-pixels'],capture_output=True);(O/'normal.log').write_bytes(r.stdout+r.stderr)
rows=[{k:j.get(k,'')for k in ['name','result','returncode','duration','stdout','stderr']}for j in map(json.loads,(B/'meson-logs/testlog.json').read_text().splitlines())];dump(O/'normal.json',rows);assert r.returncode==0 and len(rows)==4 and all(x['result']=='OK'and x['duration']<60 for x in rows)
for n,h in seal.items():assert sha((S/n).read_bytes())==h,n
print('PASS native causal controls and exact restored four registrations')
PY
O="$D/smudge-blending-output/final"
python3 "$D/source/tools/run-painter-xvfb.py" python3 "$D/source/migration/tests/run_smudge_alpha_sanitizers.py" "$D/build" --report "$O/focused-native.json" > "$O/focused-native.log" 2>&1
