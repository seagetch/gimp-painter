#!/usr/bin/env python3
"""Check exact stroke-start-snapshot acceptance and preserve the entire legacy feature duty."""
import hashlib,json,pathlib,subprocess
from check_drawable_buffer_read_resolution import load,require
ROOT=pathlib.Path(__file__).resolve().parents[1];MANIFEST=ROOT/'migration/contracts/stroke-start-snapshot.json'
def validate(m,args=None):
 _,inventory,tasks,progress=load() if args is None else args
 require(m['task']=='09.008' and m['criterion_status']=='DONE','Wrong criterion')
 require(m['original_action']=='ストローク開始 snapshot を実装する' and m['original_acceptance']=='開始前画素を後続変更から分離','Original criterion changed')
 require(m['dependencies']==['09.007'] and progress['09.007']['status']=='DONE','Layer-mask dependency incomplete')
 require(tasks['09.008']==['[x]',m['original_action'],'09.007',m['original_acceptance']] and progress['09.008']['status']=='DONE','Ledger mismatch')
 require(m['whole_hunk_checkpoint_status']=='TODO','Whole-header completion not proven')
 expected={k:r for k,r in inventory.items() if r['wbs_task']=='09.008'}
 require(len(m['duties'])==1 and set(expected)=={r['work_id'] for r in m['duties']},'Missing source duty')
 d=m['duties'][0];orig=expected[d['work_id']]
 for key in ('source_id','path','source_blob','source_range','source_sha256','source_scope'):require(d[key]==orig[key],'Changed source identity: '+key)
 require(m['legacy_commit']=='afa43fae3e920210146abed514f136fd49f671b5','Wrong legacy revision')
 data=subprocess.check_output(['git','show',m['legacy_commit']+':'+d['path']],cwd=ROOT)
 require(hashlib.sha256(data).hexdigest()==d['source_blob_sha256'],'Changed pinned source')
 relations=[];required_owners=set()
 for name in ('drawable-buffer-read-resolution','drawable-buffer-write','selection-mask','layer-mask'):
  a=json.loads((ROOT/('migration/contracts/'+name+'.json')).read_text());r=next(r for r in a['duties'] if r['path']==d['path'])
  relations.append(dict(criterion=a['task'],work_id=r['work_id'],inherited_09_008_obligation=r['successor_obligations']['09.008']))
  required_owners.update(r['successor_obligations'])
 required_owners.discard('09.008');required_owners.update(('09.013','21.010','21.011','31.017'))
 require(d['related_obligations']==relations,'Lost inherited obligation')
 require(required_owners<=set(d['successor_obligations']) and '09.008' not in d['successor_obligations'],'Lost owner or self dependency')
 require(d['whole_hunk_checkpoint_status']=='TODO' and d['historical_source_duty'].strip(),'Premature checkpoint completion')
 for owner,text in d['successor_obligations'].items():
  require(owner in tasks and owner in progress and text.strip(),'Unknown owner or missing duty')
  p=progress[owner];require('stroke-start-snapshot.json#'+owner in p['commit_or_artifact'],'Missing reverse owner link')
  if orig['status']=='DONE':
   require(tasks[owner][0]=='[x]' and p['status']=='DONE','Premature whole-header completion')
   require(p['test']!='WBS/progress ID reconciliation' and p['result']!='Existing unchecked task recorded explicitly as TODO','Placeholder completion evidence')
   require(any(x.strip() not in ('','-','tasks.md') and '/contracts/' not in x for x in p['commit_or_artifact'].split(';')),'Cross-link alone is not completion evidence')
 require(all((ROOT/p).is_file() for p in m['evidence']),'Missing runtime evidence')
 return 1
if __name__=='__main__':print(f'{validate(json.loads(MANIFEST.read_text()))} unchanged whole-header duty; native stroke-start snapshot isolation criterion verified')
