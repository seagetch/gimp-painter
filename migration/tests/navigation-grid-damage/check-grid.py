#!/usr/bin/env python3
"""Validate a bounded grid-only redraw checkpoint; not parent25.009 closure."""
import hashlib,json,pathlib,re,struct,subprocess
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def require(v,m):
 if not v:raise ValueError(m)
def check(root=R,e=E):
 m=json.loads((e/'manifest.json').read_text())
 require(m['task']=='25.009/grid-damage' and m['parent_closed'] is False and m['original_work_rows_closed']==0,'scope')
 require(m['old_runtime']=='USER-SKIPPED','old runtime')
 require(m['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True) and m['oncanvas'],'portrait')
 require(m['build_policy']==dict(optimization='2',debug=False,b_ndebug=False,asan=True,ubsan=True,lsan=False,dependency_instrumentation=False),'build policy')
 for p,h in m['source_sha256'].items():require(sha((root/p).read_bytes())==h,'source '+p)
 for p,h in m['artifact_sha256'].items():require(sha((e/p).read_bytes())==h,'artifact '+p)
 old=(e/'grid-red-gimpcanvasgrid.c.txt').read_bytes()
 require(old==subprocess.check_output(['git','show',m['baseline']+':app/display/gimpcanvasgrid.c'],cwd=R),'immutable red grid')
 current=(root/'app/display/gimpcanvasgrid.c').read_text()
 require(current.replace(m['added_extents_block'],'')==old.decode(),'only extents change; drawing unchanged')
 require('rectangle.x      -= 8;' in m['added_extents_block'] and 'rectangle.height += 16;' in m['added_extents_block'],'margin')
 for kind in ['normal','sanitizer']:
  for group in ['geometry','window-0','window-1','window-2']:
   r=json.loads((e/(kind+'-'+group+'.json')).read_text())
   require(r['result']=='OK' and r['returncode']==0 and r['duration']<60 and r['test_budget_seconds']==60,'result/budget')
   require(r['source_sha256']==m['source_sha256'] and r['xvfb']==m['xvfb'],'tested source/display')
   lines=[x for x in r['stdout'].splitlines() if x.startswith('ok ') and '# SKIP' not in x]
   require(r['cases']==1 and len(lines)==1 and lines[0].endswith('/grid-damage/'+group),'TAP')
   if group=='geometry':require(r['stdout'].count('# item geometry family=grid ')==120,'primitive cases')
   else:
    cases=[x for x in r['stdout'].splitlines() if x.startswith('# real item damage family=grid ')]
    require(len(cases)==40 and all(re.search(r'visible=[1-9][0-9]* residual=0$',x) for x in cases),'real window residuals')
 red=json.loads((e/'expected-red.json').read_text())
 require(red['result']=='FAIL' and red['returncode']==-6 and 'variant=1 visible=4029 residual=154' in red['stdout'],'real expected red')
 for p,v in red['sources'].items():require(sha((e/v['snapshot']).read_bytes())==v['sha256'],'red source')
 for name in ['item-geometry-grid-shown.png','item-geometry-grid-hidden.png']:
  b=(e/name).read_bytes();require(b[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',b[16:24])==(1080,1920),'screenshot')
 return dict(status='PASS',normal_cases=4,sanitizer_cases=4,primitive_scenes_per_run=120,window_scenes_per_run=120,parent_closed=False)
if __name__=='__main__':print(json.dumps(check(),sort_keys=True))
