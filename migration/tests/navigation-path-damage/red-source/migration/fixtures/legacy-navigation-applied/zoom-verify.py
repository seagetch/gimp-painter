#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,subprocess,tempfile,shutil
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
m=json.loads((HERE/'zoom-source-manifest.json').read_text());assert m['revision']=='afa43fae3e920210146abed514f136fd49f671b5' and len(m['functions'])==25
parts=[]
for r in m['functions']:
 raw=subprocess.check_output(['git','show',m['revision']+':'+r['source_path']],cwd=ROOT)
 assert hashlib.sha256(raw).hexdigest()==r['source_sha256']
 blob=subprocess.check_output(['git','rev-parse',m['revision']+':'+r['source_path']],cwd=ROOT,text=True).strip();assert blob==r['source_blob']
 exact='\n'.join(raw.decode().splitlines()[r['first_line']-1:r['last_line']])+'\n';assert exact==(HERE/r['file']).read_text(),r['name'];assert hashlib.sha256(exact.encode()).hexdigest()==r['sha256'];parts.append(exact.rstrip('\n'))
assert (HERE/'zoom-functions.inc').read_text()=='/* Unchanged pinned function bodies; aliases/stubs live outside this file. */\n'+'\n\n'.join(parts)+'\n'
for r in m['auxiliary_source']:
 raw=subprocess.check_output(['git','show',m['revision']+':'+r['source_path']],cwd=ROOT);assert hashlib.sha256(raw).hexdigest()==r['source_sha256']
 exact='\n'.join(raw.decode().splitlines()[r['first_line']-1:r['last_line']])+'\n';assert exact==(HERE/r['file']).read_text();assert hashlib.sha256(exact.encode()).hexdigest()==r['sha256']
with tempfile.TemporaryDirectory(prefix='verify-zoom-adapter-') as tmp:
 d=Path(tmp)
 for name in ['zoom-build-adapter.py','zoom-source-manifest.json']:shutil.copyfile(HERE/name,d/name)
 subprocess.run(['python3',str(d/'zoom-build-adapter.py')],check=True)
 assert (d/'zoom-oracle.inc').read_bytes()==(HERE/'zoom-oracle.inc').read_bytes()
r=json.loads((HERE/'zoom-check-results.json').read_text());assert len(r['runs'])==2 and all(x['exit_code']==0 and x['stdout'].count('\nok ')==7 for x in r['runs']);assert {x['sanitized'] for x in r['runs']}=={False,True}
assert len(r['negative_controls'])==4 and all(x['exit_code']!=0 for x in r['negative_controls'])
for name,h in r['source_sha256'].items():assert hashlib.sha256((HERE/name).read_bytes()).hexdigest()==h,name
print('25 unchanged pinned functions; deterministic C adapter;7 normal+7 focused sanitizer groups;7680 scenarios/run;4 rejected source mutants PASS')
