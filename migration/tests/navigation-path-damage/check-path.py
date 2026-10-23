#!/usr/bin/env python3
import hashlib,json,pathlib,re,struct,subprocess
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def require(v,m):
 if not v:raise ValueError(m)
def check(root=R,e=E):
 m=json.loads((e/'manifest.json').read_text())
 require(m['task']=='25.009/path-damage' and m['parent_closed'] is False and m['original_work_rows_closed']==0,'scope')
 require(m['old_runtime']=='USER-SKIPPED' and m['oncanvas'],'runtime/oncanvas')
 require(m['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True),'portrait')
 require(m['build_policy']==dict(optimization='2',debug=False,b_ndebug=False,asan=True,ubsan=True,lsan=False,dependency_instrumentation=False),'build policy')
 for p,h in m['source_sha256'].items():require(sha((root/p).read_bytes())==h,'source '+p)
 for p,h in m['dependency_source_sha256'].items():require(sha((root/p).read_bytes())==h,'dependency '+p)
 for p,h in m['artifact_sha256'].items():require(sha((e/p).read_bytes())==h,'artifact '+p)
 old=(e/'red-source/app/display/gimpcanvaspath.c').read_bytes();require(old==subprocess.check_output(['git','show',m['baseline']+':app/display/gimpcanvaspath.c'],cwd=R),'immutable old path')
 current=(root/'app/display/gimpcanvaspath.c').read_text();new,previous=m['extent_only_replacement']
 require(current.count(new)==1 and current.replace(new,previous)==old.decode(),'all non-extent bytes unchanged')
 for kind in ['normal','sanitizer']:
  for group in ['bounds']+['window-'+str(i) for i in range(9)]:
   r=json.loads((e/(kind+'-'+group+'.json')).read_text())
   require(r['result']=='OK' and r['returncode']==0 and r['duration']<60 and r['test_budget_seconds']==60,'result/budget')
   require(r['source_sha256']==m['source_sha256'] and r['xvfb']==m['xvfb'],'tested source/display')
   lines=[x for x in r['stdout'].splitlines() if x.startswith('ok ') and '# SKIP' not in x]
   require(r['cases']==1 and len(lines)==1 and lines[0].endswith('/path-damage/'+group),'TAP')
   if group=='bounds':require('# path recording bounds scenes=216' in r['stdout'],'216 bounds')
   else:
    cases=[x for x in r['stdout'].splitlines() if x.startswith('# path window ')]
    require(len(cases)==24 and all(re.search(r'visible=[1-9][0-9]* residual=0$',x) for x in cases),'real transitions')
    require(all('angle='+str([0,37,90][int(group[-1])//3])+' ' in x for x in cases),'window angles')
    tuples={tuple(map(int,re.search(r'H=(\d) V=(\d) style=(\d) shape=(\d) filled=(\d)',x).groups())) for x in cases}
    require(tuples=={(h,v,int(group[-1])%3,s,f) for h in range(2) for v in range(2) for s in range(3) for f in range(2)},'window combination coverage')
 red=json.loads((e/'expected-red.json').read_text());require(red['result']=='FAIL' and red['returncode']==-6 and 'style=1 shape=1 filled=0 visible=1556 residual=9' in red['stdout'],'actual old path red')
 for p,h in red['source_sha256'].items():require(sha((e/'red-source'/p).read_bytes())==h,'red source')
 layout=json.loads((e/'sanitizer-window-layout-red.json').read_text());require(layout['result']=='FAIL' and '(968 == 963)' in layout['stdout'],'layout diagnostic')
 require(sha((e/'layout-red-test.inc.txt').read_bytes())==layout['source_sha256']['app/tests/test-painter-navigation-path-damage.inc'],'layout source')
 for name in ['item-geometry-path-shown.png','item-geometry-path-hidden.png']:
  b=(e/name).read_bytes();require(b[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',b[16:24])==(1080,1920),'screenshot')
 return dict(status='PASS',normal_cases=10,sanitizer_cases=10,bounds_scenes=216,window_scenes=216,parent_closed=False)
if __name__=='__main__':print(json.dumps(check(),sort_keys=True))
