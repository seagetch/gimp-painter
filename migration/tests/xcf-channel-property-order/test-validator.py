#!/usr/bin/env python3
import importlib.util,json,pathlib,shutil,tempfile
P=pathlib.Path(__file__).resolve().parent;s=importlib.util.spec_from_file_location('v',P/'validate.py');v=importlib.util.module_from_spec(s);s.loader.exec_module(v)
mutations={'false-parent-complete':('report.json',lambda d:d.update(status='DONE')),'normal-failed':('normal.json',lambda d:d[0].update(result='FAIL')),'sanitizer-failed':('sanitizers.json',lambda d:d['results']['painter-xcf-roundtrip'].update(exit_code=1)),'changed-source':('sanitizers.json',lambda d:d.update(changed_during_run=['app/xcf/xcf-load.c'])),'no-causal-red':('expected-red.json',lambda d:d[0].update(returncode=0)),'no-source-controls':('source-contract.json',lambda d:d.update(negative_controls=[]))}
results=[]
for name,(file,mutate) in mutations.items():
 with tempfile.TemporaryDirectory(prefix='xcf-order-control-') as directory:
  p=pathlib.Path(directory)/'evidence';shutil.copytree(P,p);f=p/file;d=json.loads(f.read_text());mutate(d);f.write_text(json.dumps(d))
  try:v.check(p)
  except AssertionError:results.append({'case':name,'rejected':True})
  else:raise AssertionError(name)
(P/'validator-controls.json').write_text(json.dumps(results,indent=2)+'\n');print('PASS6 receipt mutations rejected')
