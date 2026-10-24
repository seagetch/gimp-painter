#!/bin/bash
set -euo pipefail
D=/workspace/shared/gimp-native-secondary;source "$D/env.sh";exec 9>"$D/build.lock";flock 9
python3 "$D/source/tools/run-painter-xvfb.py" python3 - <<'PY'
import pathlib,json,subprocess,hashlib
D=pathlib.Path('/workspace/shared/gimp-native-secondary');S=D/'source';B=D/'build';O=D/'smudge-alpha-centers/final';seal=json.loads((O/'source-before-build.json').read_text());results=[]
for n,h in seal.items():assert hashlib.sha256((S/n).read_bytes()).hexdigest()==h,n
for name,rel,old,new in [('alpha-plus-one','migration/fixtures/legacy-smudge-center/blend-pixels.inc','const guint blend2 = blend + 1;','const guint blend2 = blend;'),('replace-denominator','migration/fixtures/legacy-smudge-center/replace-pixels.inc','1.0 / 65536.0','1.0 / 65025.0')]:
 p=S/rel;b=p.read_bytes();assert old.encode()in b;mut=b.replace(old.encode(),new.encode());q=O/'mutants'/name;q.mkdir(parents=True,exist_ok=True);(q/'mutated-oracle.inc').write_bytes(mut)
 try:
  p.write_bytes(mut);pins=dict(seal);pins[rel]=hashlib.sha256(mut).hexdigest();(q/'source-before-build.json').write_text(json.dumps(pins,indent=2)+'\n')
  build=subprocess.run(['ninja','-j2','-C',str(B),'app/tests/gimp-painter-smudge'],capture_output=True);(q/'build.log').write_bytes(build.stdout+build.stderr);assert build.returncode==0
  run=subprocess.run(['meson','test','-C',str(B),'--no-rebuild','--print-errorlogs','--test-args=-p /painter/smudge/alpha-centers','gimp-painter-smudge'],capture_output=True);(q/'run.log').write_bytes(run.stdout+run.stderr)
  j=json.loads((B/'meson-logs/testlog.json').read_text().splitlines()[0]);row={k:j.get(k,'')for k in ['name','result','returncode','duration','stdout','stderr']};row.update(mutant=name,changed_path=rel,executable_sha256=hashlib.sha256((B/'app/tests/gimp-painter-smudge').read_bytes()).hexdigest());(q/'result.json').write_text(json.dumps(row,indent=2)+'\n');assert run.returncode!=0 and row['result']=='FAIL'and 'center mismatch'in row['stdout'];results.append(name)
 finally:p.write_bytes(b)
run=subprocess.run(['ninja','-j2','-C',str(B),'app/tests/gimp-painter-smudge'],capture_output=True);(O/'restored-build.log').write_bytes(run.stdout+run.stderr);assert run.returncode==0
run=subprocess.run(['meson','test','-C',str(B),'--no-rebuild','--print-errorlogs','gimp-painter-smudge'],capture_output=True);(O/'restored-run.log').write_bytes(run.stdout+run.stderr);assert run.returncode==0
j=json.loads((B/'meson-logs/testlog.json').read_text().splitlines()[0]);row={k:j.get(k,'')for k in ['name','result','returncode','duration','stdout','stderr']};row['executable_sha256']=hashlib.sha256((B/'app/tests/gimp-painter-smudge').read_bytes()).hexdigest();(O/'restored-result.json').write_text(json.dumps(row,indent=2)+'\n')
for n,h in seal.items():assert hashlib.sha256((S/n).read_bytes()).hexdigest()==h,n
print('PASS restored; rejected',results)
PY
