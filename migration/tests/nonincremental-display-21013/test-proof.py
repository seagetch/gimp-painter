#!/usr/bin/env python3
import pathlib,tempfile,shutil,json,hashlib,subprocess
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2]
cases=[('false-original-closure','scope.json',lambda x:x.update(original_closed=True)),('wrong-opacity','scope.json',lambda x:x.update(stroke_opacity=1)),('native-failed','normal-results.json',lambda x:x[0].update(returncode=1)),('missing-golden','normal-results.json',lambda x:x.pop()),('asan-failed','asan-results.json',lambda x:x[0].update(returncode=1)),('missing-instrumentation','asan-build.json',lambda x:x['instrumented_sources'].pop()),('mutant-survives','mutations.json',lambda x:x[0].update(returncode=0)),('held-baseline-missing','excluded-provider-seal.json',lambda x:x.pop())]
for name,file,change in cases:
 with tempfile.TemporaryDirectory()as td:
  p=pathlib.Path(td)/'proof';shutil.copytree(E,p);f=p/file;x=json.loads(f.read_text());change(x);f.write_text(json.dumps(x));h=json.loads((p/'artifact-sha256.json').read_text());h[file]=hashlib.sha256(f.read_bytes()).hexdigest();(p/'artifact-sha256.json').write_text(json.dumps(h));q=subprocess.run(['python3','-B',str(p/'validate.py'),'--root',str(R)],capture_output=True,text=True);assert q.returncode!=0,name;print('REJECT',name)
print('PASS8 semantic evidence controls, with modified artifact hashes refreshed')
