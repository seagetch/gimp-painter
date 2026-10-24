#!/usr/bin/env python3
import pathlib,tempfile,shutil,json,hashlib,importlib.util,contextlib,io
R=pathlib.Path(__file__).resolve().parents[3];passed=[]
for package,checker in [('smudge-initial-sampling','check-initial-sampling.py'),('smudge-mask-format','check-mask-format.py')]:
 P=R/'migration/tests'/package;spec=importlib.util.spec_from_file_location('prior',P/checker);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 for name,fn in [('historical pin',lambda j:j['files'][next(iter(j['files']))].update(historical='0'*64)),('current pin',lambda j:j['files'][next(iter(j['files']))].update(current='0'*64)),('checker pin',lambda j:j.update(checker_sha256='0'*64)),('future path',lambda j:j['files'].update({'unrelated.cpp':{'current':'0'*64,'historical':'0'*64}}))]:
  with tempfile.TemporaryDirectory()as td:
   q=pathlib.Path(td)/'prior';shutil.copytree(P,q);f=q/'successor-source-admission.json';j=json.loads(f.read_text());fn(j);f.write_text(json.dumps(j));manifest=json.loads((q/'manifest.json').read_text());manifest['successor-source-admission.json']=hashlib.sha256(f.read_bytes()).hexdigest();(q/'manifest.json').write_text(json.dumps(manifest))
   try:
    with contextlib.redirect_stdout(io.StringIO()):m.check(q)
   except AssertionError:passed.append(package+': '+name)
   else:raise AssertionError('accepted '+name)
print(json.dumps({'status':'PASS','rejected_successor_mutations':passed},indent=2))
