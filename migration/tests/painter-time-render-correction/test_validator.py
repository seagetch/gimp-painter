#!/usr/bin/env python3
import hashlib,json,pathlib,shutil,tempfile
from validate import P,validate,BAD
for case in range(4):
 with tempfile.TemporaryDirectory(prefix='render-erratum-validator-') as d:
  p=pathlib.Path(d)/'proof';shutil.copytree(P,p)
  if case==0:
   name='normal-render.png';shutil.copyfile(P.parents[2]/BAD,p/name)
  elif case in (1,2):
   name='normal-receipt.json';r=json.loads((p/name).read_text())
   if case==1:r['runtime_unchanged']=False
   else:r['stdout']=r['stdout'].replace('rgb=218,55,55','rgb=0,0,0')
   (p/name).write_text(json.dumps(r))
  else:
   name='preserved-b804-files.json';r=json.loads((p/name).read_text());r.pop(BAD);(p/name).write_text(json.dumps(r))
  seal=json.loads((p/'artifacts.json').read_text());seal[name]=hashlib.sha256((p/name).read_bytes()).hexdigest();(p/'artifacts.json').write_text(json.dumps(seal))
  try:validate(P.parents[2],p)
  except (AssertionError,KeyError):pass
  else:raise AssertionError('accepted corrupted render successor: '+str(case))
print('PASS: four resealed screenshot/receipt/preservation corruption controls rejected')
