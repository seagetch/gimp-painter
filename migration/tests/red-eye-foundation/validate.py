#!/usr/bin/env python3
import hashlib,json,subprocess
from pathlib import Path
p=Path(__file__).resolve().parent;r=p.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
a=json.loads((p/'artifact-sha256.json').read_text())
assert set(a)=={x.name for x in p.iterdir() if x.is_file() and x.name!='artifact-sha256.json'}
for x,h in a.items():assert sha((p/x).read_bytes())==h,x
ref=json.loads((p/'reference.json').read_text());old=subprocess.check_output(['git','show',ref['commit']+':'+ref['path']],cwd=r)
assert sha(old)==ref['sha256'];extracted=old.decode()[old.decode().index('static void\nredeye_inner_loop (const guchar *src,'):]
assert sha(extracted.encode())==ref['extracted_function_sha256'];assert (p/'old-inner-loop.inc').read_text().endswith(extracted)
for path,h in ref['old_api_sha256'].items():assert sha(subprocess.check_output(['git','show',ref['commit']+':'+path],cwd=r))==h
j=json.loads((p/'results.json').read_text());assert j['input_changed']==[]
for x,h in j['inputs'].items():assert sha((p/x).read_bytes())==h
assert set(j['runs'])=={'normal','sanitizer'}
for x in j['runs'].values():assert x['exit_code']==0 and 'PASS 234881024 scalar RGB/RGBA comparisons' in x['stdout'] and not x['stderr']
assert any('float-cast-overflow' in x for x in j['runs']['sanitizer']['compile_command'])
controls=json.loads((p/'negative-controls.json').read_text());assert len(controls)==2
for x in controls.values():assert x['exit_code']!=0 and 'mismatch' in x['stderr']
requirements=json.loads((p/'requirement-sources.json').read_text());assert requirements['counts']=={'all':108,'required_labels':93,'bounded_labels':15};assert sum(requirements['required_by_label'].values())==93
print('PASS old source/API extraction;normal+san234881024 each;2 causal mutants;108/93 inventory labels;no native route claim')
