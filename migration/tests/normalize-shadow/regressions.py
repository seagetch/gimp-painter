from pathlib import Path
import json,subprocess,hashlib,os
E=Path(__file__).resolve().parent;R=E.parents[2];base=json.loads((E/'results.json').read_text());report={}
for feature in ['color-exchange','colorify','video']:
 path='app/painter/tests/test-filter-'+feature+'-shadow.cpp';cmd=[v.replace('test-filter-normalize-shadow.cpp','test-filter-'+feature+'-shadow.cpp')for v in base['runs']['normal']['compile']];exe=Path('/tmp/normalize-'+feature+'-regression');cmd[cmd.index('-o')+1]=str(exe);build=subprocess.run(cmd,capture_output=True,text=True);assert build.returncode==0,build.stderr;q=subprocess.run([str(exe),str(R/'migration/fixtures/legacy-filter-points/fixtures.tsv')],capture_output=True,text=True,timeout=240);report[feature]={'command':cmd,'exit_code':q.returncode,'stdout':q.stdout,'stderr':q.stderr,'fixture':path,'fixture_sha256':hashlib.sha256((R/path).read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()};(E/'regressions.json').write_text(json.dumps(report,indent=2)+'\n');assert q.returncode==0,q;print('PASS',feature,flush=True)
for p,h in base['source_sha256'].items():assert hashlib.sha256((R/p).read_bytes()).hexdigest()==h,p
