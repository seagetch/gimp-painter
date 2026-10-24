#!/usr/bin/env python3
import hashlib,json,pathlib,shutil,subprocess,sys,tarfile,tempfile
E=pathlib.Path(__file__).resolve().parent;records=[]
with tempfile.TemporaryDirectory(prefix='resource-resolution-controls-')as folder:
 r=pathlib.Path(folder);e=r/'migration/tests/resource-late-resolution';shutil.copytree(E,e)
 with tarfile.open(e/'tested-provider-sources.tar.gz')as t:
  for name in json.loads((e/'results.json').read_text())['source_sha256']:
   p=r/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(t.extractfile(name).read())
 def run(name):
  x=subprocess.run([sys.executable,'-I',str(e/'validate.py'),'--check-source'],capture_output=True,text=True);records.append(dict(name=name,returncode=x.returncode,stdout=x.stdout,stderr=x.stderr));return x.returncode
 assert run('baseline')==0
 pins=(e/'artifact-sha256.json').read_bytes()
 for case in ['normal-failure','mutant-false-success','linked-drift','held-path-missing']:
  file='clean-provider-seal.json'if case=='held-path-missing'else'results.json';p=e/file;original=p.read_bytes();x=json.loads(original)
  if case=='normal-failure':next(z for z in x['results']if z['name']=='normal-run')['returncode']=1
  if case=='mutant-false-success':next(z for z in x['results']if z['name']=='mutant-paper-run')['returncode']=0
  if case=='linked-drift':x['linked_inputs_changed']=['unreviewed-change']
  if case=='held-path-missing':x['nine_held_paths_match_committed_bytes'].pop(next(iter(x['nine_held_paths_match_committed_bytes'])))
  p.write_text(json.dumps(x));h=json.loads(pins);h[file]=hashlib.sha256(p.read_bytes()).hexdigest();(e/'artifact-sha256.json').write_text(json.dumps(h));assert run(case)!=0;p.write_bytes(original);(e/'artifact-sha256.json').write_bytes(pins)
 p=r/'app/paint/painter-mypaint-surface/gimp-resources.cpp';original=p.read_bytes();p.write_bytes(original+b'\n// untested\n');assert run('provider-source-drift')!=0;p.write_bytes(original)
 assert run('restored')==0
print(json.dumps(records,indent=2))
