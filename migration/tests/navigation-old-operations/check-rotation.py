#!/usr/bin/env python3
"""Validate bounded rotation evidence, not full task completion."""
import hashlib,json,pathlib,struct,subprocess
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def require(v,m):
 if not v:raise ValueError(m)
def check(root=R,e=E):
 m=json.loads((e/'rotation-manifest.json').read_text())
 require(m['previous_checkpoint']=='c49648e13db656eb27c362c9277639dd162e04b8','previous checkpoint')
 require(m['native_cases']==37 and m['new_cases']==2,'case count')
 require((m['old_rotation_scenes'],m['reentries'],m['motions_per_entry'],m['point_pairs'])==(24,2,7,1008),'old oracle coverage')
 require(m['oncanvas'] is True and m['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True),'portrait oncanvas')
 require(m['build_policy']==dict(optimization='2',debug=False,b_ndebug=False,asan=True,ubsan=True,lsan=False,dependency_instrumentation=False),'build policy')
 for p,h in m['source_sha256'].items():require(sha((root/p).read_bytes())==h,'source '+p)
 for p,h in m['artifact_sha256'].items():require(sha((e/p).read_bytes())==h,'artifact '+p)
 for n in ['rotation-native.json','rotation-sanitizer.json']:
  row=json.loads((e/n).read_text());require('env' not in row and set(row)<=set(['name','result','duration','returncode','stdout','stderr']),'receipt field allowlist')
  require(row['result']=='OK' and row['returncode']==0,'result '+n)
  cases=[x for x in row['stdout'].splitlines() if x.startswith('ok ')]
  require(len(cases)==37,'TAP '+n)
  for name in ['rotation-origin','rotation-render']:require(any('/old-operations/'+name in x for x in cases),'new case '+name)
  require(row['stdout'].count('# old applied rotation ')==1008,'old landmark count '+n)
  require(row['stdout'].count('# old pan ')==5760,'retained pan '+n)
 for build in ['build','build-asan']:
  for group,count in [('original',35),('added',2)]:
   n='rotation-budget-'+build+'-'+group+'.json';row=json.loads((e/n).read_text())
   require(row['result']=='OK' and row['returncode']==0 and row['duration']<60,'original sixty-second budget '+n)
   cases=[x for x in row['stdout'].splitlines() if x.startswith('ok ') and '# SKIP' not in x]
   require(len(cases)==count,'isolated group count '+n)
   require(all(('/old-operations/' in x)==(group=='added') for x in cases),'isolated group membership '+n)
 f=root/'migration/fixtures/legacy-navigation-applied';fm=json.loads((f/'manifest.json').read_text());oracle=(f/'rotation-oracle.inc').read_bytes()
 require(fm['source_commit']=='afa43fae3e920210146abed514f136fd49f671b5' and len(fm['pieces'])==3,'old identity')
 original=subprocess.check_output(['git','show',fm['source_commit']+':'+fm['source_path']],cwd=R)
 for n,h in fm['pieces'].items():
  b=(f/(n+'.txt')).read_bytes();require(sha(b)==h and b in original and b in oracle,'unchanged old '+n)
 rm=json.loads((e/'rotation-red-source.json').read_text())
 require(rm['production_checkpoint']==m['previous_checkpoint'],'red checkpoint')
 for p,h in rm['sha256'].items():
  b=(e/'rotation-red-source'/(pathlib.Path(p).name+'.txt')).read_bytes();require(sha(b)==h,'red source '+p)
  if p=='app/display/gimpdisplayshell-rotate.c':require(b==subprocess.check_output(['git','show',m['previous_checkpoint']+':'+p],cwd=R),'pre-fix production source')
 red=json.loads((e/'rotation-expected-red.json').read_text())
 require(red['result']=='FAIL' and red['returncode']==-6,'expected red result')
 require('expected origin=400.32770966740947,194.77913087625265 actual=400,195' in red['stdout'],'red geometry')
 for n in ['portrait-oncanvas-rotation-m0.png','portrait-oncanvas-rotation-m1.png']:
  b=(e/n).read_bytes();require(b[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',b[16:24])==(1080,1920),'screenshot size')
 inventory='migration/inventory/additional-c-header-routes.json'
 built=json.loads((e/'rotation-build-header-routes.json').read_text());current=json.loads((root/inventory).read_text())
 route=lambda obj:next(x for x in obj['routes'] if x['path']=='app/display/gimpdisplayshell-scroll.h')
 require(route(built)==route(current),'navigation header route')
 base=json.loads(subprocess.check_output(['git','show',m['base_commit']+':'+inventory],cwd=R));base['routes'].append(route(current));require(base==built,'isolated build closure')
 return dict(status='PASS',cases=37,old_rotation_point_pairs=1008,task_closed=False)
if __name__=='__main__':print(json.dumps(check(),sort_keys=True))
