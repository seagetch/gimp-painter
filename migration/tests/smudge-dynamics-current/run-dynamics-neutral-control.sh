#!/bin/bash
set -euo pipefail
D=/workspace/shared/gimp-native-secondary;source "$D/env.sh";exec 9>"$D/build.lock";flock 9
python3 "$D/source/tools/run-painter-xvfb.py" python3 - <<'PY'
import pathlib,subprocess,json,hashlib
D=pathlib.Path('/workspace/shared/gimp-native-secondary');S=D/'source';B=D/'build';O=D/'smudge-dynamics-current/final';q=O/'neutral-causal-control';q.mkdir(exist_ok=True);p=S/'app/core/gimpdynamicsoutput.c';b=p.read_bytes();seal=json.loads((O/'source-before-build.json').read_text());assert hashlib.sha256(b).hexdigest()==seal['app/core/gimpdynamicsoutput.c'];needle=b'if (! options || ! gimp_painter_brush_geometry_options (options) ||';assert b.count(needle)==1;mut=b.replace(needle,b'if (TRUE || ! options || ! gimp_painter_brush_geometry_options (options) ||')
try:
 p.write_bytes(mut);(q/'mutant.c').write_bytes(mut);pins=dict(seal);pins['app/core/gimpdynamicsoutput.c']=hashlib.sha256(mut).hexdigest();(q/'source-before-build.json').write_text(json.dumps(pins,indent=2)+'\n')
 run=subprocess.run(['ninja','-j2','-C',str(B),'app/tests/painter-brush-geometry'],capture_output=True);(q/'build.log').write_bytes(run.stdout+run.stderr);assert run.returncode==0
 run=subprocess.run(['meson','test','-C',str(B),'--no-rebuild','--print-errorlogs','--test-args=-p /painter/geometry/default-dynamics','painter-brush-geometry'],capture_output=True);(q/'run.log').write_bytes(run.stdout+run.stderr);j=json.loads((B/'meson-logs/testlog.json').read_text().splitlines()[0]);row={k:j.get(k,'')for k in ['name','result','returncode','duration','stdout','stderr']};row['executable_sha256']=hashlib.sha256((B/'app/tests/painter-brush-geometry').read_bytes()).hexdigest();(q/'result.json').write_text(json.dumps(row,indent=2)+'\n');assert run.returncode!=0 and row['result']=='FAIL'and 'original_default_dynamics'in row['stdout']+row['stderr']
finally:p.write_bytes(b)
run=subprocess.run(['ninja','-j2','-C',str(B),'app/tests/painter-dynamics','app/tests/painter-brush-geometry'],capture_output=True);(q/'restored-build.log').write_bytes(run.stdout+run.stderr);assert run.returncode==0
run=subprocess.run(['meson','test','-C',str(B),'--no-rebuild','--print-errorlogs','--test-args=-p /painter/geometry/default-dynamics','painter-brush-geometry'],capture_output=True);(q/'restored-run.log').write_bytes(run.stdout+run.stderr);j=json.loads((B/'meson-logs/testlog.json').read_text().splitlines()[0]);row={k:j.get(k,'')for k in ['name','result','returncode','duration','stdout','stderr']};row['executable_sha256']=hashlib.sha256((B/'app/tests/painter-brush-geometry').read_bytes()).hexdigest();(q/'restored-result.json').write_text(json.dumps(row,indent=2)+'\n');assert run.returncode==0 and row['result']=='OK'
for n,h in seal.items():assert hashlib.sha256((S/n).read_bytes()).hexdigest()==h,n
print('PASS: exact neutral mapping bypass rejected; original sources restored and registered curve passes')
PY
