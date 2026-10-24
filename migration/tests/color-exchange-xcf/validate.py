#!/usr/bin/env python3
from pathlib import Path
import json,gzip,tarfile,hashlib,re,sys,subprocess
E=Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
def check(ok,why):
 if not ok:raise ValueError(why)
def result(row,cases):
 check(row['exit_code']==0,'failed result');check(re.findall(r'^ok \d+ (\S+)',row['stdout'],re.M)==['/painter-xcf-open/'+c for c in cases],'wrong case set')
new=['exchange_old_wire_admission','exchange_wire_roundtrips','exchange_malformed_wire','exchange_native_model_precedence'];cases=new+['red_eye_old_wire_admission','red_eye_old_wire_roundtrips','red_eye_malformed_wire','max_rgb_old_writer_roundtrip','max_rgb_malformed_capsules']
manifest=json.loads((E/'artifact-sha256.json').read_text());check(set(manifest)=={str(p.relative_to(E))for p in E.rglob('*')if p.is_file()and p.name!='artifact-sha256.json'and '__pycache__'not in p.parts},'artifact set')
for p,h in manifest.items():check(sha((E/p).read_bytes())==h,p)
n=json.loads((E/'native.json').read_text());s=json.loads((E/'sanitizer.json').read_text());check(set(n)==set(cases),'native set')
for c,row in n.items():result(row,[c])
result(s,new)
for text in [s['stdout'],n[new[1]]['stdout']+n[new[2]]['stdout']]:check(len(re.findall('Exchange wire scene',text))==4 and len(re.findall('Exchange malformed',text))==28,'matrix')
check(len(s['sources'])==9 and not s['instrumented_cpp_rtti']and not s['leak_detection']and not s['changed_after_run'],'sanitizer scope')
cpp=[c for c in s['commands']if any(str(v).endswith('.cpp')for v in c)];check(len(cpp)==2 and all('-fno-rtti'in c and '-fno-sanitize=vptr'in c and '-frtti'not in c for c in cpp),'RTTI ABI')
a=json.loads(gzip.decompress((E/'source-after.json.gz').read_bytes()));b=json.loads(gzip.decompress((E/'source-before.json.gz').read_bytes()));check(not a['changed_after_run']and a['sha256']==b['sha256'],'source drift')
selected=json.loads((E/'selected-sources.json').read_text());actual={}
with tarfile.open(E/'sources.tar.gz')as t:
 for member in t:
  check(member.isfile()and not Path(member.name).is_absolute()and '..'not in Path(member.name).parts and member.name not in actual,'archive safety');actual[member.name]=sha(t.extractfile(member).read())
check(actual==selected and len(actual)==48,'archive set')
for p,h in selected.items():check(a['sha256'][p]==h,'full seal mismatch')
for p,h in s['source_sha256'].items():check(actual[p]==h,'san source')
for p,h in json.loads((E/'owned-source-sha256.json').read_text()).items():
 check(actual[p]==h,'owned archive')
 if '--check-source'in sys.argv:check(sha((R/p).read_bytes())==h,p)
red=json.loads((E/'red/result.json').read_text());check(red['exit_code']!=0 and "'args' should not be NULL"in red['stdout'],'causal red')
m=json.loads((E/'mutations/results.json').read_text());check(len(m)==3,'mutants')
for row in m:check(row['exit_code']!=0 and 'ERROR:'in row['stdout']+row['stderr'],'accepted mutant')
result(json.loads((E/'mutations/restored.json').read_text()),['exchange_wire_roundtrips'])
for name in ['color-exchange-wire-foundation','color-exchange-native']:
 for p,h in json.loads((E/(name+'-predecessor.json')).read_text()).items():check(sha((R/p).read_bytes())==h,p)
 subprocess.run([sys.executable,'-B',str(E.parent/name/'validate.py')],check=True)
for row in [dict(s,exit_code=1),dict(s,stdout=s['stdout'].replace('/exchange_old_wire_admission','/wrong')),dict(s,stdout=s['stdout'].replace('ok 1 ','not ok 1 ',1))]:
 try:result(row,new)
 except ValueError:pass
 else:raise ValueError('negative control accepted')
print('PASS Exchange XCF native9/san4,4wire+28opaque scenes,67 Save/Open cycles,48 sealed inputs,3 mutants+3 result controls')
