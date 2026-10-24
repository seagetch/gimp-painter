#!/usr/bin/env python3
import hashlib,json,re,tarfile,struct
from pathlib import Path
P=Path(__file__).resolve().parent
CASES=['red_eye_old_wire_admission','red_eye_old_wire_roundtrips','red_eye_malformed_wire']
def sha(b):return hashlib.sha256(b).hexdigest()
def check(ok,why):
 if not ok:raise ValueError(why)
def result(j,cases):
 check(j['exit_code']==0,'failed result')
 check(re.findall(r'^ok \d+ (\S+)',j['stdout'],re.M)==['/painter-xcf-open/'+c for c in cases],'wrong case/count')
def main():
 manifest=json.loads((P/'artifact-sha256.json').read_text())
 check(set(manifest)=={str(p.relative_to(P)) for p in P.rglob('*') if p.is_file() and p.name!='artifact-sha256.json' and '__pycache__' not in p.parts},'artifact set')
 for p,h in manifest.items():check(sha((P/p).read_bytes())==h,p)
 n=json.loads((P/'native.json').read_text());s=json.loads((P/'sanitizer.json').read_text());b=json.loads((P/'source-before.json').read_text());a=json.loads((P/'source-after.json').read_text())
 check(set(n)==set(CASES),'native set')
 for c,j in n.items():result(j,[c])
 result(s,CASES)
 for text in (s['stdout'],n[CASES[1]]['stdout']+n[CASES[2]]['stdout']):
  check(len(re.findall('Red Eye wire scene',text))==8 and len(re.findall('Red Eye malformed',text))==16,'matrix count')
 check(not s['changed_after_run'] and not a['changed_after_run'] and b['sha256']==a['sha256'],'source drift')
 check(len(s['sources'])==9 and s['instrumented_cpp_rtti'] is False and s['leak_detection'] is False,'scope')
 cpp=[c for c in s['commands'] if any(str(v).endswith('.cpp') for v in c)]
 check(len(cpp)==2 and all('-fno-rtti' in c and '-fno-sanitize=vptr' in c and '-frtti' not in c for c in cpp),'noRTTI/vptr')
 expected=dict(s['source_sha256']);expected.update(b['sha256']);actual={}
 with tarfile.open(P/'sources.tar.gz') as t:
  for f in t:
   check(f.isfile() and not Path(f.name).is_absolute() and '..' not in Path(f.name).parts and f.name not in actual,'archive member');actual[f.name]=sha(t.extractfile(f).read())
 check(actual==expected,'source archive')
 for p,h in json.loads((P/'owned-source-sha256.json').read_text()).items():check(actual[p]==h,'owned source')
 red=json.loads((P/'red/result.json').read_text());check(red['exit_code']!=0 and "'args' should not be NULL" in red['stdout'],'causal red')
 w=json.loads((P/'writer-result.json').read_text());meta=json.loads((P/'writer-source.json').read_text());c=(P/'writer-oracle.c').read_text()
 check(w['exit_code']==0 and len(w['cases'])==8 and sha(c.encode())==w['source_sha256'],'writer result')
 for name,body in meta['exact_functions'].items():check(body in c,'exact old '+name)
 for row in w['cases']:
  name=b'plug-in-red-eye-removal\0';e=struct.pack('>I',len(name))+name+struct.pack('>III',2,4,row['run_mode']&0xffffffff)+struct.pack('>IIII',1,0,10,0)+struct.pack('>III',2,4,row['threshold']&0xffffffff)+b'\0'*8
  check(len(e)==76 and e.hex()==row['hex'],'wire frame')
 for mutant in [dict(s,exit_code=1),dict(s,stdout=s['stdout'].replace('/red_eye_old_wire_admission','/wrong'))]:
  try:result(mutant,CASES)
  except ValueError:pass
  else:raise ValueError('negative control accepted')
 print(f'PASS native3/san3;8 typed+16 opaque scenes/48 SaveOpen rounds;old writer8 source frames;{len(actual)} sealed inputs;9units/noRTTI/no vptr;2 negative controls')
if __name__=='__main__':main()
