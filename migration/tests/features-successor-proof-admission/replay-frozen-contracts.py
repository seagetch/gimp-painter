import importlib.util,subprocess,hashlib,json
from unittest import mock
from pathlib import Path
r=Path(__file__).resolve().parents[3]
s=importlib.util.spec_from_file_location('suite',r/'tools/tests/test_painter_contracts.py');t=importlib.util.module_from_spec(s);s.loader.exec_module(t)
f=t.HandleProofTests();f.setUp()
try:
 c=t.c
 checked=set()
 for proof in c.BOUNDED_HANDLE_PROOFS.values():
  for row in json.loads((f.root/proof['directory']/'source-manifest.json').read_text()):
   path=row['path']; committed=subprocess.check_output(['git','show','20b2204b11:'+path],cwd=r)
   assert hashlib.sha256(committed).hexdigest()==row['sha256'],path
   assert (f.root/path).read_bytes()==committed,path
   checked.add(path)
 original=c.verify_handle_implementations
 with mock.patch.object(c,'verify_handle_implementations',side_effect=lambda rows:original(rows,f.root)):
  errors,handles,entries=c.verify()
 assert not errors,errors
 print(json.dumps({'status':'PASS','source_base':'20b2204b11','distinct_source_inputs':len(checked),'handles':handles,'entries':entries,'scope':'sparse exact committed proof-input fixture, current unchanged inventory; no build/native execution/full source aggregate'},indent=2))
finally:f.doCleanups()
