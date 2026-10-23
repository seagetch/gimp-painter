#!/usr/bin/env python3
import importlib.util,json,pathlib,shutil,tempfile
P=pathlib.Path(__file__).resolve().parent;s=importlib.util.spec_from_file_location('v',P/'validate.py');v=importlib.util.module_from_spec(s);s.loader.exec_module(v)
mutations={'false-parent-complete':('report.json',lambda d:d.update(status='DONE')),'normal-failed':('normal.json',lambda d:d[0].update(result='FAIL')),'sanitizer-failed':('sanitizers.json',lambda d:d['results']['painter-xcf-roundtrip'].update(exit_code=1)),'changed-source':('sanitizers.json',lambda d:d.update(changed_during_run=['app/xcf/xcf-load.c'])),'missing-scenes':('sanitizers.json',lambda d:d['results']['painter-xcf-roundtrip'].update(stdout=d['results']['painter-xcf-roundtrip']['stdout'].replace('declared4294967295','missing-size'))),'false-production-fix':('report.json',lambda d:d.update(production_changed=True))}
results=[]
for name,(file,mutate)in mutations.items():
 with tempfile.TemporaryDirectory(prefix='xcf-length-control-')as directory:
  p=pathlib.Path(directory)/'evidence';shutil.copytree(P,p);f=p/file;d=json.loads(f.read_text());mutate(d);f.write_text(json.dumps(d))
  try:v.check(p)
  except AssertionError:results.append({'case':name,'rejected':True})
  else:raise AssertionError(name)
(P/'validator-controls.json').write_text(json.dumps(results,indent=2)+'\n');print('PASS6 receipt mutations rejected')
