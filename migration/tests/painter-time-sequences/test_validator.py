#!/usr/bin/env python3
import hashlib,json,pathlib,shutil,tempfile
from validate import validate,P
cases=[('final-normal.json',lambda r:r.update(returncode=1)),('final-normal.json',lambda r:r.update(stdout=r['stdout'].replace('ok 7 /timestamp-sequence/half-range-accepted','omitted case'))),('final-sanitizer.json',lambda r:r.update(stderr=r['stderr']+'\nruntime error: injected receipt failure')),('reverse-mutant.json',lambda r:r.update(returncode=0))]
for name,change in cases:
 with tempfile.TemporaryDirectory(prefix='timestamp-validator-') as d:
  p=pathlib.Path(d)/'proof';shutil.copytree(P,p);q=p/name;r=json.loads(q.read_text());change(r);q.write_text(json.dumps(r));seal=json.loads((p/'artifacts.json').read_text());seal[name]=hashlib.sha256(q.read_bytes()).hexdigest();(p/'artifacts.json').write_text(json.dumps(seal))
  try:validate(P.parents[2],p)
  except AssertionError:pass
  else:raise AssertionError('semantic mutant accepted: '+name)
print('PASS: four resealed semantic corruption controls rejected')
