#!/usr/bin/env python3
"""Retain all36 original Undo duties; pixel proof is not engine/stroke replay."""
import hashlib,json,pathlib,subprocess
from check_drawable_buffer_read_resolution import load,require
ROOT=pathlib.Path(__file__).resolve().parents[1];MANIFEST=ROOT/'migration/contracts/undo-payload.json'
ANCESTORS=['drawable-buffer-read-resolution','drawable-buffer-write','tempbuf-conversion','selection-mask','layer-mask','stroke-start-snapshot','work-buffer','dirty-region','undo-difference','commit-cancel','projection-notify','precision-refusal','small-boundaries']
def validate(m,args=None):
 _,inventory,tasks,progress=load()if args is None else args
 require(m['task']=='09.017' and m['criterion_status']=='DONE','Wrong criterion')
 require(m['original_action']=='buffer 基盤の Undo 試験を作る' and m['original_acceptance']=='描画前後の差分と復元が一致','Original criterion changed')
 require(m['dependencies']==['09.016'] and progress['09.016']['status']=='DONE','Dependency incomplete')
 require(tasks['09.017']==['[x]',m['original_action'],'09.016',m['original_acceptance']] and progress['09.017']['status']=='DONE','Ledger mismatch')
 require(m['legacy_commit']=='afa43fae3e920210146abed514f136fd49f671b5' and m['whole_hunk_checkpoint_status']=='TODO','Unproven historical completion')
 require(m['ancestor_contracts']==ANCESTORS,'Lost ancestry')
 expected={k:r for k,r in inventory.items()if r['wbs_task']=='09.017'};require(len(m['duties'])==36 and set(expected)=={d['work_id']for d in m['duties']},'Missing original source duty')
 ancestors=[]
 for name in ANCESTORS:
  a=json.loads((ROOT/f'migration/contracts/{name}.json').read_text())
  for q in a.get('duties',[])+a.get('inherited_duties',[]):ancestors.append((name,a['task'],q))
 blobs={}
 for d in m['duties']:
  orig=expected[d['work_id']]
  for key in ['source_id','path','source_blob','source_range','source_sha256','source_scope']:require(d[key]==orig[key],'Changed identity: '+key)
  require(d['original_action']==orig['action'] and d['original_acceptance']==orig['acceptance'],'Weakened original source criterion')
  if d['path']not in blobs:blobs[d['path']]=hashlib.sha256(subprocess.check_output(['git','show',m['legacy_commit']+':'+d['path']],cwd=ROOT)).hexdigest()
  require(d['source_blob_sha256']==blobs[d['path']],'Changed pinned source')
  relations=[];successors={}
  for name,task,q in ancestors:
   if all(q.get(k)==d[k]for k in ['source_id','path','source_range','source_sha256']):
    relations.append(dict(contract=name,criterion=task,work_id=q['work_id'],successor_obligations=q.get('successor_obligations',{})))
    for k,v in q.get('successor_obligations',{}).items():successors.setdefault(k,[]).append(v)
  owner='09.011/mypaint-undo-stroke' if d['path'].startswith(('app/core/core-enums','app/core/gimpimage-undo','app/core/gimpitem','app/paint/gimpmypaintcore.','app/paint/gimpmypaintcoreundo.','app/tools/gimppainttool.')) else '31.017'
  text='Original source-specific Undo verification remains required: '+orig['action']+'; '+orig['acceptance']+'; '+orig['path']+' '+orig['source_scope']+' '+orig['source_range']+'. Native generic pixel history alone does not implement or verify this original engine/stroke/hook/iterator route.'
  successors.setdefault(owner,[]).append(text);successors.pop('09.017',None);successors={k:' '.join(dict.fromkeys(v))for k,v in successors.items()}
  require(d['related_obligations']==relations and d['successor_obligations']==successors,'Lost inherited source obligation')
  require(d['source_specific_undo_owner']==owner and d['source_specific_undo_duty']==text,'Lost exact source-specific boundary criterion')
  require(d['whole_hunk_checkpoint_status']=='TODO' and 'Original engine/stroke state and native hooks, dispatch/iterator consumers remain explicitly owned' in d['native_checkpoint_scope'],'Unproven iterator/search parity')
  for k in successors:
   require(k in tasks and k in progress and 'undo-payload.json#'+k in progress[k]['commit_or_artifact'],'Missing explicit reverse owner')
   if orig['status']=='DONE':require(progress[k]['status']=='DONE' and tasks[k][0]=='[x]' and progress[k]['test']!='WBS/progress ID reconciliation','Premature source closure')
 require(m['evidence']==['migration/tests/undo-payload/report.json'] and all((ROOT/p).is_file()for p in m['evidence']),'Missing runtime evidence')
 return 36
if __name__=='__main__':print(f'{validate(json.loads(MANIFEST.read_text()))} original source-specific duties preserved; bounded native Undo checkpoint verified')
