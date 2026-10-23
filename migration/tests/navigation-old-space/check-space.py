#!/usr/bin/env python3
import csv,hashlib,importlib.util,io,json,pathlib,re,struct,subprocess
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def require(v,m):
 if not v:raise ValueError(m)
def check(root=R,e=E):
 m=json.loads((e/'manifest.json').read_text());require(m['task']=='25.009/drag-zoom-scroll' and m['scope']['old_runtime']=='USER-SKIPPED' and m['scope']['parent25.009_closed'] is False,'scope')
 require(m['original_criterion']=='Shift回転・Ctrl scaling・通常scrollの開始/drag/終了、zoom距離300、device→image変換と原点を旧操作fixtureで比較する','criterion')
 require(m['dependencies']==['25.008','01.017'],'dependencies')
 require(m['build_policy']==dict(optimization='2',debug=False,b_ndebug=False,asan=True,ubsan=True,lsan=False,dependency_instrumentation=False),'build policy')
 require(m['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True) and m['oncanvas'],'portrait')
 for p,h in m['source_sha256'].items():require(sha((root/p).read_bytes())==h,'source '+p)
 for p,h in m['artifact_sha256'].items():require(sha((e/p).read_bytes())==h,'artifact '+p)
 for kind in ['normal','sanitizer']:
  for group,count in [('original',35),('rotation',2),('zoom',10),('space',3)]:
   r=json.loads((e/(kind+'-'+group+'.json')).read_text())
   require(set(r)==set(['name','result','duration','returncode','stdout','stderr','source_sha256','cases','test_budget_seconds','xvfb']),'receipt fields')
   require(r['source_sha256']==m['source_sha256'],'tested source')
   require(r['result']=='OK' and r['returncode']==0 and r['duration']<60 and r['test_budget_seconds']==60,'result/budget')
   lines=[x for x in r['stdout'].splitlines() if x.startswith('ok ') and '# SKIP' not in x]
   require(r['cases']==count and len(lines)==count,'TAP count')
   if group=='space':
    require(r['stdout'].count('# old applied zoom scene=20')==1800,'Space point pairs')
    for name in ['origin','oncanvas-render','deferred-release']:require(any(x.endswith('/old-space/'+name) for x in lines),'Space case '+name)
   if group=='original':require(r['stdout'].count('# old pan ')==5760,'retained middle pan')
   if group=='rotation':require(r['stdout'].count('# old applied rotation ')==1008,'retained rotation')
   if group=='zoom':require('requested=522.5333333333333 applied=256' in r['stdout'],'retained extreme zoom')
 red=json.loads((e/'expected-red.json').read_text());require(red['result']=='FAIL' and red['returncode']==-6 and 'expected=413.60000000000002,206.39999999999998 actual=413,206' in red['stdout'],'expected red')
 rm=json.loads((e/'red-source.json').read_text())
 for p,h in rm['sha256'].items():
  b=(e/'red-source'/p).read_bytes();require(sha(b)==h,'red source')
  if p=='app/display/gimpdisplayshell-tool-events.c':require(b==subprocess.check_output(['git','show',m['previous_checkpoint']+':'+p],cwd=R),'red immutable production')
 audit=json.loads((e/'work-item-audit.json').read_text());require(len(audit['rows'])==18,'18 obligations')
 baseline=list(csv.DictReader(io.StringIO(subprocess.check_output(['git','show',m['previous_checkpoint']+':migration/inventory/legacy-port-work-items.tsv'],cwd=R,text=True)),delimiter='\t'))
 expected={r['work_id']:r for r in baseline if r['wbs_task']==m['task']};require(set(expected)=={r['work_id'] for r in audit['rows']},'exact original IDs')
 raw=subprocess.check_output(['git','show',audit['source_revision']+':app/display/gimpdisplayshell-tool-events.c'],cwd=R).decode().splitlines(True)
 for a in audit['rows']:
  old=expected[a['work_id']]
  for k in ['source_id','source_blob','source_range','source_sha256']:require(a[k]==old[k],'pinned work attribution')
  require(a['original_action']==old['action'] and a['original_acceptance']==old['acceptance'],'original work criterion')
  match=re.search(r'-> (\d+)(?:,(\d+))?',a['source_range']);start=int(match[1]);count=int(match[2] or 1)
  require(sha(''.join(raw[start-1:start-1+count]).encode())==a['pinned_excerpt_sha256'],'pinned hunk excerpt')
  others=[{'work_id':r['work_id'],'task':r['wbs_task'],'status':r['status']} for r in baseline if r['source_id']==a['source_id'] and r['work_id']!=a['work_id']]
  require(a['other_whole_hunk_owners']==others,'other duties retained in historical audit')
  require(a['status']=='PROVEN_CURRENT_SCOPE','work scope accepted')
 for name in ['portrait-oncanvas-space-m0.png','portrait-oncanvas-space-m1.png']:
  b=(e/name).read_bytes();require(b[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',b[16:24])==(1080,1920),'portrait screenshot')
 return dict(status='PASS',normal_cases=50,sanitizer_cases=50,original_work_items=18,parent_task_closed=False)
if __name__=='__main__':print(json.dumps(check(),sort_keys=True))
