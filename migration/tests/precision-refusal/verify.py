#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re,tarfile,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
r=json.loads((E/'report.json').read_text());assert r['task']=='09.015' and r['native_tap_count']==16 and r['actual_production_mutants_rejected']==3
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for n,h in r['source_sha256'].items():
  assert sha(t.extractfile(n).read())==h,n
  if a.check_source:assert sha((R/n).read_bytes())==h,n
rows=[json.loads(s)for s in (E/'native.json').read_text().splitlines()];assert len(rows)==3 and all(x['result']=='OK'for x in rows);assert sum(len(re.findall(r'^ok \d+ ',x['stdout'],re.M))for x in rows)==16
native=next(x for x in rows if x['name'].endswith(' / painter-mypaint-precision'));assert native['stdout'].count('unsupported-kind=')==12 and 'native-144-matrix' in native['stdout']
s=json.loads((E/'sanitizer.json').read_text());assert s['exit_code']==0 and len(re.findall(r'^ok \d+ ',s['stdout'],re.M))==1 and s['stdout'].count('unsupported-kind=')==12
assert len(re.findall(r'^ok \d+ ',(E/'asan.log').read_text(),re.M))==2
assert len(re.findall(r'^ok \d+ ',(E/'precision-asan.log').read_text(),re.M))==8
b=json.loads((E/'sanitizer-build.json').read_text());assert len(b['instrumented_sources'])==12 and not b['changed_during_build'] and not b['vptr_sanitizer'] and not b['leak_detection']
assert 'app/core/gimpimage-color-profile.c'in b['instrumented_sources'] and 'libgimpcolor/gimpcolorprofile.c'not in b['instrumented_sources']
m=json.loads((E/'mutations.json').read_text());assert len(m)==2 and all(x['exit_code']!=0 and x['rejected']for x in m)
for x in m:assert 'not ok' in (E/(x['name']+'.log')).read_text()
m=json.loads((E/'native-mutant.json').read_text());assert m['exit_code']!=0 and m['rejected'] and 'not ok' in (E/'native-mutant.log').read_text()
rows=[json.loads(s)for s in (E/'restored.json').read_text().splitlines()];assert len(rows)==1 and rows[0]['result']=='OK' and len(re.findall(r'^ok \d+ ',rows[0]['stdout'],re.M))==1
assert not r['production_changed'] and not r['rendered_UI_evidence'] and not r['whole_application']
prior=R/'migration/tests/color-placement';assert sha((prior/'report.json').read_bytes())==r['predecessor_report_sha256'];subprocess.run([sys.executable,'-I',str(prior/'verify.py')],check=True)
print('09.015 PASS: 16 native TAP, standalone ASan10/native selected ASan1,3 actual mutants;2 inherited whole-scalar duties stay open')
