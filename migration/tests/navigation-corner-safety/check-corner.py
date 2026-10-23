#!/usr/bin/env python3
import hashlib,json,pathlib,re,struct,subprocess
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def require(v,m):
 if not v:raise ValueError(m)
def check(root=R,e=E):
 m=json.loads((e/'manifest.json').read_text())
 require(m['task']=='25.009/corner-center-safety' and m['parent_closed'] is False and m['original_work_rows_closed']==0,'scope')
 require(m['old_runtime']=='USER-SKIPPED' and m['oncanvas'],'runtime/oncanvas')
 require(m['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True),'portrait')
 require(m['build_policy']==dict(optimization='2',debug=False,b_ndebug=False,asan=True,ubsan=True,lsan=False,dependency_instrumentation=False),'build policy')
 for p,h in m['source_sha256'].items():require(sha((root/p).read_bytes())==h,'source '+p)
 for p,h in m['artifact_sha256'].items():require(sha((e/p).read_bytes())==h,'artifact '+p)
 old=(e/'red-source/app/display/gimpcanvascorner.c').read_bytes();require(old==subprocess.check_output(['git','show',m['baseline']+':app/display/gimpcanvascorner.c'],cwd=R),'immutable old corner')
 current=(root/'app/display/gimpcanvascorner.c').read_text();restored=current
 for new,previous in m['center_only_replacements']:
  require(restored.count(new)==1,'single center-only block');restored=restored.replace(new,previous)
 require(restored==old.decode(),'all directional bytes unchanged')
 for kind in ['normal','sanitizer']:
  for group in ['default','directional','window-0','window-1','window-2']:
   r=json.loads((e/(kind+'-'+group+'.json')).read_text())
   require(r['result']=='OK' and r['returncode']==0 and r['duration']<60 and r['test_budget_seconds']==60,'result/budget')
   require(r['source_sha256']==m['source_sha256'] and r['xvfb']==m['xvfb'],'tested source/display')
   lines=[x for x in r['stdout'].splitlines() if x.startswith('ok ') and '# SKIP' not in x]
   require(r['cases']==1 and len(lines)==1 and lines[0].endswith('/corner-safety/'+group),'TAP')
   if group=='directional':require('# directional corner bounds scenes=192' in r['stdout'],'192 directional scenes')
   if group.startswith('window'):
    cases=[x for x in r['stdout'].splitlines() if x.startswith('# real corner transition ')]
    require(len(cases)==64 and all(re.search(r'visible=[1-9][0-9]* residual=0$',x) for x in cases),'real transitions')
 red=json.loads((e/'expected-red.json').read_text());require(red['result']=='FAIL' and red['returncode']==-6 and "'gimp_canvas_item_get_extents(item)' should be NULL" in red['stdout'],'default red')
 for p,h in red['sources'].items():require(sha((e/'red-source'/p).read_bytes())==h,'red source')
 contract=json.loads((e/'corner-contract.json').read_text());require(len(contract['sources'])==8,'caller audit')
 for a in contract['sources']:
  b=(e/'corner-contract-source'/a['snapshot']).read_bytes();require(sha(b)==a['sha256'] and b==subprocess.check_output(['git','show',a['revision']+':'+a['path']],cwd=R),'pinned callers')
 for name in ['item-geometry-corner-shown.png','item-geometry-corner-hidden.png']:
  b=(e/name).read_bytes();require(b[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',b[16:24])==(1080,1920),'screenshot')
 return dict(status='PASS',normal_cases=5,sanitizer_cases=5,directional_scenes=192,window_scenes=192,parent_closed=False)
if __name__=='__main__':print(json.dumps(check(),sort_keys=True))
