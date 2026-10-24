#!/bin/bash
set -euo pipefail
D=/workspace/shared/gimp-native-secondary
source "$D/env.sh";exec 9>"$D/build.lock";flock 9
python3 "$D/source/tools/run-painter-xvfb.py" python3 - <<'PY'
import pathlib,json,subprocess,os,gzip,hashlib,time
D=pathlib.Path('/workspace/shared/gimp-native-secondary');S=D/'source';B=D/'build';O=D/'smudge-alpha-centers/final'
seal=json.loads((O/'source-before-build.json').read_text())
for n,h in seal.items():assert hashlib.sha256((S/n).read_bytes()).hexdigest()==h,n
expected=gzip.decompress((S/'migration/fixtures/legacy-paper-smudge/pixels.tsv.gz').read_bytes());rows=[]
env={**os.environ,'GIMP_TESTING_ABS_TOP_SRCDIR':str(S),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(B),'GIMP_TESTING_PLUGINDIRS':str(B/'plug-ins/common'),'UI_TEST':'yes','GIMP3_DIRECTORY':str(B/'app/tests/gimpdir-output'),'PAINTER_SMUDGE_PAPER_FIXTURE':'1'}
for args in [[],['owned']]:
 start=time.monotonic();run=subprocess.run([str(B/'app/tests/painter-smudge-trace'),*args],env=env,cwd=B,capture_output=True,timeout=60);actual=b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(b'SMUDGE ')or x==b'SMUDGE_CAPTURE_COMPLETE')+b'\n';row={'args':args,'exit_code':run.returncode,'duration':time.monotonic()-start,'bytes':len(actual),'records':len(actual.splitlines()),'exact':actual==expected,'stderr':run.stderr.decode(),'normalized_sha256':hashlib.sha256(actual).hexdigest(),'expected_sha256':hashlib.sha256(expected).hexdigest(),'executable_sha256':hashlib.sha256((B/'app/tests/painter-smudge-trace').read_bytes()).hexdigest()};rows.append(row)
(O/'normal-paper.json').write_text(json.dumps(rows,indent=2)+'\n');print(rows);assert all(r['exit_code']==0 and r['exact'] and r['records']==193 and r['duration']<60 for r in rows)
PY
