#!/bin/bash
set -euo pipefail
source <native>/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK";flock 9
python3 - <<'PY'
import hashlib,json,pathlib,subprocess
src=pathlib.Path('<native>/source');build=src.parent/'build-port';out=pathlib.Path('<evidence>');exe=build/'app/tests/gimp-painter-surface';rows=[]
for n in range(3):
 before=hashlib.sha256(exe.read_bytes()).hexdigest();name='dirty-verified-repeat-'+str(n)
 r=subprocess.run(['python3',str(src/'tools/run-painter-xvfb.py'),'meson','test','-C',str(build),'--no-rebuild','--print-errorlogs','--logbase',name,'gimp-painter-surface'],capture_output=True,text=True,timeout=90);assert r.returncode==0,(r.stdout,r.stderr)
 rs=[json.loads(l) for l in (build/'meson-logs'/(name+'.json')).read_text().splitlines() if l.strip()];assert len(rs)==1 and rs[0]['result']=='OK' and rs[0]['stdout'].count('\nok ')==20,rs
 after=hashlib.sha256(exe.read_bytes()).hexdigest();assert before==after
 rows.append(dict(binary_sha256=before,logbase=name,result='OK',tap_count=20,stdout=rs[0]['stdout'],stderr=rs[0]['stderr']))
(out/'repeat20.json').write_text(json.dumps(rows,indent=2)+'\n');print('Three distinct20-case repeats PASS, binary SHA unchanged throughout each')
PY
