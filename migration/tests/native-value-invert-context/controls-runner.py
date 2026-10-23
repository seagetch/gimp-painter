#!/usr/bin/env python3
import hashlib,json,re,subprocess,sys
from pathlib import Path
root=Path('/workspace/shared/gimp-painter-value-invert-owner')
scripts=[('tools/tests/test_value_invert_capture.py',36),('migration/tests/test_sharpen_context_sanitizers.py',20),('migration/tests/test_value_invert_focused_normal.py',8)]
results=[]
for path,count in scripts:
 command=[sys.executable,'-B',str(root/path)]
 p=subprocess.run(command,cwd=root,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=180)
 text=p.stdout+p.stderr
 sys.stdout.write(text);sys.stdout.flush()
 counts=re.findall(r'^Ran (\d+) tests? in ',text,re.M)
 ok=p.returncode==0 and counts==[str(count)] and re.search(r'^OK$',text,re.M) and not re.search(r'\b(skipped|FAILED)\b',text)
 results.append({'script':path,'sha256':hashlib.sha256((root/path).read_bytes()).hexdigest(),'command':command,'expected_count':count,'observed_counts':counts,'exit_code':p.returncode,'passed':bool(ok),'stdout':p.stdout,'stderr':p.stderr})
r={'status':'passed' if all(x['passed'] for x in results) else 'failed','scope':'Pure Python controls, including synthetic rejection examples and mocked native subprocesses; not native or genuine runtime acceptance','tests_run':sum(int(x['observed_counts'][0]) for x in results if len(x['observed_counts'])==1),'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'scripts':results,'previous_incomplete_invocation':'Imported sanitizer test module executed its own 20-test top-level runner and exited before other modules; retained controls-incomplete.log does not establish 64 controls.'}
Path('/workspace/shared/gimp-value-invert-owner-controls-2.json').write_text(json.dumps(r,indent=2)+'\n')
raise SystemExit(0 if r['status']=='passed' and r['tests_run']==64 else 1)
