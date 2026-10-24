#!/usr/bin/env python3
import pathlib,json,hashlib,subprocess,re
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
ref=json.loads((E/'reference.json').read_text());assert ref['commit']=='afa43fae3e920210146abed514f136fd49f671b5'
sources={}
for row in ref['sources']:
 data=subprocess.check_output(['git','show',ref['commit']+':'+row['path']],cwd=R);assert sha(data)==row['sha256'];assert subprocess.check_output(['git','rev-parse',ref['commit']+':'+row['path']],cwd=R,text=True).strip()==row['blob'];sources[row['path']]=data.decode()
def function(s,name):
 m=list(re.finditer(r'\n'+name+r'\s*\(',s))[-1];start=m.start()+1;brace=s.index('{',start);depth=1;i=brace+1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
assert (E/'old-indexed.inc').read_text()=='static void\n'+function(sources['plug-ins/common/contrast-normalize.c'],'indexed_normalize')+'\n'
r=json.loads((E/'results.json').read_text());assert not r['inputs_changed'];assert set(r['runs'])=={'normal','sanitizer'}
for n,h in r['inputs'].items():assert sha((E/n).read_bytes())==h
for name,row in r['runs'].items():assert row['returncode']==0 and 'PASS 42896 nonempty palette scenes'in row['stdout']
assert '-fsanitize=address,undefined,float-cast-overflow'in r['runs']['sanitizer']['command'];assert len(r['negative_controls'])==3
for row in r['negative_controls']:assert row['returncode']!=0 and 'Assertion'in row['stderr']
print('PASS pinned old indexed Normalize:42896 nonempty palettes normal+sanitizer,3 rejected mutants; native image/history unverified')
