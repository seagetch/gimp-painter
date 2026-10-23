#!/usr/bin/env python3
"""Source-bound old applied navigation evidence; no old-runtime claim."""
import hashlib,json,pathlib,struct,subprocess
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def require(v,m):
 if not v:raise ValueError(m)
def check(root=R,e=E):
 m=json.loads((e/'manifest.json').read_text())
 require(m['task']=='25.009/drag-zoom-scroll','task')
 require(m['criterion']=='Shift回転・Ctrl scaling・通常scrollの開始/drag/終了、zoom距離300、device→image変換と原点を旧操作fixtureで比較する','unchanged criterion')
 require(m['dependencies']==['25.008','01.017'],'dependencies')
 require(m['scope']['old_runtime']=='USER-SKIPPED' and not m['scope']['physical_device_parity'] and not m['scope']['parent25.009_closed'],'scope')
 require(m['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True) and m['oncanvas'] is True,'portrait oncanvas')
 require(m['build_policy']==dict(optimization='2',debug=False,b_ndebug=False,asan=True,ubsan=True,lsan=False,dependency_instrumentation=False),'build policy')
 for p,h in m['source_sha256'].items():require(sha((root/p).read_bytes())==h,'source '+p)
 for p,h in m['artifact_sha256'].items():require(sha((e/p).read_bytes())==h,'artifact '+p)
 for kind in ['normal','sanitizer']:
  for group,count in [('original',35),('rotation',2),('zoom',10)]:
   n=kind+'-'+group+'.json';r=json.loads((e/n).read_text())
   require(set(r)==set(['name','result','duration','returncode','stdout','stderr','source_sha256','cases','test_budget_seconds','xvfb']),'receipt fields')
   require(r['result']=='OK' and r['returncode']==0 and r['duration']<60 and r['test_budget_seconds']==60,'outcome/budget '+n)
   require(r['source_sha256']==m['source_sha256'],'same tested source '+n)
   lines=[x for x in r['stdout'].splitlines() if x.startswith('ok ') and '# SKIP' not in x]
   require(len(lines)==count and r['cases']==count,'cases '+n)
   if group=='original':require(r['stdout'].count('# old pan ')==5760,'old pan pairs')
   if group=='rotation':require(r['stdout'].count('# old applied rotation ')==1008,'old rotation pairs')
   if group=='zoom':
    for case in ['origin','limits','oncanvas-render','extreme-pointer','invalidation','invalid-inputs','boundary-pattern','mixed-chain','focus-fallback','resize-preference']:
     require(any(x.endswith('/old-zoom/'+case) for x in lines),'case '+case)
    require(r['stdout'].count('# layer stipple original-path pixels ')==48,'ordinary stipple pixels')
    require('requested=522.5333333333333 applied=256' in r['stdout'],'extreme requested/applied')
 red=json.loads((e/'expected-red.json').read_text());require(red['result']=='FAIL' and red['returncode']==-6,'expected red')
 require('expected=395.58595612869237,214.16955646855047 actual=396,218' in red['stdout'],'pre-fix origin mismatch')
 rm=json.loads((e/'red-source-manifest.json').read_text());require(m['previous_checkpoint'].startswith(rm['production_checkpoint']),'red checkpoint')
 for p,h in rm['sha256'].items():
  b=(e/'red-source'/p).read_bytes();require(sha(b)==h,'red source '+p)
  if p=='app/display/gimpdisplayshell-scale.c':require(b==subprocess.check_output(['git','show',m['previous_checkpoint']+':'+p],cwd=R),'red production immutable')
 failure=json.loads((e/'extreme-pointer-render-failure.json').read_text());require(failure['result']=='FAIL' and 'out of memory' in failure.get('stderr',''),'retained OOM')
 for name in ['portrait-oncanvas-zoom-m0.png','portrait-oncanvas-zoom-m1.png']:
  b=(e/name).read_bytes();require(b[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',b[16:24])==(1080,1920),'portrait image')
 for r in json.loads((e/'headers.json').read_text()):require(r['result']=='OK' and r['returncode']==0,'header test')
 require(len(json.loads((e/'headers.json').read_text()))==2,'two header tests')
 probes=json.loads((e/'header-probes.json').read_text())['probes']
 for header in ['app/display/gimpdisplayshell.h','app/display/gimpdisplayshell-scroll.h','app/display/gimpdisplayshell-scale.h']:
  for language in ['c','cpp']:require(any(p['header']==header and p['language']==language for p in probes),'compiled navigation header '+header+' '+language)
 inv='migration/inventory/additional-c-header-routes.json';built=json.loads((e/'build-header-routes.json').read_text());current=json.loads((root/inv).read_text())
 base=json.loads(subprocess.check_output(['git','show',m['base_commit']+':'+inv],cwd=R))
 for path in ['app/display/gimpdisplayshell-scroll.h','app/display/gimpdisplayshell-scale.h']:
  route=next(x for x in current['routes'] if x['path']==path);require(route==next(x for x in built['routes'] if x['path']==path),'current navigation route');base['routes'].append(route)
 require(base==built,'isolated header route closure')
 require((root/'app/display/gimpcanvasrectangle.c').read_bytes()==subprocess.check_output(['git','show',m['previous_checkpoint']+':app/display/gimpcanvasrectangle.c'],cwd=R),'discarded clipping reverted')
 return dict(status='PASS',normal_cases=47,sanitizer_cases=47,task=m['task'],parent_task_closed=False)
if __name__=='__main__':print(json.dumps(check(),sort_keys=True))
