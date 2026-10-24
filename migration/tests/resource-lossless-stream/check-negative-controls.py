#!/usr/bin/env python3
"""Private-copy receipt/source controls; never modify the committed evidence."""
import hashlib,json,pathlib,shutil,subprocess,sys,tarfile,tempfile
E=pathlib.Path(__file__).resolve().parent
records=[]
with tempfile.TemporaryDirectory(prefix='resource-proof-controls-')as folder:
 root=pathlib.Path(folder);e=root/'migration/tests/resource-lossless-stream';shutil.copytree(E,e)
 with tarfile.open(e/'tested-sources.tar.gz')as t:
  for row in json.loads((e/'results.json').read_text())['source_sha256']:
   p=root/row;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(t.extractfile(row).read())
 def run(name):
  z=subprocess.run([sys.executable,'-I',str(e/'validate.py'),'--check-source'],capture_output=True,text=True)
  records.append(dict(name=name,returncode=z.returncode,stdout=z.stdout,stderr=z.stderr));return z.returncode
 assert run('positive-baseline')==0
 data=(e/'results.json').read_bytes();pins=(e/'artifact-sha256.json').read_bytes()
 for name,target in [('normal-failure','normal'),('mutant-false-success','drop-unknown-object')]:
  x=json.loads(data);next(row for row in x['results']if row['name']==target)['returncode']=1 if target=='normal' else 0
  (e/'results.json').write_text(json.dumps(x))
  # Bypass only local checksum to exercise semantic verification independently.
  h=json.loads(pins);h['results.json']=hashlib.sha256((e/'results.json').read_bytes()).hexdigest();(e/'artifact-sha256.json').write_text(json.dumps(h))
  assert run(name)!=0
 (e/'results.json').write_bytes(data);(e/'artifact-sha256.json').write_bytes(pins)
 p=root/'app/paint/painter-mypaint/resource.cpp';old=p.read_bytes();p.write_bytes(old+b'\n// untested drift\n');assert run('current-source-drift')!=0;p.write_bytes(old)
 p=e/'tested-sources.tar.gz';old=p.read_bytes();p.unlink();assert run('missing-source-archive')!=0;p.write_bytes(old)
 assert run('restored-positive')==0
print(json.dumps(records,indent=2))
